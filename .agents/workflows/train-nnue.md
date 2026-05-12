---
description: Train or fine-tune the mix9svq NNUE model for WALL/Portal board variants
---

# Workflow: Train Rapfi NNUE (WALL / Portal Variant)

// turbo-all

## IMPORTANT: Prerequisites Before Starting

> [!WARNING]
> **Phase 0 (Encoding Fix) MUST be complete before generating training data.**
> If `Accumulator::initIndexTable()` still uses board-edge distance only (ignoring WALLs),
> all self-play positions near WALLs will be mislabeled. Garbage in = garbage out.
> Verify by checking `mix9svqnnue.cpp` line ~305 that the loop breaks at WALL cells.

---

## Phase 1 — Environment Setup

1. **Verify the portal engine builds cleanly:**
   ```bash
   cd portal_src
   ninja -C build
   # Confirm pbrain-MINT-P exists in build/
   ls build/pbrain-MINT-P
   ```

2. **Clone and set up the trainer** (skip if already present):
   ```bash
   git clone https://github.com/dhbloo/pytorch-nnue-trainer
   cd pytorch-nnue-trainer

   # Python 3.10+ and PyTorch 2.3+ required
   pip install accelerate configargparse tqdm tensorboard lz4
   pip install dataset/pipeline/line_encoding_cpp  # C++ pipeline — REQUIRED for mix9svq
   ```

3. **Configure accelerate** (one-time setup):
   ```bash
   accelerate config
   # Select: single GPU or CPU-only as appropriate
   ```

---

## Phase 2 — Generate WALL Self-Play Data

1. **Locate or set up `c-gomoku-cli`** (data generator):
   - Repo: `dhbloo/c-gomoku-cli`
   - Alternatively, use the portal engine's built-in self-play mode if available.

2. **Run self-play with WALL boards:**
   ```bash
   # Example: generate games on 15×15, 17×17, and 20×20 with 3 WALLs each
   # Adjust engine path, threads, and game count to your hardware
   c-gomoku-cli \
     -engine1 cmd="./portal_src/build/pbrain-MINT-P" \
     -engine2 cmd="./portal_src/build/pbrain-MINT-P" \
     -rule freestyle -boardsize 17 \
     -games 20000 -threads 4 \
     -wall_count 3 \
     -output "./wall_data_17x17.binpack.lz4"
   ```
   - Target: **50K–100K** total games across sizes (15, 17, 20).
   - Use `wall_count=3` per game with random placement (distance constraints enforced by generator).

3. **Verify dataset format:**
   ```bash
   cd pytorch-nnue-trainer
   python visualize_dataset.py --dataset_type packed_binary ./wall_data_17x17.binpack.lz4
   # Confirm: board states near WALLs show correct truncated line shapes
   ```

---

## Phase 3 — Configure Fine-Tuning

1. **Create `configs/wall_finetune.yaml`:**
   ```yaml
   # Model
   model_type: mix9svq
   model_args:
     dim_middle: 64
     dim_policy: 32
     dim_value: 64
     input_type: basic

   # Fine-tuning — freeze pattern recognition, retrain value head
   pretrained_checkpoint: /path/to/existing/mix9svq_checkpoint.pt
   freeze_layers:
     - codebook          # VQ pattern vocabulary — transfers perfectly
     - mapping_index     # shape→codebook map — transfers perfectly
     - feature_dwconv    # 3×3 spatial conv — size-independent

   # Optimizer — 10× lower LR to prevent forgetting free-board tactics
   optimizer: AdamW
   learning_rate: 0.0001
   weight_decay: 0.01

   # Training
   batch_size: 512
   iterations: 200000   # ~5× less than from-scratch

   # Data — mix free-board + WALL to prevent regression on free boards
   dataset_type: packed_binary
   dataset_path:
     - path: ./wall_data_15x15.binpack.lz4
       weight: 0.25
     - path: ./wall_data_17x17.binpack.lz4
       weight: 0.30
     - path: ./wall_data_20x20.binpack.lz4
       weight: 0.25
     - path: ./free_data_existing.binpack.lz4   # existing free-board data
       weight: 0.20
   ```

   > [!TIP]
   > The 20% free-board mix prevents catastrophic forgetting.
   > If you see regression on free boards during validation, increase this to 35%.

---

## Phase 4 — Train

1. **Launch training:**
   ```bash
   cd pytorch-nnue-trainer
   accelerate launch train.py -c configs/wall_finetune.yaml -r run_dirs/wall_ft_01
   ```

2. **Monitor convergence:**
   ```bash
   tensorboard --logdir run_dirs/wall_ft_01
   ```
   - Fine-tuned value head should converge significantly faster than from-scratch.
   - Expect meaningful improvement in 50K–100K iterations.

3. **Resume if interrupted:**
   ```bash
   # Trainer auto-detects last checkpoint from run_config.yaml
   accelerate launch train.py -c run_dirs/wall_ft_01/run_config.yaml
   ```

---

## Phase 5 — Export

1. **Export for each target board size:**
   ```bash
   # 17×17 freestyle
   python export.py \
     -c run_dirs/wall_ft_01/run_config.yaml \
     -p run_dirs/wall_ft_01/checkpoints/best.pt \
     -o weights/wall_freestyle_17.bin \
     --export_type bin-lz4 \
     --export_args "{rule: freestyle, board_size: 17}"

   # 15×15 freestyle
   python export.py \
     -c run_dirs/wall_ft_01/run_config.yaml \
     -p run_dirs/wall_ft_01/checkpoints/best.pt \
     -o weights/wall_freestyle_15.bin \
     --export_type bin-lz4 \
     --export_args "{rule: freestyle, board_size: 15}"
   ```

2. **Deploy weights:**
   - Place `.bin` files where the engine's config points for the WALL rule variant.
   - The engine selects the correct weight at load time via the `boardsize_mask` header.

---

## Phase 6 — Validate

1. **Tournament test: new weights vs. old weights on WALL boards:**
   - Target: **>55% win rate** for new weights on WALL boards.
   - Acceptable regression on free boards: **<3%** (i.e., <48.5% win rate on free boards).

2. **Regression test on free boards:**
   - Run same tournament on standard free boards with no WALLs.
   - If regression >3%, increase free-board data mix ratio and retrain.

3. **Pattern sanity check:**
   ```bash
   ./portal_src/build/portal-pattern-test
   # Verify: cells adjacent to WALLs receive different shape indices than before fix
   ```
