---
description: Enhance the engine's Classical Evaluation and Search Algorithm for custom rules (WALL/Portal)
---

# Workflow: Enhance Classical Eval & Search for WALL/Portal

// turbo-all

## Context & Constraint

NNUE (`mix9svqnnue.*`) is **frozen at clean Rapfi baseline** — do not touch it.
All improvements go through:
1. **Classical Evaluation** — `eval/eval.cpp` + `config.h`
2. **Move Ordering** — `game/movegen.cpp`  
3. **Search Tuning** — `search/searchthread.cpp`, `search/ab.cpp`, `search/vcfsearch.cpp`

Read the `wall-eval` skill for implementation templates before coding anything.

---

## Phase 1 — Diagnose the Gap

1. **Classify the strategic weakness:**

   | Symptom | Root cause | Fix in |
   |---------|-----------|--------|
   | Engine plays into dead pocket (doomed region) | Missing dead-pocket penalty | `eval.cpp` |
   | Engine misses unstoppable B4 in 5-cell corridor | Missing corridor bonus | `eval.cpp` |
   | Engine ignores threats that teleport via portal | VCF not following portal | `vcfsearch.cpp` |
   | Engine moves late to critical WALL-adjacent cell | Move ordering too generic | `movegen.cpp` |
   | Engine doesn't see deep portal threat sequences | LMR pruning too aggressive | `ab.cpp` |
   | Opening move placed outside mandatory first-move zone | Missing first-move bonus | `movegen.cpp` |

2. **Verify the bug is not a pattern bug:**
   ```bash
   cd portal_src
   ninja -C build
   ./build/portal-pattern-test   # all 79 tests must pass
   ./build/portal-test
   ```
   If tests fail → use the `debug-engine` workflow first.

---

## Phase 2 — Read Before Coding

3. **Read the relevant source files in full:**
   ```bash
   # For eval changes:
   cat portal_src/eval/eval.cpp
   cat portal_src/config.h

   # For move ordering changes:
   cat portal_src/game/movegen.cpp
   cat portal_src/game/movegen.h

   # For search changes:
   cat portal_src/search/searchthread.cpp
   cat portal_src/search/vcfsearch.cpp
   ```

4. **Identify the exact hook point** — write a 3-line summary of where the change goes before implementing.

---

## Phase 3 — Implement (One Heuristic at a Time)

### 3a. Classical Evaluation (`eval.cpp`)

Find `Evaluation::evaluate<R>()`. Add the WALL correction **after** classical eval,
**before** the NNUE path:

```cpp
// After: Value eval = clamp(basicEval + threatEval, ...)
// [PORTAL: WALL CORRECTION]
if (board.numWalls() > 0) {
    Value wallCorr = computeWallStrategicCorrection<R>(board, self);
    eval = clamp(eval + wallCorr, VALUE_EVAL_MIN, VALUE_EVAL_MAX);
}
// Before: if (board.evaluator()) { ... }
```

Implement `computeWallStrategicCorrection<R>()` with these sub-heuristics
(implement each one separately and test before adding the next):

- **[A] Dead Pocket Penalty** — BFS flood fill, count stones in regions < 5 wide
- **[B] Corridor Squeeze Bonus** — scan 5-cell bounded segments, check for B4
- **[C] Isolated Threat Discount** — discount B3/B4 in small walled-off regions

Add required constants to `config.h`:
```cpp
inline int WALL_DEAD_POCKET_PENALTY     = -80;
inline int WALL_CORRIDOR_FOUR_BONUS     = +500;
inline int WALL_ISOLATED_THREAT_PENALTY = -40;
inline int WALL_FIRST_MOVE_BONUS        = +300;
```

### 3b. Move Ordering (`movegen.cpp`)

In `MovePicker` score assignment, add WALL/Portal cell bonuses:

```cpp
// [PORTAL:] WALL-adjacent cells often critical
if (board.numWalls() > 0 && board.isAdjacentToWall(pos))
    score += Config::WALL_ADJACENCY_MOVE_BONUS;  // ~+100

// [PORTAL:] Portal-adjacent cells enable teleport threats
if (board.numPortals() > 0 && board.isAdjacentToPortal(pos))
    score += Config::PORTAL_ADJACENCY_MOVE_BONUS;  // ~+80

// [PORTAL:] First-move zone at opening
if (board.ply() == 0 && board.isInFirstMoveZone(pos))
    score += Config::WALL_FIRST_MOVE_BONUS;  // +300
```

Add `isAdjacentToWall()`, `isAdjacentToPortal()`, `isInFirstMoveZone()` to `board.h` if missing.

### 3c. Search (`ab.cpp` / `searchthread.cpp`)

Override LMR for portal interactions:

```cpp
// [PORTAL:] Don't prune portal-adjacent moves too early
if (board.numPortals() > 0 && board.isAdjacentToPortal(move)
    && threatLevel >= B3)
    reduction = std::max(0, reduction - 1);
```

Verify VCF through portal:
- The VCF scanner calls `pattern4[color]` which is computed via `getKeyAt()`.
- `getKeyAt()` already handles portals via `buildPortalKey()`.
- Add a test case in `portal-test` that creates a 4-in-a-row that wins only through a portal.

---

## Phase 4 — Build & Verify

4. **After each sub-heuristic, build and test:**
   ```bash
   ninja -C portal_src/build
   ./portal_src/build/portal-pattern-test
   ./portal_src/build/portal-test
   ```
   All 79+ tests must pass after every change.

5. **Add new test cases** for any scenario the existing tests don't cover:
   - A dead pocket position → verify `eval` is negative for the trapped side
   - A 5-cell corridor B4 → verify `eval` is strongly positive for that side
   - First move outside zone → verify engine prefers zone cell

6. **Playtest validation:**
   - Start engine via piskvork: `START 17` + WALL positions via `YXBOARD` 
   - Verify opening move lands in first-move zone
   - Set up a corridor B4 — verify engine wins rather than playing elsewhere

---

## Phase 5 — Tune Constants

7. Constants in `config.h` are starting estimates. Tune by:
   - Running self-play tournaments (engine vs engine, 100 games per setting)
   - Adjusting constants to maximize win rate on WALL boards while keeping free-board strength stable
   - Target: **> 55% win rate** vs old engine on WALL boards, **< 3% regression** on free boards
