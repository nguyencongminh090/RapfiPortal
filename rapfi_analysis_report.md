# Rapfi Engine — Analyst Report

> **Project:** Rapfi — Gomoku/Renju AI Engine  
> **Language:** C++17  
> **Protocol:** Gomocup / Piskvork  
> **License:** GNU GPL v3  
> **Build System:** CMake + Ninja  

---

## 1. Project Overview

Rapfi là một **chess engine chuyên biệt cho Gomoku và Renju**, tuân thủ giao thức **Piskvork/Gomocup**.  
Engine hỗ trợ nhiều chế độ vận hành: chơi cờ, benchmark, tự chơi (selfplay), training data prep, và quản lý database.

---

## 2. Directory Structure

```
rapfi-master/
├── src/
│   ├── main.cpp              # Entry point
│   ├── config.h / config.cpp # Global configuration & model tables
│   ├── internalConfig.cpp    # Built-in default config (TOML)
│   ├── core/                 # Primitive types & platform utilities
│   ├── game/                 # Board, Pattern, Move generation
│   ├── search/               # Search algorithms (AB + MCTS)
│   │   ├── ab/               # Alpha-Beta search
│   │   └── mcts/             # Monte Carlo Tree Search
│   ├── eval/                 # Neural network evaluators (NNUE)
│   ├── command/              # Protocol & CLI command handlers
│   ├── database/             # Position database (YXdb)
│   ├── tuning/               # Training data & SPSA tuner
│   └── external/             # 3rd party dependencies
├── build/                    # CMake build artifacts
└── Executable/               # Output binaries (empty initially)
```

---

## 3. Module Breakdown

### 3.1 `core/` — Foundation Layer

| File | Responsibility |
|------|---------------|
| `types.h` | Core enums: `Color`, `Pattern`, `Pattern4`, `Value`, `Rule`, `Bound` |
| `pos.h` | `Pos` struct (board coordinate), `Direction` enum, transforms |
| `hash.h/cpp` | Zobrist hash |
| `platform.h/cpp` | SIMD, NUMA, threading primitives |
| `iohelper.h/cpp` | I/O utilities, logging macros |
| `utils.h/cpp` | Math helpers (`combineNumber`, `power`, etc.) |

**Key types:**
```cpp
typedef uint16_t PatternCode;   // Combined code of 4 line patterns
typedef int16_t  Score;         // Cell-level heuristic score
typedef int16_t  Eval;          // Classical eval score
typedef float    Depth;         // Search depth (float for fractional ply)
typedef uint64_t HashKey;       // Zobrist hash key
```

---

### 3.2 `game/` — Board & Pattern Engine

#### Board Representation

```
FULL_BOARD_SIZE = 32  (includes 5-cell boundary on each side)
MAX_BOARD_SIZE  = 22  (actual playable area)
Encoding: 2 bits per cell in uint64_t bitKey[] arrays
  - 00: empty  01: black  10: white  11: wall
```

The `Board` class is the central state container:
- `cells[FULL_BOARD_CELL_COUNT]` — per-cell `Cell` struct
- `bitKey0..3[N]` — 4-direction bitkeys for pattern lookup
- `stateInfos[]` — incremental `StateInfo` per ply (for fast undo)
- Template-parameterized `move<Rule, MoveType>()` / `undo<Rule, MoveType>()`

**`Cell` struct** stores:
- `piece` (Color), `cand` (candidate flag)
- `pattern4[SIDE_NB]` — combined 4-direction threat pattern
- `score[SIDE_NB]` — heuristic move score
- `pattern2x[4]` — compressed 2-color pattern per direction

#### Pattern Recognition System

```
Pattern  (14 types) — single line at one cell
  DEAD → OL → B1/F1 → B2/F2/F2A/F2B → B3/F3/F3S → B4/F4 → F5

Pattern4 (14 levels) — combined 4-direction threat at one cell
  NONE → FORBID → L_FLEX2 → ... → B_FLEX4 → A_FIVE
```

Pattern lookup uses **precomputed tables** (`PatternConfig`):
- `PATTERN2x[KeyCnt]` — maps bitkey → `Pattern2x` (simultaneous B/W lookup)
- `PCODE[P][P][P][P]` — 4D table mapping 4 patterns → `PatternCode`
- `P4SCORES[rule][pcode]` — maps pcode → `Pattern4Score` (pattern4 + scores)
- **BMI2 PEXT** instruction used for fast key extraction when available

#### Move Generation (`movegen.h/cpp`)

Candidate-based generation — only cells within `CandArea` (expanding rectangle) and flagged as candidates are considered. Supports 6 candidate range modes (`SQUARE2` to `FULL_BOARD`).

