---
name: portal-engine-skill
description: Deep knowledge of Portal Gomoku Engine architecture — use when implementing or debugging portal_src code
---

# Skill: Portal Engine Development

## 1. Portal Rule (Confirmed Semantics)

### WALL
- Immovable cell. Cannot place stones.
- Blocks lines in ALL 4 directions (same as board boundary).
- Stored as `piece = WALL` in `cells[]`.

### Portal Pair (A, B)
- Both cells: immovable (cannot place stones), stored as `piece = WALL`.
- Teleportation rule (confirmed):
  - Portals teleport unconditionally in **ALL 4 directions**.
  - When scanning direction D and you reach portal A → **skip A, skip B**, continue from the cell after B in the **same direction D**. Same for B → skip A.
  - Portal cells are **zero-width** in the virtual line window: they do NOT contribute bits.
  - **Loop detection**: If teleporting through portals revisits a physical cell already in the currently building window, the window is terminated (the rest becomes WALL) to prevent infinite loops (especially in small collinear gaps).

### Visual Examples
```
Horizontal portal WIN (even if A and B are not on same row!):
  Physical:      (x:1)X (2)X [A=(3,5)] ... [B=(7,2)] (8)X (9)X (10)*
  Virtual (dir=H): X X X X X → WIN
```


## 2. Architecture: What Changes vs What Stays

### UNCHANGED (do NOT touch)
| Component | Why unchanged |
|-----------|--------------|
| `Pattern` enum (DEAD..F5) | Purely tactical, geometry-free |
| `Pattern4` enum | Geometry-free |
| `PatternCode` (0..3875) | Index into PCODE[p0][p1][p2][p3] |
| `PATTERN2x[key]` table | Maps 9-bit window → Pattern; input changes, not table |
| `PCODE[a][b][c][d]` table | 4 patterns → PatternCode |
| `EVALS[rule][pcode]` | binary_file classical eval — fully reusable |
| `EVALS_THREAT[rule][pcode]` | Threat eval — fully reusable |
| `P4SCORES[rule][pcode]` | Move ordering — fully reusable |
| All of `eval/`, `search/`, `tuning/` | Work on patterns, not geometry |

### MODIFIED
| Component | What changes |
|-----------|-------------|
| `board.h` | Add `Portal` struct, `portals[]` array, `getKeyAt()` portal logic |
| `board.cpp` | `newGame()`: init portals, set WALL bits. `move()`: portal update zone |
| `config.h` | Add portal/WALL storage |
| `command/gomocup.cpp` | Parse WALL/PORTAL from protocol |

## 3. Key Data Structures

## 3. Key Data Structures

### Portal Data (in board.h)
```cpp
struct PortalPair {
    Pos a;  ///< First portal cell
    Pos b;  ///< Second portal cell
};

// ... inside Board ...
PortalPair portals[MAX_PORTAL_PAIRS];
Pos portalPartner[FULL_BOARD_CELL_COUNT]; // O(1) teleport partner lookup
bool portalAffected[4][FULL_BOARD_CELL_COUNT]; // Gatings for dual-path getKeyAt
```

## 4. The Core Algorithm: `getKeyAt<R>(pos, dir)`

This is the ONLY place where portal teleportation is implemented.
All downstream code (PATTERN2x lookup, PCODE, EVALS) is unchanged.

### Current Rapfi implementation (conceptual):
```
getKeyAt(pos, dir):
  Extract 2*(2L+1) bits from bitKey[dir][row/col/diag]
  centered around pos
  → return as uint64_t key
```

### Portal-aware implementation:
```cpp
getKeyAt(pos, dir):
  // Fast path: no portal in range — use existing bitKey rotation
  if (!portalAffected[dir][pos]):
      return fastBitKeyExtract(pos, dir)   // unchanged Rapfi code

  // Slow path: build virtual window manually
  return buildPortalKey(pos, dir)

buildPortalKey(pos, dir):
  // 1. Walk left tracking windowCells using portalStep()
  // 2. Center is pos
  // 3. Walk right tracking windowCells using portalStep()
  // Use Pos::PASS (-1) as sentinel if we hit boundary or loop.
  // CRITICAL: Must track `posList` to detect if we step onto a cell
  // we already collected (loop detection). If loop detected, terminate that end with Walls.
  // 4. Assemble 2-bit colors from windowCells into 64-bit key and return.

portalStep(cur, dir, sign):
  next = cur + DIRECTION[dir] * sign
  if out_of_bounds(next): return next
  partner = portalPartner[next]
  if partner != Pos::NONE: 
      return partner + DIRECTION[dir] * sign   // Teleport unconditionally
  return next
```


## 5. Update Zone in `move()`

When a stone is placed at `pos`, these cells need `pattern2x` refresh:

### Normal zone (unchanged from Rapfi):
- All cells within `[-L, +L]` in each of 4 directions from `pos`

### Portal extension (NEW):
- If any portal is within `[-L, +L]` of `pos` in direction D:
  - Also update cells within `[-L, +L]` of the **other portal end** in direction D
  - Because placing a stone near portal A changes the virtual line for cells near portal B

```
Example:
  Portal A at x=4, B at x=7 (horizontal, row=5)
  Stone placed at (3,5) ← one step left of A

  Normal update zone: cells (3-L,5)..(3+L,5) = (-1,5)..(7,5) in horizontal
  Portal extension: cells (7-L,5)..(7+L,5) = (3,5)..(11,5) for B's neighborhood
  → ensures cells near B also get their virtual line recalculated
```

## 6. `newGame()` Changes

```cpp
// 1. Mark portal cells as WALL in cells[] and portalPartner[]
for each portal pair (A, B):
    cells[A].piece = WALL
    cells[B].piece = WALL
    portalPartner[A] = B
    portalPartner[B] = A

// 2. Compute portalAffected[][] and portalUpdateZone
// Walk from all portals out to depth L in all 4 directions.
// Any non-portal cell reached is marked portalAffected[dir] = true.

// 3. IMPORTANT: Set WALL bitKey to 0b00
// Since newGame() clears bitKeys to 0b11 (EMPTY), WALLs must be explicitly flipped:
// flipBitKey(p, BLACK); flipBitKey(p, WHITE);
```

## 7. `binary_file` Compatibility

The `binary_file` (Rapfi classical model) is **100% reusable** without retraining because:
1. `PatternCode` is computed from 4 direction patterns — same type, same table
2. `EVALS[rule][pcode]` maps pattern combinations to scores — pattern MEANING is preserved through portals
3. The portal only changes HOW we build the 9-cell window — not WHAT the window's bits mean

Portal positions create unusual pattern combinations that the original Rapfi never saw, but:
- F5 through portal = still F5 = WIN
- The engine will correctly evaluate threats even through portals
- Retuning later will improve accuracy but is NOT required initially

## 8. Common Pitfalls

| Pitfall | Correct approach |
|---------|----------------|
| Forgetting to update B's neighborhood when stone placed near A | Extend update zone in `move()` (via `portalUpdateZone`) |
| Loop in collinear portal chain | Keep a `posList` in `buildPortalKey` to terminate window if a cell is visited twice. |
| Using `Pos::NONE` (0) as sentinel | Cell 0 is a valid top-left board cell! Use `Pos::PASS` (-1) instead. |
| Forgetting to fix WALL bitKey | Rapfi naturally makes empty cells 0b11. WALLs require `0b00`. You must flip both BLACK and WHITE bits in `newGame()` to set them correctly. |
