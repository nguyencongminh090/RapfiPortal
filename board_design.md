# Board Structure Design — Portal Gomoku Engine

## 1. Current Rapfi Architecture Summary

### Key insight: bitKey is a flat bitboard per line

```
bitKey0[y] = uint64_t  →  encodes entire ROW y
                          bit 2*x+1:2*x = {00=empty, 01=black, 10=white, 11=wall}

getKeyAt<R>(pos, dir=0):
  return rotr(bitKey0[y], 2*(x - L))
  → extracts 2*(2L+1) bits centered at (x,y) along row
  → lookupPattern<R>(key) → Pattern2x
```

This is **extremely fast** — one shift + one table lookup per direction. No loops, no branches.

### The problem for portals

`bitKey0[y]` stores cells contiguously along row y. A portal at `(4,5)↔(7,5)` means:
- The virtual line at cell `(3,5)` in horizontal dir is: `...(2,5), (3,5), [skip A], [skip B], (8,5), (9,5)...`
- But `rotr(bitKey0[5], ...)` gives **physical** neighbors: `(2,5), (3,5), (4,5)=WALL, (5,5), (6,5)...`

We cannot fix this with just bitKey manipulation. We need an **alternate path for portal-affected cells**.

---

## 2. Design Strategy: Dual-Path Architecture

```
                      ┌─ Fast path: rotr(bitKey[dir], ...) → UNCHANGED Rapfi
getKeyAt<R>(pos, dir)─┤
                      └─ Slow path: buildPortalKey(pos, dir) → NEW portal-aware
```

**Decision criterion**: Is the 9-cell window (or 11-cell for STANDARD/RENJU) of `(pos, dir)` affected by any portal?

### Pre-computed bitmask: `portalAffected[dir][pos]`

At `newGame()`, for each portal pair, for the aligned direction, mark every cell within `HalfLineLen` steps (through portal) as "portal-affected". This is a **one-time O(portals × L)** computation.

```
Storage: bool portalAffected[4][FULL_BOARD_CELL_COUNT]  ← 4 KB total
Or more compact: uint64_t portalAffectedMask[4][FULL_BOARD_SIZE]  ← bit per cell per line
```

---

## 3. New Data Structures

### Portal Pair

```cpp
/// PortalPair represents two connected portal cells.
///
///   ┌─────────────────────────────────────────────┐
///   │  a     : Pos    — first portal cell          │
///   │  b     : Pos    — second portal cell         │
///   │  dir   : int8_t — aligned direction (0-3)    │
///   │                    0=H(—) 1=V(|) 2=D(\) 3=A(/)│
///   │                    -1 if A,B not aligned     │
///   │  gap   : int8_t — physical steps from A to B │
///   │                    along aligned direction    │
///   └─────────────────────────────────────────────┘
///
/// Portal cells are stored as piece=WALL in cells[].
/// In the aligned direction (dir), they act as zero-width transparent cells.
/// In all other directions, they act as standard WALL.
struct PortalPair {
    Pos    a;          ///< First portal endpoint
    Pos    b;          ///< Second portal endpoint (b > a along dir)
    int8_t dir;        ///< Aligned direction index, or -1 if invalid
    int8_t gap;        ///< Physical cell distance from a to b along dir

    /// Check if this portal is valid (endpoints are aligned in some direction).
    [[nodiscard]] constexpr bool isValid() const { return dir >= 0; }
};
```

### Portal Lookup Table (per-cell, per-direction)

```cpp
/// PortalLink encodes what happens when a line scan reaches a portal cell.
///
///   ┌───────────────────────────────────────────────┐
///   │  exitPos : Pos    — where to continue after   │
///   │                      skipping both portals    │
///   │                      (= cell after the OTHER  │
///   │                      portal in scan direction)│
///   │  partner : Pos    — the other portal cell     │
///   └───────────────────────────────────────────────┘
///
/// Stored in portalLinkTable[dir][Pos] for O(1) lookup during line walk.
/// Only populated for portal cells in their aligned direction.
/// For non-portal cells or non-aligned directions: exitPos = Pos::NONE.
struct PortalLink {
    Pos exitPos;       ///< Cell after the partner portal (in positive dir)
    Pos partner;       ///< The other portal in this pair
};
```

---

## 4. Board Class — Modified Members

