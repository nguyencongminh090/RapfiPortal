---
name: wall-eval
description: >
  Classical evaluation heuristics and strategic corrections for WALL boards
  in portal_src. Use when adding or debugging wall-aware evaluation logic
  in eval.cpp, or designing heuristics to compensate for NNUE blindness near WALLs.
---

# Skill: WALL Board Classical Evaluation

## 1. Why Classical Eval Needs WALL Awareness

The NNUE evaluator (`mix9svq`) evaluates positions based on learned pattern statistics
from **free-board games**. Near WALLs, several strategic concepts are distorted:

| Free Board | WALL Board |
|-----------|-----------|
| Center is strongest | Center may be split — a pocket can become the new "center" |
| Edge = weaker starts | Wall-adjacent cells can be fortress anchors |
| Threat count = good proxy for winning | Threats behind a WALL wall are isolated — not addable |
| 4-in-a-row is ~equally dangerous anywhere | 4-in-a-row in a 5-cell corridor = unstoppable |

The classical eval in `eval.cpp` runs **first** (as `basicEval + threatEval`). If the NNUE
evaluator is outside the alpha-beta margin, classical eval is the only signal. Adding a
WALL correction here immediately improves the engine even before retraining.

## 2. Where To Add the Correction in `eval.cpp`

```cpp
// In Evaluation::evaluate<R>() [eval.cpp ~line 96]
Value basicEval  = (evaluateBasic(st0, self) + evaluateBasic(st1, self)) / 2;
Value threatEval = evaluateThreat<R>(st0, self);
Value eval       = std::clamp(basicEval + threatEval, VALUE_EVAL_MIN, VALUE_EVAL_MAX);

// [PORTAL: WALL CORRECTION — add here]
if (board.hasWalls()) {
    Value wallCorrection = computeWallStrategicCorrection<R>(board, self);
    eval = std::clamp(eval + wallCorrection, VALUE_EVAL_MIN, VALUE_EVAL_MAX);
}

if (board.evaluator()) {
    // ... NNUE path (unchanged)
}
```

The correction is added to the classical eval value, which then feeds into the
NNUE margin check. If `eval` is outside the alpha-beta window even after correction,
the NNUE is skipped and the corrected classical value is returned directly.

## 3. WALL Correction Heuristics

### Heuristic A: Dead Pocket Penalty

A "dead pocket" is a region enclosed by WALLs and/or board edges that is too small
to contain a 5-in-a-row in ANY direction. Cells in dead pockets are tactically useless.

```cpp
// Detect dead pockets using flood-fill from each empty cell
// A pocket of size < 5 in its widest dimension = dead pocket
int deadPocketCount = countDeadPockets(board);
// Penalize the side with more stones trapped in dead pockets
Value pocketPenalty = deadPocketCount * Config::WALL_DEAD_POCKET_PENALTY;
```

**Constants (add to `config.h`):**
```cpp
inline int WALL_DEAD_POCKET_PENALTY = -80;  // per dead pocket cell trapped
```

### Heuristic B: Corridor Squeeze Bonus

A "corridor" is a row/column/diagonal with width exactly 5 — the minimum for a win.
Any open-4 (B4) in a corridor is **unstoppable** (defender has no room to block both ends).

```cpp
// For each direction D, scan lines of exactly 5 playable cells (wall-to-wall or edge-to-wall)
// If self has a B4 in any such corridor → large bonus
bool corridorFour = hasCorridorFour(board, self);
Value corridorBonus = corridorFour ? Config::WALL_CORRIDOR_FOUR_BONUS : 0;
```

**Constants:**
```cpp
inline int WALL_CORRIDOR_FOUR_BONUS = +500;  // unstoppable B4 in 5-cell corridor
```

### Heuristic C: WALL-Adjacent Threat Isolation

A threat that is **fully enclosed** behind WALLs cannot be part of a longer line.
Such threats should be discounted — they represent local pressure, not global attack.

