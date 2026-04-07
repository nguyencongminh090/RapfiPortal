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
  - Portal (A, B) is **active** for direction D **only if** A and B are collinear in direction D.
  - When scanning direction D and you reach portal A → **skip A, skip B**, continue from the cell after B in direction D. Same for B → skip A.
  - Portal cells are **zero-width** in the virtual line window: they do NOT contribute bits.
  - For all OTHER directions: portal cells act exactly as WALL (line is blocked).

### Visual Examples (10x10 board)
```
Horizontal portal WIN (A and B on same row):
  Physical:  .  X  X  X [A] .  . [B]  X  *
  Virtual:   .  X  X  X  X  X  .        = 5-in-a-row → WIN

Diagonal portal WIN (A and B on same \ diagonal):
  Physical (diagonal view): (1,1)X (2,2)X [A=(3,3)] [B=(6,6)] (7,7)X (8,8)X ★(9,9)
  Virtual: X X X X X → WIN

Non-aligned direction = WALL:
  X  X [A] X  X   (A's partner B is on different row)
  →  X  X [WALL] X  X   (blocked)
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

### Portal Struct (to add in board.h)
```cpp
/// Portal pair: two immovable cells that teleport lines in one direction.
/// dir = precomputed direction index (0=H, 1=V, 2=\, 3=/) or -1 if not aligned.
struct Portal {
    Pos a;    ///< First portal cell position
    Pos b;    ///< Second portal cell position
    int dir;  ///< Aligned direction: 0=H 1=V 2=diag 3=antidiag, -1=invalid
};
```

### Portal Direction Computation
```cpp
/// Returns the direction index (0-3) if a and b are collinear, else -1.
/// 0=Horizontal(—), 1=Vertical(|), 2=Diagonal(\), 3=AntiDiag(/)
constexpr int portalDirOf(Pos a, Pos b) {
    int dx = b.x() - a.x(), dy = b.y() - a.y();
    if (dy == 0 && dx != 0)      return 0;  // same row
    if (dx == 0 && dy != 0)      return 1;  // same col
    if (dx == dy && dx != 0)     return 2;  // diagonal
    if (dx == -dy && dx != 0)    return 3;  // anti-diagonal
    return -1;
}
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
```
getKeyAt(pos, dir):
  // Fast path: no portal in range — use existing bitKey rotation
  if (!anyPortalNearby(pos, dir)):
      return fastBitKeyExtract(pos, dir)   // unchanged Rapfi code

  // Slow path: build virtual window manually
  return buildPortalKey(pos, dir)

buildPortalKey(pos, dir):
  walk = pos
  // Step backward L times (skipping portal cells)
  for i in 1..L:
      walk = portalStep(walk, dir, -1)

  key = 0
  // Walk forward 2L+1 steps, collecting bits
  for i in 0..(2*L):
      key |= colorBits(get(walk)) << (2*i)
      walk = portalStep(walk, dir, +1)
  return key

portalStep(cur, dir, sign):
  next = cur + DIRECTION[dir] * sign
  for each portal (A, B) with portal.dir == dir:
      if next == A: return B + DIRECTION[dir] * sign  // skip A, skip B
      if next == B: return A + DIRECTION[dir] * sign  // skip B, skip A
  return next  // no portal — normal step

colorBits(pos):
  piece = cells[pos].piece
  if piece == EMPTY: return 0b00
  if piece == BLACK: return 0b01
  if piece == WHITE: return 0b10
  // WALL or portal cell (should never appear — already skipped): return 0b11
  return 0b11
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
// 1. Mark portal cells as WALL in cells[] and bitKeys[]
for each portal pair (A, B):
    cells[A].piece = WALL   // already done since portals are WALLs
    cells[B].piece = WALL
    // The bitKey for A and B already has WALL bits (11) from boundary init
    // No additional bitKey change needed for non-aligned directions

// 2. Pre-compute portal direction
for each portal pair:
    portal.dir = portalDirOf(portal.a, portal.b)

// 3. Pattern init
// FOR_EVERY_POSITION: compute getKeyAt() for each empty cell
// Portal cells are WALL → they are skipped by FOR_EVERY_POSITION
// Cells near portals will naturally use buildPortalKey() via getKeyAt()
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
| Forgetting to update B's neighborhood when stone placed near A | Extend update zone in `move()` |
| Portal cells contributing WALL bits to aligned direction | Must use buildPortalKey() which SKIPS portal cells |
| Scanning past the other end of portal into board boundary | portalStep() must check boundary after teleport |
| Two portals chaining (A→B→C→D) | Walk loop handles naturally — each step checks ALL portals |
| Portal with dir=-1 (A and B not aligned) | Both cells act purely as WALL — no special handling needed |