---

### 3.3 `search/` — Search Algorithms

#### Architecture

```
Searcher (abstract base)
├── ABSearcher   (Alpha-Beta, src/search/ab/)
└── MCTSSearcher (Monte Carlo, src/search/mcts/)

ThreadPool → vector<SearchThread>
           └── MainSearchThread (thread 0)
```

Each `SearchThread` owns:
- A cloned `Board`
- An `Evaluator` instance
- A `DBClient` for database queries
- `RootMoves` list + custom `SearchData`

#### Alpha-Beta Searcher (`search/ab/`)

**Iterative deepening** with **Lazy SMP** (parallel threads at different depths).

Key search techniques implemented:
| Technique | Location |
|-----------|----------|
| Aspiration Windows | `aspirationSearch()` |
| Transposition Table | `hashtable.h/cpp` |
| Move Ordering | `movepick.h/cpp` (killer, history, policy prior) |
| Null Move Pruning | `search.cpp` step 8 |
| Futility Pruning | `parameter.h` — `futilityMargin()` |
| Razoring | `parameter.h` — `razorMargin()` |
| Singular Extension | `parameter.h` — `singularMargin()` |
| LMR (Late Move Reduction) | `parameter.h` — `reduction()` |
| Policy-guided reduction | `parameter.h` — `policyReduction()` |
| VCF/VCF-defend search | `vcfsearch()` / `vcfdefend()` |
| Database cutoffs | Step 5 in `search()` |

**VCF (Victory by Continuous Forcing)** — a specialized quiescence search that only explores forcing moves (B4, F4, F5 threats) when depth ≤ 0.

**Rule-templated search** — `template<Rule R, NodeType NT>` — compile-time specialization for `FREESTYLE`, `STANDARD`, `RENJU` rules, allowing per-rule tuning without runtime branching.

**Strength levels** — `SkillMovePicker` randomly selects sub-optimal moves to allow handicap play.

**Balance mode** — forces the engine to find moves that lead to equal positions (used for opening generation).

#### MCTS Searcher (`search/mcts/`)

Files: `node.h/cpp`, `nodetable.h`, `search.cpp`, `searcher.h`

- Tree of `MCTSNode` objects stored in `NodeTable`
- Standard UCT selection with NNUE policy priors
- Parallel tree search supported

---

### 3.4 `eval/` — Neural Network Evaluation

#### Evaluator Hierarchy

```
Evaluator (abstract base class)
├── Mix10NNUEEvaluator   — "Mix 10" architecture NNUE
├── Mix9SVQNNUEEvaluator — "Mix 9" with scalar vector quantization
└── ONNXEvaluator        — Generic ONNX runtime evaluator
```

**Evaluator interface:**
```cpp
virtual ValueType evaluateValue(const Board &, AccLevel) = 0;
virtual void evaluatePolicy(const Board &, PolicyBuffer &, AccLevel) = 0;
```

`ValueType` contains: `value` (int16), `winProb`, `lossProb`, `drawProb` (float).

`AccLevel` (0=BEST to 3=LOW) — controls speed/accuracy tradeoff.

`PolicyBuffer` — flat float array indexed by `Pos`, scaled by factor 32.

`simdops.h` — massive SIMD operation library (SSE/AVX2/AVX512/NEON) for NNUE inference.

`weightloader.h` — handles LZ4-compressed weight file loading.

Classical evaluation fallback via `EVALS[rule][pcode]` and `EVALS_THREAT` tables in `Config`.

---

### 3.5 `config.h/cpp` — Global Configuration

Central namespace `Config::` holding:
- **Model tables:** `EVALS[RULE_NB+1][PCODE_NB]`, `P4SCORES[...][...]`, `EVALS_THREAT[...]`
- **General options:** threads, message mode, candidate range, TT size
- **Search parameters:** aspiration window, max depth, MultiPV settings
- **Time management:** match space, falloff factors, time divisor
- **Database options:** cache size, query depth, overwrite rules

Loaded from TOML-format config files at startup via `Config::loadConfig()`.

---

### 3.6 `command/` — Protocol & CLI Interface

| File | Function |
|------|----------|
| `gomocup.cpp` | Main Gomocup/Piskvork protocol loop (51 KB — largest command file) |
| `command.h/cpp` | Entry dispatch, `configPath`, `loadConfig()` |
| `benchmark.cpp` | Performance benchmarking |
| `selfplay.cpp` | Automated game generation |
| `tuning.cpp` | SPSA parameter tuning interface |
| `dataprep.cpp` | Training data preprocessing |
| `database.cpp` | Database management CLI |
| `opengen.cpp` | Opening book generation |