```cpp
class Board {
    // ========== UNCHANGED from Rapfi ==========
    Cell cells[FULL_BOARD_CELL_COUNT];

    uint64_t bitKey0[FULL_BOARD_SIZE];
    uint64_t bitKey1[FULL_BOARD_SIZE];
    uint64_t bitKey2[FULL_BOARD_SIZE * 2 - 1];
    uint64_t bitKey3[FULL_BOARD_SIZE * 2 - 1];

    int          boardSize;
    int          boardCellCount;
    int          moveCount;
    int          passCount[SIDE_NB];
    Color        currentSide;
    HashKey      currentZobristKey;
    StateInfo   *stateInfos;
    UpdateCache *updateCache;
    // ... candidateRange, evaluator, thread pointers ...

    // ========== NEW: Portal data ==========

    /// PORTAL: Static wall positions (set once at newGame, never change)
    /// Stored separately for protocol/setup purposes.
    /// In cells[], walls are already piece=WALL.
    std::vector<Pos> wallPositions;

    /// PORTAL: Portal pair definitions (max 4 pairs for reasonable gameplay)
    static constexpr int MAX_PORTAL_PAIRS = 4;
    int        portalCount = 0;
    PortalPair portals[MAX_PORTAL_PAIRS];

    /// PORTAL: Per-direction portal lookup for O(1) access during line walk.
    /// portalLink[dir][pos] → PortalLink to skip through portal.
    /// If portalLink[dir][pos].exitPos == Pos::NONE → no portal here in this dir.
    PortalLink portalLink[4][FULL_BOARD_CELL_COUNT];

    /// PORTAL: Per-direction bitmask of cells whose pattern window touches a portal.
    /// If bit is set → must use slow path (buildPortalKey) instead of bitKey rotation.
    /// Indexed as: portalAffected[dir][pos] (flat bool array).
    bool portalAffected[4][FULL_BOARD_CELL_COUNT];

    /// PORTAL: Extended update zone — cells near portal B that also need refresh
    /// when a stone is placed near portal A (and vice versa).
    /// Built once at newGame().
    /// Key: portal pair index. Value: list of extra cells to update per direction.
    struct PortalUpdateZone {
        Pos cells[20];     ///< Extra cells to update (at most ~2*L per pair)
        int count = 0;
    };
    PortalUpdateZone portalUpdateZones[MAX_PORTAL_PAIRS][4];

    // ========== NEW: Methods ==========

    /// PORTAL: Build the virtual line key for a portal-affected cell.
    /// Walks the line through portal teleportation, collecting color bits.
    template <Rule R>
    uint64_t buildPortalKey(Pos pos, int dir) const;

    /// PORTAL: Step one cell along direction, handling portal teleportation.
    /// If next cell is a portal aligned in this direction, skip both portals.
    Pos portalStep(Pos cur, int dir, int sign) const;

    /// PORTAL: Initialize all portal data structures at newGame().
    void initPortals();

    /// PORTAL: Compute which cells are portal-affected per direction.
    template <Rule R>
    void computePortalAffectedMask();
};
```

---

## 5. Modified `getKeyAt<R>()` — The Core Change

```cpp
template <Rule R>
inline uint64_t Board::getKeyAt(Pos pos, int dir) const
{
    assert(dir >= 0 && dir < 4);

    // PORTAL: Check if this cell's pattern window is affected by a portal
    if (portalAffected[dir][pos]) {
        return buildPortalKey<R>(pos, dir);
    }

    // Fast path — unchanged from Rapfi
    constexpr int L = PatternConfig::HalfLineLen<R>;
    int x = pos.x() + BOARD_BOUNDARY;
    int y = pos.y() + BOARD_BOUNDARY;

    switch (dir) {
    default:
    case 0: return rotr(bitKey0[y], 2 * (x - L));
    case 1: return rotr(bitKey1[x], 2 * (y - L));
    case 2: return rotr(bitKey2[x + y], 2 * (x - L));
    case 3: return rotr(bitKey3[FULL_BOARD_SIZE - 1 - x + y], 2 * (x - L));
    }
}
```

---

## 6. `buildPortalKey<R>()` — Virtual Line Walk

```cpp
/// Build a pattern key by walking the virtual line through portals.
/// The result is equivalent to what getKeyAt would return if portals
/// were zero-width transparent cells.
///
/// Walk algorithm:
///   1. Start at pos, step backward L times (handling portal skips)
///   2. Collect 2L+1 color-bit pairs by stepping forward (handling portal skips)
///   3. The center cell (pos itself) is included — fuseKey() removes it later
///
/// Portal skip: when a step lands on portal cell P (aligned in this dir),
///   → jump to P's partner Q, then one more step in the same direction
///   → P and Q contribute NO bits to the key (zero-width)
///
/// Edge case: if after teleport, the next cell is out of bounds → WALL bits (0b11)
template <Rule R>
uint64_t Board::buildPortalKey(Pos pos, int dir) const
{
    constexpr int L = PatternConfig::HalfLineLen<R>;
    constexpr int WindowLen = 2 * L + 1;  // 9 for FREESTYLE, 11 for STANDARD/RENJU

    Direction step = DIRECTION[dir];
    uint64_t key = 0;

    // Find the starting position: L steps backward from pos
    Pos cur = pos;
    for (int i = 0; i < L; i++) {
        cur = portalStep(cur, dir, -1);
    }

    // Collect WindowLen color bits
    for (int i = 0; i < WindowLen; i++) {
        // Read color at cur → encode to 2 bits
        Color c = (cur >= 0 && cur < FULL_BOARD_CELL_COUNT) ? cells[cur].piece : WALL;
        uint64_t bits;
        switch (c) {
            case EMPTY: bits = 0b00; break;
            case BLACK: bits = 0b01; break;
            case WHITE: bits = 0b10; break;
            default:    bits = 0b11; break;  // WALL
        }
        key |= bits << (2 * i);

        if (i < WindowLen - 1) {
            cur = portalStep(cur, dir, +1);
        }
    }

    return key;
}
```

