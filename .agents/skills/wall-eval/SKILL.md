---
name: wall-eval
description: >
  Classical evaluation heuristics and strategic corrections for WALL/Portal boards
  in portal_src. PRIMARY method for WALL board evaluation (not a fallback).
  Use when implementing or debugging WALL-aware classical evaluation in eval.cpp,
  move ordering in movegen.cpp, or search tuning in searchthread.cpp.
---

# Skill: WALL Board Classical Evaluation & Search

## 1. Design Philosophy

The engine uses **classical evaluation as the primary signal** for WALL/Portal strategy.
NNUE (`mix9svq`) runs unchanged on top — it provides general Gomoku pattern strength.
The classical correction layer bridges the gap:

```
Final Eval = NNUE(position)          ← general Gomoku strength
           + wallCorrection(board)   ← WALL/Portal strategic overlay  ← OUR WORK
```

The correction is implemented in `eval.cpp` and feeds into the alpha-beta search.
Move ordering heuristics in `movegen.cpp` ensure WALL-adjacent cells are searched early.

## 2. Key Files and Their Roles

| File | Role | Status |
|------|------|--------|
| `eval/eval.cpp` | Compute and apply `wallCorrection` | **Implement here** |
| `config.h` | Heuristic constants (tunable) | **Add constants here** |
| `game/movegen.cpp` | Move ordering: score WALL/Portal-adjacent cells higher | **Implement here** |
| `search/searchthread.cpp` | LMR reductions near portals, depth extensions | **Implement here** |
| `search/vcfsearch.cpp` | VCF/VCT through portals (uses `getKeyAt()`) | **Verify correctness** |
| `game/board.h` | `hasWalls()`, `isAdjacentToWall()`, `isAdjacentToPortal()` queries | **Add helpers if missing** |

## 3. Where to Hook in `eval.cpp`

Find the `Evaluation::evaluate<R>()` function. The correction goes **after** classical eval
and **before** the NNUE path:

```cpp
// === EXISTING RAPFI CODE ===
Value basicEval  = (evaluateBasic(st0, self) + evaluateBasic(st1, self)) / 2;
Value threatEval = evaluateThreat<R>(st0, self);
Value eval       = std::clamp(basicEval + threatEval, VALUE_EVAL_MIN, VALUE_EVAL_MAX);

// === [PORTAL: WALL CORRECTION] ===
if (board.numWalls() > 0) {
    Value wallCorr = computeWallStrategicCorrection<R>(board, self);
    eval = std::clamp(eval + wallCorr, VALUE_EVAL_MIN, VALUE_EVAL_MAX);
}

// === EXISTING RAPFI CODE — NNUE path below ===
if (board.evaluator()) { ... }
```

## 4. Heuristic A: Dead Pocket Penalty

A **dead pocket** is a region enclosed by WALLs and/or board edges that is too narrow
(< 5 cells in every direction) to ever contain a winning 5-in-a-row.
Stones in dead pockets are tactically irrelevant — penalize the side with more such stones.

```cpp
/// Count stones of each color trapped in dead pockets (regions < 5 wide).
/// Uses BFS/flood-fill from each empty cell, grouped by WALL/edge boundaries.
std::pair<int,int> countDeadPocketStones(const Board &board, Color self)
{
    // BFS: collect connected regions separated by WALL cells and board edges.
    // For each region: compute max line length in H/V/diag directions.
    // If max_line_length < 5: region is "dead" — count trapped stones of each color.
    int selfDead = 0, oppoDead = 0;
    // ... implementation ...
    return {selfDead, oppoDead};
}

// In computeWallStrategicCorrection:
auto [selfDead, oppoDead] = countDeadPocketStones(board, self);
correction += (oppoDead - selfDead) * Config::WALL_DEAD_POCKET_PENALTY;
```

**Constant:** `WALL_DEAD_POCKET_PENALTY = -80` (penalty per trapped stone, relative)

## 5. Heuristic B: Corridor Squeeze Bonus

A **corridor** is a line segment of exactly 5 playable cells bounded by WALLs or edges.
An open-4 (flex-B4) in such a corridor is **unstoppable** — the defender cannot block both ends.
This is a critical tactical concept specific to WALL boards.

```cpp
/// Returns true if `side` has an open-4 in any 5-cell wall-bounded corridor.
bool hasCorridorFour(const Board &board, Color side)
{
    // For each row/column/diagonal:
    //   Scan for maximal segments bounded by WALL cells or edges.
    //   If segment length == 5 exactly:
    //     Check if `side` has a B4 pattern anywhere in the segment.
    // ...
}

// In computeWallStrategicCorrection:
if (hasCorridorFour(board, self))   correction += Config::WALL_CORRIDOR_FOUR_BONUS;
if (hasCorridorFour(board, ~self))  correction -= Config::WALL_CORRIDOR_FOUR_BONUS;
```

**Constant:** `WALL_CORRIDOR_FOUR_BONUS = +500` (near-winning position)

## 6. Heuristic C: Isolated Threat Discount

A threat (B3, B4) that is **fully enclosed** behind WALLs cannot grow into a double-threat.
Such threats should be discounted since they represent isolated local pressure only.