**Run modes** (CLI `--mode` argument):
`gomocup` | `bench` | `opengen` | `tuning` | `selfplay` | `dataprep` | `database`

---

### 3.7 `database/` — Position Database

| File | Responsibility |
|------|---------------|
| `dbtypes.h/cpp` | Record types: `DBRecord`, `DBLabel`, bounds, values |
| `dbstorage.h` | Abstract storage interface |
| `yxdbstorage.h/cpp` | YXQ format database storage implementation |
| `dbclient.h/cpp` | High-level query/write client with caching |
| `dbutils.h/cpp` | Import/export utilities (YXQ, lib format) |
| `cache.h` | LRU cache template |

---

### 3.8 `tuning/` — Training Pipeline

| File | Responsibility |
|------|---------------|
| `dataset.h/cpp` | Dataset loading and sampling |
| `dataentry.h` | Single training sample format |
| `datawriter.h/cpp` | Binary data serialization |
| `tuner.h/cpp` | SPSA gradient-free optimizer |
| `tunemap.h` | Parameter mapping for tuning |
| `optimizer.h/cpp` | Optimizer interface |

---

## 4. Data Flow Diagram

```
[Gomocup Protocol Input]
        │
        ▼
[Command::gomocupLoop()]
        │  parses: START, TURN, BEGIN, BOARD, etc.
        ▼
[ThreadPool::startThinking(board, options)]
        │
        ├── [Opening::probeOpening()]  ─── book hit? → return immediately
        │
        ├── [DBClient::queryChildren()]  ─── forced DB move? → return
        │
        └── [ABSearcher::searchMain()]
                │
                ├── TimeControl::init()
                ├── TT::incGeneration()
                └── iterativeDeepingLoop()
                        │
                        ├── aspirationSearch()
                        │       └── search<Rule, NT>()
                        │               ├── TT probe
                        │               ├── DB query
                        │               ├── Static eval (Eval::evaluate)
                        │               ├── Pruning (null, futility, razor)
                        │               ├── MovePicker loop
                        │               │   └── search<Rule, NT>() [recursive]
                        │               └── TT store
                        │
                        └── [bestMove] → output via printer
```

---

## 5. Key Design Patterns

| Pattern | Implementation |
|---------|---------------|
| **Template Policy** | `Board::move<Rule, MoveType>()`, `search<Rule, NT>()` — compile-time rule specialization |
| **Abstract Factory** | `Config::createSearcher()`, `Config::createDefaultDBStorage()` |
| **Strategy (Plugin)** | `Evaluator` base class — swappable NNUE backends |
| **Observer** | `Evaluator` hooks: `beforeMove/afterMove/beforeUndo/afterUndo` |
| **Thread Pool** | `ThreadPool` + `SearchThread` with Lazy SMP |
| **Incremental Update** | `StateInfo` stack, `UpdateCache`, bitkey arrays |
| **Command Pattern** | Run modes dispatched from `main()` |

---

## 6. Highlighted Technical Points

1. **Bitboard with 2-bit per cell encoding** — 64-bit `bitKey` arrays store color info for all 4 line directions simultaneously.

2. **Precomputed pattern tables** — `PATTERN2x[65536]` (Freestyle) and larger Standard/Renju tables map bit patterns → line patterns in O(1).

3. **Pattern4Score packing** — a 32-bit struct packs `scoreSelf (14 bits)` + `scoreOppo (14 bits)` + `pattern4 (4 bits)` — zero memory overhead for cell score computation.

4. **SIMD-accelerated NNUE** — `simdops.h` (106 KB) provides AVX2/AVX512/NEON implementations for the neural network inference.

5. **VCF search** — dedicated quiescence for forcing move sequences (B4/F4/F5 chains), critical for Gomoku correctness.

6. **Lazy SMP** — worker threads search at slightly different depths; main thread picks the best result by vote weighting depth × value.

7. **Database integration** — search directly queries a position database mid-tree to retrieve pre-computed results, enabling strong TB-style play.

---

## 7. Summary Table

| Metric | Value |
|--------|-------|
| Source files | ~65 files |
| Largest file | `eval/simdops.h` (106 KB), `search/ab/search.cpp` (73 KB), `command/gomocup.cpp` (51 KB) |
| Search algorithms | Alpha-Beta (primary) + MCTS (secondary) |
| Game rules | Freestyle, Standard, Renju |
| Evaluators | Mix10NNUE, Mix9SVQNNUE, ONNX |
| Max board size | 22×22 |
| Threading model | Lazy SMP (multi-thread AB) |
| Build system | CMake + Ninja |
| Protocol | Gomocup/Piskvork |
