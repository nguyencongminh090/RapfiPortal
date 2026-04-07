# Deep Dive: `config.toml` Loading & `binary_file`

---

## 1. Config Loading Flow

```
main()
  └── Command::loadConfig()           [command/command.cpp:101]
        │
        ├─ 1. Is configPath absolute?  → use it directly
        ├─ 2. Exists in CWD?          → resolve from CWD
        ├─ 3. Exists in binary dir?   → resolve from binary dir
        └─ 4. Not found + allowInternalConfig=true
                  └── use Config::InternalConfig  (built-in TOML string in internalConfig.cpp)
        │
        └── Config::loadConfig(stream)          [config.cpp:260]
              │
              ├── parse TOML via cpptoml
              │
              ├── readRequirement()   → version check [min_version / max_version]
              ├── readGeneral()       → threads, message_mode, candidate_range, TT size...
              ├── readSearch()        → AB/MCTS params, time control...
              ├── readDatabase()      → YixinDB setup...
              └── readModel()         → ← THE MAIN TOPIC
```

### Config file resolution priority:
| Priority | Location |
|----------|----------|
| 1 | Absolute path (if specified with `--config`) |
| 2 | Working directory (`./config.toml`) |
| 3 | Binary executable directory (`<rapfi_dir>/config.toml`) |
| 4 | Built-in internal config (hardcoded in `internalConfig.cpp`) |

---

## 2. The `[model]` Section — Full Structure

```toml
[model]
binary_file = "path/to/model.bin"   # ← shortcut: load all classical tables from binary
scaling_factor = 200.0              # sigmoid scaling: win_rate = sigmoid(eval / scale)

# If binary_file is NOT specified, tables are read inline:
[model.eval]
  model_type = 2        # 0=raw, 1=table1, 2=table2
  table2 = [...]        # upper-triangular pair values

[model.eval.renju]      # override for renju rule (asymmetric)
  [model.eval.renju.black]
  [model.eval.renju.white]

[model.score]
  [model.score.self]
    model_type = 1
    table1 = [...]
  [model.score.oppo]
    ...

[model.evaluator]       # optional NNUE evaluator (separate from classical)
  type = "mix10"        # or "mix9svq", "ort"
  [[model.evaluator.weights]]
    weight_file = "path/to/nnue_weights.bin"
```

> **Key logic** (`config.cpp:468`):
> ```cpp
> std::string modelPath = t.get_as<std::string>("binary_file").value_or("");
> if (!modelPath.empty()) {
>     Command::loadModelFromFile(modelPath);   // binary path → load EVALS/P4SCORES
> }
> else {
>     // read [model.eval] and [model.score] inline from TOML
> }
> ```
> `binary_file` and inline `[model.eval]`/`[model.score]` are **mutually exclusive**. Binary takes priority.

---

## 3. What Exactly Is `binary_file`?

`binary_file` is a **LZ4-compressed binary dump** of the **classical evaluation tables** only.

### Data written by `Config::exportModel()` [config.cpp:987]:

```
┌──────────────────────────────────────────────┐
│ LZ4 compressed stream                         │
│                                              │
│  double  scalingFactor              (8 bytes) │
│  int16_t EVALS[RULE_NB+1][PCODE_NB]          │
│  int16_t EVALS_THREAT[RULE_NB+1][THREAT_NB]  │
│  ─── for each of (RULE_NB+1) rules: ───       │
│  int16_t scores[PCODE_NB][2]                 │
│    (= P4SCORES[][].scoreSelf, scoreOppo)      │
└──────────────────────────────────────────────┘
```

### Exact sizes at compile time:

| Symbol | Description | Size |
|--------|-------------|------|
| `PCODE_NB` | `combineNumber(14, 4)` = **C(14+4-1,4) = 3876** pattern codes | 3876 entries |
| `THREAT_NB` | `2^11 = 2048` threat combinations | 2048 entries |
| `RULE_NB+1` | 4 rule slots (FREESTYLE, STANDARD, RENJU_BLACK, RENJU_WHITE) | 4 |
| `EVALS` | 4 × 3876 × int16_t | ~30 KB |
| `EVALS_THREAT` | 4 × 2048 × int16_t | ~16 KB |
| `P4SCORES` (as scores) | 4 × 3876 × 2 × int16_t | ~60 KB |
| **Total uncompressed** | | **~106 KB** |

### What each table contains:

| Table | Purpose | Indexed by |
|-------|---------|-----------|
| `EVALS[rule][pcode]` | Static eval of a cell for current side | `(rule, PatternCode)` |
| `EVALS_THREAT[rule][threat]` | Additional threat-pattern-based eval | `(rule, ThreatCode)` |
| `P4SCORES[rule][pcode]` | Move ordering heuristic scores (self + oppo) | `(rule, PatternCode)` |

**`PatternCode`** encodes the combined pattern of **all 4 directions** at a cell:
```
PCODE[patDir0][patDir1][patDir2][patDir3]  →  PatternCode (0..3875)
```
where each `pat` is one of 14 `Pattern` values (DEAD, OL, B1, F1, ..., F5).

