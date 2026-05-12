---
name: portal-engine-skill
description: >
  Deep knowledge of Portal Gomoku Engine architecture (portal_src) — use when
  implementing or debugging board.h/board.cpp, getKeyAt(), NNUE encoding,
  eval.cpp, or any WALL/Portal feature in the engine.
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
    movegen.h/.cpp   ← DO NOT TOUCH
  eval/
    mix9svqnnue.h    ← MODIFIED (WALL-aware): Accumulator stores wallMap
    mix9svqnnue.cpp  ← MODIFIED (WALL-aware): initIndexTable(), move()
    eval.cpp         ← MODIFIED: wallStrategicCorrection hook
    eval.h           ← unchanged
    evaluator.h/.cpp ← unchanged
    mix10nnue.*      ← DO NOT TOUCH (not in use for this variant)
    simdops.h        ← DO NOT TOUCH
    weightloader.h   ← unchanged
  config.h/.cpp      ← MODIFIED: Portal/WALL config storage
  command/
    gomocup.cpp      ← MODIFIED: WALL/PORTAL protocol input
    opengen.cpp      ← MODIFIED: new rule support
  main.cpp           ← MODIFIED: new entry
  [core/, search/, database/, tuning/, external/] ← DO NOT TOUCH
```

**Build artifacts** (in `build/` relative to project root):
- `pbrain-MINT-P` — main engine binary
- `portal-test` — test binary for portal mechanics
- `portal-pattern-test` — test binary for pattern/key generation

**Build command:**
```bash
cd /path/to/rapfi-master/portal_src
cmake --preset linux-clang-release   # or check CMakePresets.json for your preset
ninja -C build
```

## 2. Rule Semantics

### WALL
- Immovable cell — `cells[pos].piece = WALL`.
- Blocks lines in ALL 4 directions (exactly like a board boundary).
- Stored in `bitKey` as `0b00` (same as boundary sentinel). **Must be explicitly set** in `newGame()` by calling `flipBitKey(p, BLACK); flipBitKey(p, WHITE)` because `newGame()` initializes all cells to `0b11` (EMPTY).

### Portal Pair (A, B)
- Both stored as `piece = WALL` in `cells[]`.
- Teleport rule: a line scanning direction D that hits portal A → skip A, skip B, continue from B+direction in SAME direction D. Symmetric (B → A also).
- Portal cells are **zero-width**: do NOT contribute bits to the pattern window.
- Loop detection: if walking the virtual window revisits a physical cell already collected, terminate that end with WALL sentinel.

## 3. Key Data Structures

### Portal Data (board.h)
```cpp
struct PortalPair {
    Pos a;  ///< First portal cell
    Pos b;  ///< Second portal cell
};

// Inside Board:
PortalPair portals[MAX_PORTAL_PAIRS];
Pos  portalPartner[FULL_BOARD_CELL_COUNT]; // O(1) lookup: partner of any portal cell
bool portalAffected[4][FULL_BOARD_CELL_COUNT]; // true if cell within L steps of a portal in dir d
```

### Shape / Encoding (mix9svqnnue)
```cpp
// Per cell, per direction — index into mapping_index[]
indexTable[boardSize*boardSize][4]  // [cell][dir] → shape index in [0, ShapeNum)

// WALL-aware version needs:
wallDistTable[boardSize*boardSize][8]  // [cell][dir×2] → wall distance in each half-direction
```

## 4. `getKeyAt<R>(pos, dir)` — The Core Algorithm

This is the **ONLY** place portal teleportation is implemented.
All downstream code (PATTERN2x lookup, PCODE, EVALS) is unchanged.

```cpp
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

## 5. WALL-Aware Encoding in `initIndexTable()`

**Current bug:** Only uses board-edge distance, ignoring internal WALLs.
**Fix:** Scan each direction from `(x, y)` up to `half=5` steps; stop at WALL or boundary.

```cpp
// WALL-AWARE (pseudocode):
auto wallAwareDist = [&](int x, int y, int dx, int dy) -> int {
    for (int i = 1; i <= half; i++) {
        int nx = x + dx*i, ny = y + dy*i;
        if (nx < 0 || nx >= boardSize || ny < 0 || ny >= boardSize) return i - 1;
        if (board->cell(Pos(nx, ny)).piece == WALL) return i - 1;
    }
    return half;
};
int distx0 = wallAwareDist(x, y, -1, 0);  // LEFT
int distx1 = wallAwareDist(x, y, +1, 0);  // RIGHT
// ... similarly for all 4 directions
```

This function is called at game start (in `initIndexTable(board*)`) and produces the correct
shape index for each cell that encodes WALL-truncated line length.

## 6. Update Zone in `move()`

When a stone is placed at `pos`, cells needing `pattern2x` refresh:

```
Normal zone (unchanged from Rapfi):
  All cells within [-L, +L] in each of 4 directions from pos

Portal extension (NEW — PORTAL: comment required):
  For each direction D where a portal falls within [-L, +L] of pos:
    Also refresh cells within [-L, +L] around the portal's PARTNER in direction D

Reason: placing near portal A changes the virtual line seen by cells near portal B.
```

Also in `move()`: when sweeping to update `indexTable` entries for WALL-aware encoding,
**break the sweep loop** when encountering a WALL cell — do not update cells on the other side.

## 7. `newGame()` Checklist

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
// 5. [PORTAL:] Call initIndexTable(this) on NNUE accumulator with board pointer
```

## 8. `binary_file` / Classical Eval Compatibility

The classical evaluation tables (`EVALS`, `P4SCORES`, `EVALS_THREAT`) are **100% reusable**:
- `PatternCode` is derived from 4 direction patterns — same types, same table
- `EVALS[rule][pcode]` maps pattern combinations to scores — pattern MEANING is preserved through portals
- F5 through portal = still F5 = WIN

Portal positions create unusual pattern combinations not seen in original Rapfi training,
but the engine correctly evaluates threats. Re-tuning the classical eval improves accuracy
but is NOT required initially.

## 9. NNUE Evaluation Strategy Near WALLs

Beyond encoding fixes, the value head needs strategic context. In `eval.cpp`:

```cpp
// After getting NNUE value, apply WALL strategic correction:
Value wallCorrection = computeWallCorrection(board, ply);
// wallCorrection factors:
//   - number of dead pockets (wall-enclosed regions)
//   - corridor width (single-cell corridors are tactically critical)
//   - number of wall-adjacent threats blocked for each side
Value finalValue = blend(nnueValue, nnueValue + wallCorrection, wallInfluence);
```

## 10. Common Pitfalls

| Pitfall | Correct Approach |
|---------|-----------------|
| Forgetting to update B's neighborhood when stone placed near A | Extend update zone via `portalUpdateZone` in `move()` |
| Loop in collinear portal chain | Keep `posList` in `buildPortalKey`; terminate window if a cell is visited twice |
| Using `Pos::NONE` (0) as sentinel | Cell (0,0) IS a valid board cell! Use `Pos::PASS` (-1) instead |
| Forgetting to fix WALL bitKey in `newGame()` | Rapfi inits empty cells to `0b11`. WALLs need `0b00`. Call `flipBitKey` for each WALL/Portal pos |
| Calling `initIndexTable()` before portals/WALLs are set up | WALLs must be registered in `cells[]` BEFORE `initIndexTable` scans for them |
| Modifying "copied, unchanged" files | Only touch files marked MODIFIED in the source map above |
| Sweeping past WALLs in `move()` update | Break the update loop at WALL boundaries in all 4 directions |