---

## 7. `portalStep()` — Single Step with Portal Teleportation

```cpp
/// Take one step in direction dir (forward if sign=+1, backward if sign=-1).
/// If the next cell is a portal aligned in this direction:
///   → Skip both portal cells (zero-width)
///   → Return the cell after the partner portal
///
/// Example (dir=HORIZONTAL, sign=+1):
///   cur = (3,5), portal A=(4,5), B=(7,5)
///   next = (4,5) = portal A → skip A, skip B → return (8,5)
///
/// If no portal: returns cur + step (normal behavior)
inline Pos Board::portalStep(Pos cur, int dir, int sign) const
{
    Direction step = DIRECTION[dir] * sign;
    Pos next = cur + step;

    // Check if next is a portal cell aligned in this direction
    const PortalLink& link = portalLink[dir][next];
    if (link.exitPos != Pos::NONE) {
        // Teleport: skip both portal A and B, land on cell after partner
        return (sign > 0) ? link.exitPos : (link.partner - step);
        // Note: for backward walk, exitPos was computed for forward direction,
        // so we use partner - step instead
    }

    return next;
}
```

> [!IMPORTANT]
> The backward-walk case needs care. `exitPos` is pre-computed for the *positive* direction. For backward walk, we use `partner + DIRECTION[dir] * (-1)` instead.

Actually, let me redesign this more cleanly:

```cpp
/// PORTAL: PortalLink stores BOTH forward and backward exit positions.
struct PortalLink {
    Pos fwdExit;   ///< Cell after partner portal, going FORWARD (+dir)
    Pos bwdExit;   ///< Cell after partner portal, going BACKWARD (-dir)
    Pos partner;   ///< The other portal in this pair
};
```

Then `portalStep` becomes trivially simple:

```cpp
inline Pos Board::portalStep(Pos cur, int dir, int sign) const
{
    Pos next = cur + DIRECTION[dir] * sign;

    const PortalLink& link = portalLink[dir][next];
    if (link.fwdExit != Pos::NONE) {
        return (sign > 0) ? link.fwdExit : link.bwdExit;
    }

    return next;
}
```

---

## 8. `initPortals()` — One-Time Setup at `newGame()`

```
initPortals():
  // 1. Clear all portal data
  memset(portalLink, 0, sizeof(portalLink))  // all exitPos = Pos::NONE
  memset(portalAffected, 0, sizeof(portalAffected))

  // 2. Place WALL cells on board
  for each wallPos in wallPositions:
      cells[wallPos].piece = WALL
      // bitKey already has WALL bits from boundary init

  // 3. Place portal cells as WALL + compute direction
  for i = 0..portalCount:
      PortalPair& p = portals[i]
      cells[p.a].piece = WALL
      cells[p.b].piece = WALL
      p.dir = computePortalDir(p.a, p.b)
      // bitKeys: portal cells get WALL bits in ALL 4 directions
      // This is correct for 3 non-aligned directions
      // For the aligned direction, getKeyAt() will use buildPortalKey() instead

  // 4. Build portalLink table for O(1) lookup
  for i = 0..portalCount:
      if portals[i].dir < 0: continue  // not aligned
      int dir = portals[i].dir
      Direction step = DIRECTION[dir]

      // For portal A: hitting A means teleport to B+step / B-step
      portalLink[dir][portals[i].a] = {
          .fwdExit = portals[i].b + step,    // forward: land after B
          .bwdExit = portals[i].b - step,    // backward: land before B (from A's perspective)
          .partner = portals[i].b
      }
      // For portal B: hitting B means teleport to A+step / A-step
      portalLink[dir][portals[i].b] = {
          .fwdExit = portals[i].a + step,
          .bwdExit = portals[i].a - step,
          .partner = portals[i].a
      }

  // 5. Compute portalAffected mask
  computePortalAffectedMask<R>()
```

---

## 9. `computePortalAffectedMask<R>()`