---

## 4. How Is `binary_file` Created?

### Method A: Tuning pipeline (primary method)

```
rapfi tuning --training-dataset ... --output outdir --name mymodel --epochs 1000
```

Internally:
1. `Tuner` runs gradient descent on `EVALS[]`, `EVALS_THREAT[]`, `P4SCORES[]` using game records.
2. After each checkpoint: `tuner.saveParams()` → writes optimized values back to `Config::EVALS[...]`.
3. Then: `Config::exportModel(ofstream)` → serializes all tables LZ4-compressed → **writes `.bin` file**.

Relevant code (`tuning.cpp:300-303`):
```cpp
tuner.saveParams();
Config::ScalingFactor = stat.scalingFactor;
std::ofstream model(outpath / modelFileName, std::ios::binary);
Config::exportModel(model);   // → binary_file
```

### Method B: Manual construction from TOML

You can write a `config.toml` with `[model.eval]` + `[model.score]` tables inline, load it, then immediately export:
```cpp
Config::loadConfig(tomlStream);   // populate EVALS/P4SCORES from TOML
Config::exportModel(binStream);   // dump to binary_file
```
This allows manually creating a binary from human-readable TOML values (e.g., from `internalConfig.cpp`).

### Method C: Re-export any loaded config

Since `binary_file` is just a snapshot of the in-memory tables after loading, you can also re-export after loading any external config.

---

## 5. Is `binary_file` Classical Only?

**Yes. `binary_file` is exclusively for the classical evaluation.** It contains:
- `EVALS[][]` — classical positional score tables
- `EVALS_THREAT[][]` — threat pattern bonus tables
- `P4SCORES[][]` — move ordering scores

It does **NOT** contain:
- NNUE (neural network) weights → those are separate `.bin` files in `[model.evaluator.weights]`
- MCTS parameters
- Search parameters
- Any `[general]`, `[search]`, `[database]` config

### Two separate evaluation systems:

```
config.toml
├── [model]
│   ├── binary_file = "classic.bin"   ← CLASSICAL eval tables
│   └── [model.evaluator]
│       └── weight_file = "nnue.bin"  ← NNUE neural network weights
```

**They are loaded independently and used together:**
- Classical evaluation is always available as fast fallback.
- NNUE is queried when available; a margin formula switches between them:
  ```cpp
  // eval/eval.cpp pattern — margin decides if NNUE or classical is used
  EvaluatorMarginScale, EvaluatorMarginWinLossScale, EvaluatorMarginWinLossExponent
  ```

---

## 6. What Happens on Rule Change?

The binary_file stores data for **all 4 rule slots simultaneously**:

```
Index 0 = FREESTYLE
Index 1 = STANDARD
Index 2 = RENJU (Black side, due to asymmetry)
Index 3 = RENJU (White side)
```

Rule selection at query time (`config.h:191`):
```cpp
constexpr int tableIndex(Rule r, Color c) {
    return r + (r == Rule::RENJU ? c : 0);
}
```

So **one single `binary_file` covers all 3 rules** (FREESTYLE, STANDARD, RENJU) with per-rule tuned values.

When the engine receives a new rule via the Gomocup `START` command, it simply indexes into the already-loaded table using `tableIndex(rule, color)`.  
**No reload. No recomputation. Instant.**

---

## 7. Your Custom Rule: WALL + Teleportation Portals

### What changes at the fundamental level?

Your rule introduces:
- **Static WALL cells** — cells that are always occupied, blocking line patterns
- **Teleportation portals** — a pair of cells where a line "wraps" from one cell to the other

This affects the **`Pattern` computation** — the core of the classical eval.

### How patterns are currently computed:

```
Classic flow per cell:
  bitKey[dir] → fuseKey<R>() → PATTERN2x[key] → Pattern{Black,White}
  Pattern[dir0..dir3] → PCODE[p0][p1][p2][p3] → PatternCode
  EVALS[rule][pcode]  → cell eval score
```

The `PATTERN2x` tables are **precomputed from a fixed line definition** that assumes:
- All 4 directions are straight lines
- Boundaries are only at board edges + `BOARD_BOUNDARY` walls

### Issue 1 — WALL cells

Static WALLs already exist in the board representation (`WALL` color). The bitkey encoding uses `11` for WALL cells. So **WALL cells are partially handled at the bitkey level**.

**However:** The `PATTERN2x` lookup tables (`PATTERN2x[KeyCnt<FREESTYLE>]`) were computed assuming that walls only appear at board edges. If you introduce internal WALLs at fixed positions, the existing `PATTERN2x` tables would still be _queried correctly_ for those cells — the wall bit pattern (`11`) would just return `DEAD` pattern, which means the line is blocked there.

> ✅ **Static WALLs:** The classical eval tables (`binary_file`) likely **do not need retraining** if you correctly place WALL stones in the board initialization (`newGame()`). The pattern lookup will naturally return `DEAD` for blocked lines through walls. However, the **score calibration** (how much a WALL affects strategy) may be suboptimal since it was tuned for wall-free positions. Retuning would improve quality but may not be strictly required to get a working engine.