```cpp
/// Count threats (A4/B4/B3) for `side` that are isolated in regions smaller than threshold.
int countIsolatedThreats(const Board &board, Color side, int regionThreshold = 10)
{
    // Flood-fill to find region boundaries (WALLs/edges).
    // For each region with area < regionThreshold:
    //   Count cells with pattern4[side] >= B3 in this region.
    // ...
}

// In computeWallStrategicCorrection:
correction += countIsolatedThreats(board, ~self) * Config::WALL_ISOLATED_THREAT_PENALTY;
correction -= countIsolatedThreats(board, self)  * Config::WALL_ISOLATED_THREAT_PENALTY;
```

**Constant:** `WALL_ISOLATED_THREAT_PENALTY = -40` (per isolated threat cell)

## 7. Heuristic D: First-Move Zone Bonus

In the WALL game, the very first stone **must** be in the 8 cells surrounding a WALL.
The engine must strongly prefer these cells at ply ≤ 1.
This belongs in **move ordering** (`movegen.cpp`), not in `eval.cpp`.

```cpp
// In MovePicker::score() or equivalent:
if (board.ply() == 0 && board.isInFirstMoveZone(pos))
    score += Config::WALL_FIRST_MOVE_BONUS;
```

**Constant:** `WALL_FIRST_MOVE_BONUS = +300`

## 8. Required Constants in `config.h`

Add inside the `Config` namespace:
```cpp
// PORTAL: WALL evaluation heuristic constants — tunable via config file
inline int WALL_DEAD_POCKET_PENALTY     = -80;   // per trapped stone in dead pocket
inline int WALL_CORRIDOR_FOUR_BONUS     = +500;  // B4 in 5-cell corridor = unstoppable
inline int WALL_ISOLATED_THREAT_PENALTY = -40;   // per isolated threat in small region
inline int WALL_FIRST_MOVE_BONUS        = +300;  // for legal first-move zone cells
```

## 9. Move Ordering Enhancements (`movegen.cpp`)

In the `MovePicker` or equivalent move scoring loop, add WALL/Portal bonuses:

```cpp
// [PORTAL:] WALL-adjacent cells are often critical (forced defense or attack anchor)
if (board.numWalls() > 0 && board.isAdjacentToWall(pos))
    score += Config::WALL_ADJACENCY_MOVE_BONUS;  // suggest: +100

// [PORTAL:] Portal-adjacent cells can create or block teleport threats
if (board.numPortals() > 0 && board.isAdjacentToPortal(pos))
    score += Config::PORTAL_ADJACENCY_MOVE_BONUS; // suggest: +80
```

Add helpers to `board.h` if not already present:
```cpp
bool isAdjacentToWall(Pos pos) const;
bool isAdjacentToPortal(Pos pos) const;
bool isInFirstMoveZone(Pos pos) const;
int  numWalls() const { return (int)walls_.size(); }
int  numPortals() const { return (int)portals_.size(); }
```

## 10. Search Enhancements (`searchthread.cpp` / `ab.cpp`)

### LMR Reduction Override for Portal Interactions
In the Late-Move Reduction logic, reduce the reduction for moves that interact with portals:

```cpp
// Standard LMR (existing Rapfi code):
int reduction = lmrTable[depth][moveCount];

// [PORTAL:] Reduce LMR for moves near active portal — don't prune portal threats early
if (board.numPortals() > 0 && board.isAdjacentToPortal(move)
    && (self_threat >= B3 || oppo_threat >= B3))
    reduction = std::max(0, reduction - 1);
```

### VCF/VCT Through Portals (`vcfsearch.cpp`)
The VCF search calls `getKeyAt()` which already handles portals via `buildPortalKey()`.
**Verify** this path is exercised by adding a test case in `portal-test`:
```
Portal pair at (7,7)↔(9,7). BLACK 4-in-a-row at (3,7)-(6,7). Is VCF detected via portal?
```

## 11. Full Correction Function Template

```cpp
/// Compute WALL-specific strategic correction value for classical eval.
/// Called from evaluate<R>() when board has at least one WALL cell.
template <Rule R>
Value computeWallStrategicCorrection(const Board &board, Color self)
{
    Value correction = VALUE_ZERO;

    // [A] Dead pocket penalty — penalize stones trapped in sub-5 regions
    auto [selfDead, oppoDead] = countDeadPocketStones(board, self);
    correction += Value((oppoDead - selfDead) * Config::WALL_DEAD_POCKET_PENALTY);

    // [B] Corridor squeeze bonus — B4 in 5-cell corridor is unstoppable
    if (hasCorridorFour(board, self))
        correction += Config::WALL_CORRIDOR_FOUR_BONUS;
    if (hasCorridorFour(board, ~self))
        correction -= Config::WALL_CORRIDOR_FOUR_BONUS;

    // [C] Isolated threat discount — B3/B4 behind walls cannot grow
    correction += Value(countIsolatedThreats(board, ~self) * Config::WALL_ISOLATED_THREAT_PENALTY);
    correction -= Value(countIsolatedThreats(board, self)  * Config::WALL_ISOLATED_THREAT_PENALTY);

    return std::clamp(correction, Value(-800), Value(+800));
}
```

## 12. Validation Scenarios

| Scenario | Expected Effect |
|----------|----------------|
| 5-cell corridor, self has open-4 | `eval` increases ~+500 |
| 1-cell dead pocket trapping opponent stone | `eval` increases ~+80 |
| All threats isolated behind WALLs | `eval` near zero for both sides |
| Opening at ply 0 outside first-move zone | Engine penalizes, prefers zone cell |
| Portal-adjacent move in alpha-beta | LMR reduction reduced by 1 ply |
| VCF through portal (4-in-a-row teleports to 5) | VCF detected and returned |
