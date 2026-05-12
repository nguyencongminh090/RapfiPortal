---
name: portal-engine-skill
description: >
  Deep knowledge of Portal Gomoku Engine architecture (portal_src) — use when
  implementing or debugging board.h/board.cpp, getKeyAt(), classical eval.cpp,
  search, or any WALL/Portal feature in the engine.
---

# Skill: Portal Engine Development

## 1. Project Source Map

```
portal_src/
  game/
    board.h          ← MODIFIED: Portal struct, portalAffected[], getKeyAt()
    board.cpp        ← MODIFIED: newGame(), move(), undo()
    pattern.h/.cpp   ← DO NOT TOUCH (geometry-free pattern tables)
    wincheck.h       ← DO NOT TOUCH
    movegen.h/.cpp   ← CAN MODIFY: move ordering heuristics for WALL/Portal
  eval/
    eval.cpp         ← CAN MODIFY: WALL strategic correction hook
    eval.h           ← unchanged
    evaluator.h/.cpp ← unchanged
    mix9svqnnue.*    ← DO NOT TOUCH (NNUE modification is OUT OF SCOPE)
    mix10nnue.*      ← DO NOT TOUCH
    simdops.h        ← DO NOT TOUCH
    weightloader.h   ← unchanged
  search/
    searchthread.cpp ← CAN MODIFY: depth extensions for portal/wall threats
    ab.cpp           ← CAN MODIFY: null-move and LMR tuning for WALL boards
    vcfsearch.cpp    ← CAN MODIFY: VCF/VCT must follow portal teleportation
  config.h/.cpp      ← MODIFIED: Portal/WALL config + new heuristic constants
  command/
    gomocup.cpp      ← MODIFIED: WALL/PORTAL protocol input
    opengen.cpp      ← MODIFIED: new rule support
  main.cpp           ← MODIFIED: new entry
  [core/, database/, tuning/, external/] ← DO NOT TOUCH
```

**Build artifacts** (in `build/` relative to `portal_src/`):
- `pbrain-MINT-P` — main engine binary
- `portal-test` — integration tests for portal mechanics
- `portal-pattern-test` — pattern/key generation tests

**Build command:**
```bash
cd portal_src
ninja -C build
# First time or after CMake changes:
cmake --preset linux-clang-release && ninja -C build
```

## 2. Strategic Direction

> **NNUE modification is OUT OF SCOPE.** The `mix9svqnnue` files are restored to
> clean Rapfi baseline. All engine intelligence improvements go through:
> 1. **Classical Evaluation** — `eval.cpp` WALL correction heuristics
> 2. **Move Ordering** — `movegen.cpp` / `MovePicker` for topological awareness
> 3. **Search Tuning** — `searchthread.cpp`, `ab.cpp`, `vcfsearch.cpp`

## 3. Rule Semantics

### WALL
- Immovable cell — `cells[pos].piece = WALL`.
- Blocks lines in ALL 4 directions (exactly like a board boundary).
- Stored in `bitKey` as `0b00`. **Must be explicitly set** in `newGame()` by calling
  `flipBitKey(p, BLACK); flipBitKey(p, WHITE)` because `newGame()` initializes all cells to `0b11` (EMPTY).

### Portal Pair (A, B)
- Both stored as `piece = WALL` in `cells[]`.
- Teleport rule: a line scanning direction D that hits portal A → skip A, skip B, continue from B+direction in SAME direction D. Symmetric (B → A also).
- Portal cells are **zero-width**: do NOT contribute bits to the pattern window.
- Loop detection: if walking the virtual window revisits a physical cell already collected, terminate with WALL sentinel.

## 4. Key Data Structures

### Portal Data (board.h)
```cpp
struct PortalPair {
    Pos a;  ///< First portal cell
    Pos b;  ///< Second portal cell
};

// Inside Board:
PortalPair portals[MAX_PORTAL_PAIRS];
Pos  portalPartner[FULL_BOARD_CELL_COUNT]; // O(1) lookup: partner of any portal cell
bool portalAffected[4][FULL_BOARD_CELL_COUNT]; // true if cell within L steps of portal in dir d
Pos  portalUpdateZone[MAX_PORTAL_PAIRS][4][2*HALF_LINE_LEN+1]; // extra update cells for move()
```

## 5. `getKeyAt<R>(pos, dir)` — The Core Algorithm

This is the **ONLY** place portal teleportation is implemented.
All downstream code (PATTERN2x lookup, PCODE, EVALS) is unchanged.