### Issue 2 — Teleportation Portals

A teleportation portal means a line can "jump" from cell A to cell B (not adjacent). This **fundamentally breaks** the current pattern computation:

```
Current assumption:
  Line direction = fixed step (e.g., RIGHT = +1)
  Line at pos P = {..., P-2, P-1, P, P+1, P+2, ...}

Teleportation:
  Line at pos P = {..., P-1, PORTAL_A, PORTAL_B, P+2, ...}
  The bitkey is built from contiguous row/column offsets → invalid for teleportation
```

The `bitKey0..3` arrays are built column/row by column/row:
```cpp
bitKey0[y] |= mask << (2 * x);   // row y: bits are positions x along row
```
A teleportation jump within a row/column would mean bits are non-contiguous, which the current lookup table cannot represent.

> ⚠️ **Teleportation portals:** The classical eval (`binary_file`) **cannot handle this directly**. You would need either:
> 1. Redesign the `Pattern` computation for teleportation-aware lines → rebuild `PATTERN2x` tables → retrain classical eval
> 2. Disable classical eval entirely and rely solely on NNUE (which can be trained to implicitly learn portal geometry)

---

## 8. Summary Table — Your Custom Engine Plan

| Feature | binary_file Impact | Action Needed |
|---------|-------------------|---------------|
| Static WALLs (fixed positions) | Partial compatibility — WALL bits handled | Optional retune for better quality |
| Teleportation portal | Incompatible with pattern bitkey design | Must redesign pattern system; retrain or use NNUE only |
| New rule alongside existing rules | Can add a new rule slot (index 4+) by expanding arrays | Modify `RULE_NB`, expand tables, retrain for new slot |
| NNUE weights (`weight_file`) | Completely separate from binary_file | Need to retrain NNUE separately (or start without it) |

---

## 9. Recommendation for Your Tool (binary_file Parser/Visualizer)

Since `binary_file` has a well-defined format (LZ4 + raw struct dump), a parser is straightforward:

### Format to parse:
```
[LZ4 header + compressed data]
  └── [8 bytes] double ScalingFactor
  └── [4 × 3876 × 2 bytes] EVALS[4][3876]          ← int16_t
  └── [4 × 2048 × 2 bytes] EVALS_THREAT[4][2048]   ← int16_t
  └── [4 × 3876 × 2 × 2 bytes] P4SCORES scores     ← int16_t[pcode][self/oppo]
```

### To reconstruct human-readable form:
For each `EVALS[rule][pcode]`:
- Decompose `pcode → (patDir0, patDir1, patDir2, patDir3)` by inverting `PatternConfig::PCODE[a][b][c][d]`
- Print as pattern name combination, e.g., `F3+B3+F1+DEAD → eval=42`

### Tool design suggestion:
```python
import lz4.frame, struct, numpy as np

with open("model.bin", "rb") as f:
    data = lz4.frame.decompress(f.read())

offset = 0
scaling = struct.unpack_from("d", data, offset)[0]; offset += 8

PCODE_NB = 3876; THREAT_NB = 2048; RULE_NB1 = 4

evals        = np.frombuffer(data, dtype=np.int16, count=RULE_NB1*PCODE_NB,   offset=offset)
evals        = evals.reshape(RULE_NB1, PCODE_NB); offset += evals.nbytes

evals_threat = np.frombuffer(data, dtype=np.int16, count=RULE_NB1*THREAT_NB,  offset=offset)
evals_threat = evals_threat.reshape(RULE_NB1, THREAT_NB); offset += evals_threat.nbytes

scores       = np.frombuffer(data, dtype=np.int16, count=RULE_NB1*PCODE_NB*2, offset=offset)
scores       = scores.reshape(RULE_NB1, PCODE_NB, 2)

# Then map pcode → (p0,p1,p2,p3) and visualize as heatmap
```

> **Note:** LZ4 in Rapfi uses the LZ4 **frame format** (via the `Compressor` class in `core/iohelper.h`). Make sure to use `lz4.frame` not `lz4.block`.

---

## 10. Architectural Decision for Your Engine

```
Option A — Keep classical eval, retrain for new rule
  ✅ Fast classical eval available
  ✅ No NNUE dependency
  ❌ Teleportation fundamentally breaks Pattern system
  ❌ Requires redesigning bitkey computation

Option B — Disable classical eval, NNUE only
  ✅ NNUE can implicitly learn custom board topology
  ✅ Cleaner separation
  ❌ Slower (NNUE takes more compute)
  ❌ Need sufficient training data for new rule

Option C — Hybrid: WALL-aware classical + portal-aware NNUE
  ✅ Best of both worlds
  ❌ Most engineering effort
```

**Recommended starting point:** Keep classical eval as-is for WALL support (walls → DEAD pattern naturally). Disable `binary_file`'s pattern tables for portal cells, and train a small NNUE to handle portal geometry. Use NNUE margin switching to fall back to classical when NNUE is not confident.
