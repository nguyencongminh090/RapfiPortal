---
name: nnue-trainer
description: >
  Deep knowledge of mix9svq NNUE architecture, WALL-aware encoding fixes, and
  pytorch-nnue-trainer pipeline. Use when training, fine-tuning, or debugging
  the neural network evaluator for WALL/Portal board variants.
---

# Skill: Rapfi NNUE Training & Optimization

## 1. Architecture Map (mix9svq)

```
Input: board state (per-cell)
  └─ initIndexTable()  ← 4 shape indices per cell [H, V, D1, D2]
        │   ← BUG: currently uses board-edge distance only, ignores WALL distance
        ▼
  mapping_index[2][ShapeNum=442503]   ← shape → codebook entry (frozen)
        ▼
  codebook[2][65536][FeatureDim=64]   ← VQ compressed features (frozen)
        ▼  (summed per cell into mapSum[])
  feature_dwconv_weight[9][FeatDWConvDim] ← 3×3 spatial convolution (frozen)
        ▼  (spread into mapConv[outerBoardSize²])
  valueSum.global / valueSum.group[3][3]   ← spatial aggregation (TRAIN)
        ▼
  evaluateValue()   StarBlock MLP: value_corner/edge/center/quad (TRAIN)
        ▼
  value_l1 / value_l2 / value_l3  final MLP (TRAIN)
        ▼
  Win/Loss/Draw probabilities
```

Key constants (from `mix9svqnnue.h`):
- `ShapeNum = 442503` — total distinct 11-cell line shapes (ternary)
- `FeatureDim = 64` — codebook feature dimension
- `FeatDWConvDim = 32` — DWConv (first half of FeatureDim)
- `MAX_BOARD_SIZE = 22` — hard architectural limit (`FULL_BOARD_SIZE=32`, `BOUNDARY=5`)

## 2. The WALL Encoding Bug (Must Fix Before Training)

**File:** `portal_src/eval/mix9svqnnue.cpp` — `Accumulator::initIndexTable()` (line ~255)

**Problem:** The index table uses only board-edge distance as boundary. It is blind
to internal WALL cells, so cells behind a WALL receive the same shape index as cells
with an open line, corrupting the NNUE input.

```cpp
// CURRENT (wrong near WALLs):
int distx0 = std::min(x - 0, half);          // distance to left EDGE only
int distx1 = std::min(boardSize - 1 - x, half); // distance to right EDGE only

// NEEDED (wall-aware):
// Scan direction for WALLs up to distance `half`, use whichever is closer
int distx0 = wallAwareDistance(board, x, y, Direction::LEFT, half);
int distx1 = wallAwareDistance(board, x, y, Direction::RIGHT, half);
```

**Also fix:** `Accumulator::move()` — When a stone is placed, shapes are updated in
all 4 directions. Currently the update sweeps past WALL cells, contaminating cells on
the other side. Must break the update loop at WALL boundaries.

> [!IMPORTANT]
> The self-play data generator MUST use the fixed engine. If training data is generated
> with the unfixed engine, positions near WALLs are mislabeled → garbage training signal.

## 3. Transfer Learning Strategy (Free Board → WALL Board)

**Core insight:** A WALL-truncated line (e.g., `BB.W` where W=wall) encodes identically
to a board-edge-truncated line (e.g., `BB.` near the edge). The codebook already knows
all 442,503 shapes including every edge pattern. Only the value aggregation is wrong.

### Freeze (pattern recognition — transfers perfectly):
| Layer | Reason |
|-------|--------|
| `codebook[2][65536][64]` | Local pattern vocabulary; WALL = edge to codebook |
| `mapping_index[2][442503]` | Shape → codebook; same shapes, same meanings |
| `feature_dwconv_weight[9][32]` | 3×3 local spatial mixing; size-independent |
| Policy head layers | Local policy; transfers well |

### Train (value aggregation — must relearn):
| Layer | Reason |
|-------|--------|
| `value_corner` StarBlock | WALL creates false "corner pockets" |
| `value_edge` StarBlock | Threat density changes near WALLs |
| `value_center` StarBlock | WALLs can partition the center |
| `value_quad` StarBlock | 2×2 quadrant distribution is disrupted |
| `value_l1/l2/l3` MLP | Final output reweighting needed |

**Result:** ~80% faster training, no catastrophic forgetting of basic Gomoku tactics.

## 4. Training Pipeline (`pytorch-nnue-trainer`)

**Repo:** `dhbloo/pytorch-nnue-trainer`

### Key dependencies:
```bash
pip install accelerate configargparse tqdm tensorboard lz4
pip install dataset/pipeline/line_encoding_cpp   # C++ pipeline — REQUIRED for mix9
```

### Training command:
```bash
accelerate launch train.py \
  -r run_dirs/wall_ft \
  -d ./wall_selfplay_data \
  --dataset_type packed_binary \
  --model_type mix9svq \
  --pretrained_checkpoint path/to/existing_weights.pt \
  --freeze_layers codebook mapping_index feature_dwconv \
  --learning_rate 0.0001 \
  --batch_size 512 \
  --iterations 200000
```

### Export command:
```bash
python export.py \
  -c run_dirs/wall_ft/run_config.yaml \
  -p run_dirs/wall_ft/checkpoints/best.pt \
  -o wall_freestyle_17.bin \
  --export_type bin-lz4 \
  --export_args "{rule: freestyle, board_size: 17}"
```

### Weight file header (from `weightloader.h`):
```cpp
struct Header {
    uint32_t magic;          // 0xacd8cc6a
    uint32_t arch_hash;      // must match model architecture
    uint32_t rule_mask;      // 1=freestyle, 2=standard, 4=renju
    uint32_t boardsize_mask; // bit i set = size (i+1) supported
    uint32_t desc_len;
    char     description[];
};
```
To test a weight on a board size not in its `boardsize_mask`, patch the header or
set `boardsize_mask = 0xFFFFFFFF` in the export args.

## 5. Multi-Size Training

The architecture is size-agnostic (11-cell window is local). To support multiple sizes:

```yaml
# Mix training data from multiple sizes
datasets:
  - path: ./data/free_15x15/    # weight: 0.30
  - path: ./data/free_20x20/    # weight: 0.20
  - path: ./data/wall_15x15/    # weight: 0.15
  - path: ./data/wall_17x17/    # weight: 0.15
  - path: ./data/wall_20x20/    # weight: 0.20
```

Export separate `.bin` files per target size. The engine selects the matching weight
at load time via `boardsize_mask`.

## 6. Data Generation Requirements

- Self-play via `c-gomoku-cli` or modified Rapfi engine outputting `.binpack`/`.binpack.lz4`
- **For WALL training**: use random WALL placement (3 walls per game, constraints from game rules)
- Target: ~50K–100K WALL games (~10M labeled positions)
- Minimum search depth: 10–15 (for tactical accuracy)

## 7. Common Pitfalls

| Pitfall | Fix |
|---------|-----|
| Generating data before fixing `initIndexTable` | Fix encoding FIRST. Label quality = training quality. |
| `boardsize_mask` blocks testing on new size | Patch header or use `boardsize_mask = 0xFFFFFFFF` in export |
| Fine-tuning LR too high → forgets Gomoku basics | Use ≤ 0.0001 (10× lower than from-scratch) |
| Training only on WALLs → regression on free boards | Mix 50% free + 50% WALL data |
| Confusing `mix9svq` with `mix10` | mix10 is a different arch — weights are not interchangeable |