```
getKeyAt(pos, dir):
  // Fast path: no portal in range — use existing bitKey rotation (unchanged Rapfi code)
  if (!portalAffected[dir][pos]):
      return fastBitKeyExtract(pos, dir)

  // Slow path: manually build virtual 11-cell window with portal skips
  return buildPortalKey(pos, dir)

buildPortalKey(pos, dir):
  // 1. Walk LEFT from pos using portalStep(), collecting up to `half` cells
  // 2. Center = pos
  // 3. Walk RIGHT from pos using portalStep(), collecting up to `half` cells
  // Sentinel: use Pos::PASS (-1), NOT Pos::NONE (0) — NONE collides with cell (0,0)!
  // Loop guard: if portalStep() lands on a cell already in the window, terminate with WALL.
  // 4. Assemble 2-bit colors from window into 64-bit key and return.

portalStep(cur, dir, sign):
  next = cur + DIRECTION[dir] * sign
  if out_of_bounds(next): return next
  partner = portalPartner[next]
  if partner != Pos::NONE:
      return partner + DIRECTION[dir] * sign  // teleport: skip both portal cells
  return next
```

## 6. Classical Evaluation Enhancement Points

The classical eval pipeline in `eval.cpp`:
```cpp
Value basicEval  = evaluateBasic(st0, self) + evaluateBasic(st1, self);
Value threatEval = evaluateThreat<R>(st0, self);
Value eval       = clamp(basicEval + threatEval, VALUE_EVAL_MIN, VALUE_EVAL_MAX);

// [PORTAL: WALL CORRECTION — the correct place to add WALL awareness]
if (board.hasWalls()) {
    Value wallCorrection = computeWallStrategicCorrection<R>(board, self);
    eval = clamp(eval + wallCorrection, VALUE_EVAL_MIN, VALUE_EVAL_MAX);
}
```

Key WALL heuristics to implement (see `wall-eval` skill for full details):
- **Dead pocket penalty**: regions enclosed by walls too small for a 5-in-a-row
- **Corridor squeeze bonus**: open-4 in an exactly-5-cell corridor = unstoppable
- **Isolated threat discount**: threats that cannot extend past WALL boundaries

## 7. Move Ordering Enhancement Points

In `movegen.cpp` / `MovePicker::score()`:
```cpp
// [PORTAL:] Bonus for moves adjacent to WALL (defensive necessity)
if (board.isAdjacentToWall(pos))
    score += WALL_ADJACENCY_MOVE_BONUS;

// [PORTAL:] Bonus for moves adjacent to portal (attacking opportunity)
if (board.isAdjacentToPortal(pos))
    score += PORTAL_ADJACENCY_MOVE_BONUS;
```

## 8. Search Enhancement Points

In `searchthread.cpp` / `ab.cpp`:
- **Reduced LMR for portal interactions**: if a candidate move is portal-adjacent and has a certain threat level, reduce the LMR reduction by 1 ply to search it deeper.
- **VCF portal extension** (`vcfsearch.cpp`): the threat sequence scanner must call `getKeyAt()` which already handles portals — verify this code path is exercised.

## 9. `newGame()` Checklist

```cpp
// 1. Standard board init (existing Rapfi code) ...

// 2. [PORTAL:] Set WALL/Portal cell bitKeys to 0b00
for (auto &wallPos : walls)
    flipBitKey(wallPos, BLACK), flipBitKey(wallPos, WHITE);
for (auto &pp : portals)
    flipBitKey(pp.a, BLACK), flipBitKey(pp.a, WHITE),
    flipBitKey(pp.b, BLACK), flipBitKey(pp.b, WHITE);

// 3. [PORTAL:] Set portalPartner[]
for (auto &pp : portals)
    portalPartner[pp.a] = pp.b, portalPartner[pp.b] = pp.a;

// 4. [PORTAL:] Compute portalAffected[4][] by walking L steps from each portal
// 5. [PORTAL:] Compute portalUpdateZone[][] for use in move()
// 6. Reset evaluator via initEmptyBoard() — DO NOT use syncWithBoard()
```

## 10. Classical Eval Table Compatibility

The classical evaluation tables (`EVALS`, `P4SCORES`) are **100% reusable**:
- `PatternCode` is derived from 4 direction patterns — same types, same table
- `EVALS[rule][pcode]` maps pattern combinations to scores — pattern MEANING is preserved through portals
- F5 through portal = still F5 = WIN
- The WALL correction (§6 above) is ADDITIVE to the existing table output

## 11. Common Pitfalls

| Pitfall | Correct Approach |
|---------|-----------------|
| Forgetting to update B's neighborhood when stone placed near A | Extend update zone via `portalUpdateZone` in `move()` |
| Loop in collinear portal chain | Keep `posList` in `buildPortalKey`; terminate window if cell visited twice |
| Using `Pos::NONE` (0) as sentinel | Cell (0,0) IS a valid board cell! Use `Pos::PASS` (-1) instead |
| Forgetting to fix WALL bitKey in `newGame()` | Rapfi inits empty cells to `0b11`. WALLs need `0b00`. Call `flipBitKey` |
| Modifying NNUE files | `mix9svqnnue.*` is clean Rapfi baseline — DO NOT TOUCH |
| Modifying "copied, unchanged" files | Only touch files marked MODIFIED or CAN MODIFY in §1 source map |
| Adding eval heuristics in the wrong place | Add `wallCorrection` AFTER `basicEval + threatEval`, BEFORE NNUE path |