A cell `pos` is portal-affected in direction `dir` if **any portal cell** falls within its `[-L, +L]` line window (using virtual walking).

```
computePortalAffectedMask<R>():
  for each portal pair P where P.dir >= 0:
      int dir = P.dir
      Direction step = DIRECTION[dir]

      // Cells physically near portal A (within L steps)
      for i = -L .. +L:
          Pos target = P.a + step * i
          if target is valid empty cell:
              portalAffected[dir][target] = true

      // Cells physically near portal B (within L steps)
      for i = -L .. +L:
          Pos target = P.b + step * i
          if target is valid empty cell:
              portalAffected[dir][target] = true

      // ALSO: cells reachable THROUGH the portal within L steps
      // After teleporting from A, cells near B+step*1..B+step*L
      // have their window pass back through B→A
      // But these are already covered by "near B" above ✓
```

---

## 10. `move()` Changes — Extended Update Zone

```
Rapfi move() loop (simplified):
  for i = -L .. +L (skipping 0):
      for dir = 0..3:
          Pos posi = pos + DIRECTION[dir] * i
          if cells[posi].piece != EMPTY: continue
          cells[posi].pattern2x[dir] = lookupPattern<R>(getKeyAt<R>(posi, dir))
          // ... update p4count, score, etc

Portal extension — AFTER the normal loop:
  for each portal pair P where P.dir >= 0:
      int dir = P.dir
      // If pos is within L steps of P.a (in direction dir):
      //   → refresh all cells within L steps of P.b (in direction dir)
      // And vice versa
      if physicalDistAlongDir(pos, P.a, dir) <= L:
          refreshPortalNeighbors(P.b, dir)
      if physicalDistAlongDir(pos, P.b, dir) <= L:
          refreshPortalNeighbors(P.a, dir)
```

---

## 11. Performance Analysis

| Operation | Rapfi | Portal Engine | Overhead |
|-----------|-------|---------------|----------|
| `getKeyAt` (no portal nearby) | 1 shift | 1 branch + 1 shift | ~1 cycle |
| `getKeyAt` (portal nearby) | N/A | loop of ~9 steps | ~20-30 cycles |
| `move()` update loop | 4 × (2L+1) cells | same + portal extension | +0..2L cells per portal |
| `newGame()` init | O(boardSize²) | same + portal init | +O(portals × L) |
| Memory | ~36 KB cells + bitkeys | +20 KB portal tables | <5% overhead |

### Percentage of cells affected

On a 15×15 board with 1 portal pair:
- Portal-affected cells per direction: ~2 × (2×4+1) = ~18 cells
- Total empty cells: 225
- **Only ~8% of cells use slow path in one direction, 0% in other 3 directions**

---

## 12. Memory Layout Summary

```
Board object memory map:
  ┌─────────────────────────────────────┐
  │ cells[1024]              (20 KB)    │ Cell × FULL_BOARD_CELL_COUNT
  │ bitKey0..3               (~3. KB)    │ uint64_t × (32 + 32 + 63 + 63)
  │ ─── Rapfi original above ───        │
  │ portalLink[4][1024]      (24 KB)    │ PORTAL: PortalLink per dir per cell
  │ portalAffected[4][1024]  ( 4 KB)    │ PORTAL: bool per dir per cell
  │ portals[4]               (32 bytes) │ PORTAL: PortalPair definitions
  │ wallPositions             (dynamic) │ PORTAL: std::vector<Pos>
  │ portalUpdateZones         (640 B)   │ PORTAL: pre-computed update lists
  │ portalCount              (4 bytes)  │ PORTAL: number of active pairs
  └─────────────────────────────────────┘
  Total portal overhead: ~28 KB  (acceptable for board game engine)
```

---

## 13. Design Decisions & Rationale

| Decision | Rationale |
|----------|-----------|
| `portalLink[dir][pos]` flat table | O(1) lookup during line walk — critical for `move()` hot path |
| `portalAffected[dir][pos]` bool array | Branch prediction: >90% cells are NOT portal-affected → fast path dominates |
| `PortalLink` stores both fwd/bwd exit | Avoids direction arithmetic in the hot `portalStep()` function |
| Portal cells stored as `WALL` in `cells[]` | Reuses existing WALL handling for 3 non-aligned directions for free |
| `MAX_PORTAL_PAIRS = 4` | Reasonable gameplay limit; avoids dynamic allocation |
| Pre-computed `portalAffected` mask | One-time cost at `newGame()` vs per-move branching |

> [!WARNING]
> **Not yet addressed:** `undo()` must restore portal-extended cells exactly as `move()` modified them. The `UpdateCache` array size (`40`) might need increasing if portal extension adds many cells. Need to verify: `4 × (2L+1) + portal_extension ≤ array_size`.