```cpp
// Count threats (B3, B4) per "region" (flood-fill zone separated by WALLs)
// Discount threats in small regions (< 10 cells)
int isolatedThreatCount = countIsolatedThreats(board, self);
Value isolationPenalty = isolatedThreatCount * Config::WALL_ISOLATED_THREAT_PENALTY;
```

**Constants:**
```cpp
inline int WALL_ISOLATED_THREAT_PENALTY = -40;  // per isolated threat cell
```

### Heuristic D: First-Move Zone Bonus (WALL Rule)

In WALL rule, the first stone MUST be placed in the 8 cells surrounding a WALL.
The engine should strongly prioritize these cells in the opening (ply ≤ 2).

```cpp
if (board.ply() <= 2) {
    // Bonus if the candidate move is in firstMoveZone[]
    Value openingBonus = isInFirstMoveZone(board, move) ? Config::WALL_FIRST_MOVE_BONUS : 0;
}
```

**Constants:**
```cpp
inline int WALL_FIRST_MOVE_BONUS = +300;  // for moves in the mandatory first-move zone
```

## 4. Implementation Guide for `computeWallStrategicCorrection`

```cpp
/// Compute WALL-specific strategic correction value.
/// Called from evaluate<R>() when board has at least one WALL cell.
///
/// @param board  The board to evaluate (read-only).
/// @param self   The side to move.
/// @return       Correction value to ADD to classical eval.
///               Positive = good for self, Negative = bad for self.
template <Rule R>
Value computeWallStrategicCorrection(const Board &board, Color self)
{
    Value correction = VALUE_ZERO;

    // [A] Dead pocket penalty
    // PORTAL: Penalize both sides proportionally to their stones in dead pockets
    auto [selfDead, oppoDead] = countDeadPocketStones(board, self);
    correction += (oppoDead - selfDead) * Config::WALL_DEAD_POCKET_PENALTY;

    // [B] Corridor squeeze bonus
    // PORTAL: B4 in a 5-cell corridor is unstoppable — large bonus
    if (hasCorridorFour(board, self))
        correction += Config::WALL_CORRIDOR_FOUR_BONUS;
    if (hasCorridorFour(board, ~self))
        correction -= Config::WALL_CORRIDOR_FOUR_BONUS;

    // [C] Isolated threat discount
    // PORTAL: Discount threats that cannot extend beyond WALL boundaries
    correction += countIsolatedThreats(board, ~self) * Config::WALL_ISOLATED_THREAT_PENALTY;
    correction -= countIsolatedThreats(board, self) * Config::WALL_ISOLATED_THREAT_PENALTY;

    return std::clamp(correction, Value(-800), Value(+800));
}
```

## 5. Required Constants in `config.h`/`config.cpp`

Add to `config.h` in the `Config` namespace:
```cpp
// PORTAL: WALL evaluation heuristic constants
inline int WALL_DEAD_POCKET_PENALTY     = -80;
inline int WALL_CORRIDOR_FOUR_BONUS     = +500;
inline int WALL_ISOLATED_THREAT_PENALTY = -40;
inline int WALL_FIRST_MOVE_BONUS        = +300;
```

These can be tuned via the config file mechanism (same pattern as existing `EvaluatorMarginScale`).

## 6. Testing the Correction

After implementing, validate these scenarios manually:

| Scenario | Expected effect |
|----------|----------------|
| Board with a 5-cell corridor, self has open-4 in it | eval increases by ~500 |
| Board with a 1-cell dead pocket trapping opponent's stone | eval increases by ~80 |
| All threats isolated behind WALLs | eval near zero (neither side has real advantage) |
| Opening on WALL board, first move outside first-move zone | engine penalizes and picks a zone cell instead |

Use `test_play.py` from pytorch-nnue-trainer, or write a small `portal-test` case that
checks these specific positions.
