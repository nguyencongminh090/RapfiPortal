# Pattern Detection Test Suite — Implementation Plan

## Goal

Create `portal_src/test_pattern.cpp` — a standalone test file that verifies all
**Pattern** and **Pattern2x** values from `src/game/pattern.h` work correctly,
in both **normal** (no portal) and **portal** scenarios.

---

## Key Encoding (Bit Layout)

From `board.h` and `pattern.h`:

```
// Bit encoding for each cell in a 64-bit key:
//   EMPTY = 0b11,  BLACK = 0b10,  WHITE = 0b01,  WALL = 0b00
//
// FREESTYLE: HalfLineLen=4, window = 9 cells (4 left + center + 4 right)
//            KeyLen = 16 bits, center at bits [8:9]
// STANDARD:  HalfLineLen=5, window = 11 cells
//            KeyLen = 20 bits, center at bits [10:11]
//
// fuseKey<R>() removes the center 2 bits before table lookup.
// lookupPattern<R>(key) → Pattern2x {patBlack, patWhite}
```

---

## Test Strategy

### Phase 1 — Normal Pattern Tests (no portals)

Test by **placing stones on a plain board** and **reading `cell.pattern[side]`
from board after `move()` updates the pattern pipeline**.

Each test scenario:
- Lay out a known position
- Check cell `.pattern[side]` at a specific position/direction
- Check `cell.pattern4[side]` (Pattern4) value
- Check `board.p4Count(side, X)` counter

#### All 14 `Pattern` values to cover:

| Pattern | Name | Description | Test layout |
|---------|------|-------------|-------------|
| DEAD | `X_.__X` | Fully blocked — no threat | WALL·_·WALL or O·_·O |
| OL | `OO.OOO` | One away from overline | 5-FREESTYLE empty cell in middle of 6 |
| B1 | `X.____` | One BLACK in half-line, blocked | 1 stone near WALL |
| F1 | `X_.___` | One BLACK open | 1 stone one side open |
| B2 | `XO.___` | Two BLACK, one blocked | 2 stones near WALL |
| F2 | `_O__._ ` | Open two (spread) | 2 stones one gap apart |
| F2A | `_O_.__` | Open two (closer) | 2 stones adjacent, open ends |
| F2B | `_O.___` | Open two (adjacent) | 2 adjacent, end closed |
| B3 | `XOO.__` | Three, one blocked | 3 stones near WALL |
| F3 | `_OO_._` | Open three | 3 contiguous, open |
| F3S | `__OO.__` | Open three spaced | 3 stones with gap |
| B4 | `XOOO._X` | Four, both ends near blocked | 4 stones one end WALL |
| F4 | `_OOO._` | Open four | 4 contiguous, one open end |
| F5 | `XOOOOOX` | Five | 5 stones → win |

> Note: OL only matters for STANDARD/RENJU (overline). In FREESTYLE, 6-in-a-row is also F5.

#### Pattern2x Tests:
- For each layout, verify **both** BLACK and WHITE entries of the Pattern2x are correct
- Symmetry: swapping stone colors → swapped Pattern2x

---

### Phase 2 — Portal Pattern Tests

Test that the **same 14 patterns** are correctly detected **across a portal pair**.

Key scenarios:

| Test | Portal type | Pattern expected | Description |
|------|------------|-----------------|-------------|
| P-F5-H | Horizontal portal | F5 | 3 BLACK + portal + 2 BLACK = five |
| P-F5-V | Vertical portal | F5 | 3 + portal + 2 (vertical) |
| P-B4 | Horizontal portal | B4 | 4 stones, exit end near WALL |
| P-F4 | Horizontal portal | F4 | 4 stones, exit end open |
| P-F3 | Horizontal portal | F3 | 3 stones through portal |
| P-DEAD | Horizontal portal | DEAD | Line blocked on both sides by WALL |
| P-WALL-BLOCK | Horizontal portal | DEAD at gap | WALL between two stones → no line |
| P-DUAL | Two portal pairs | F5 | Five using both portals (chain) |

---

## Test File Structure

```
portal_src/test_pattern.cpp
├── #include headers (board.h, pattern.h, config.h, types.h)
├── Helper: keyForLine() — build key from a string like "XX.XX" → uint64_t
├── Helper: patternName()  — Pattern → string for printing
├── Helper: pattern4Name() — Pattern4 → string
├── Helper: checkPat()     — compare expected vs actual, print result
│
├── Phase 1 — Normal (plain board via board.move())
│   ├── test_pat_dead()    — DEAD: surrounded by WALL
│   ├── test_pat_b1()      — B1: 1 stone, one end blocked
│   ├── test_pat_f1()      — F1: 1 stone, open
│   ├── test_pat_b2()      — B2: 2 stones, one end blocked
│   ├── test_pat_f2()      — F2/F2A/F2B variants
│   ├── test_pat_b3()      — B3: 3 stones, one end blocked
│   ├── test_pat_f3()      — F3/F3S variants
│   ├── test_pat_b4()      — B4: 4 stones, one end near WALL
│   ├── test_pat_f4()      — F4: 4 stones, open
│   ├── test_pat_f5()      — F5: 5 stones
│   ├── test_pat_ol()      — OL: 6-in-a-row center (STANDARD only)
│   └── test_pat_symmetry()— Pattern2x: same position, verify both colors
│
├── Phase 2 — Portal
│   ├── test_portal_f5_horizontal()
│   ├── test_portal_f5_vertical()
│   ├── test_portal_b4()
│   ├── test_portal_f4()
│   ├── test_portal_f3()
│   ├── test_portal_wall_blocks()
│   └── test_portal_pattern2x_symmetry()
│
└── main()
    ├── Config::loadConfig(InternalConfig)
    ├── Run all tests
    └── Print RESULTS: N passed, M failed
```

---

## Key Implementation Notes

1. **Access `cell.pattern[dir]`** — `board.cell(pos).pattern[dir]` gives the raw `Pattern`
   in each direction (`dir=0..3`). This is set by `updatePattern4AndScore()`.

2. **Access `cell.pattern4`** — `board.cell(pos).pattern4[BLACK]` gives the combined
   `Pattern4` for that empty cell if BLACK plays there.

3. **WALL placement** — Use `board.addWall(pos)` before `newGame()` to create WALL
   boundaries. This directly limits patterns.

4. **Direction encoding** — dir: 0=H, 1=V, 2=\, 3=/

5. **Don't check pattern on OCCUPIED cells** — `pattern[]` is only meaningful for EMPTY cells.
   Check the cell *adjacent* to the last stone placed, or an empty cell between stones.

6. **OL pattern** — Only exists in STANDARD / RENJU tables. Skip in FREESTYLE.

7. **CMake target** — add `portal-pattern-test` target alongside `portal-test` in CMakeLists.txt.

---

## Build Integration

Add to `portal_build/CMakeLists.txt`:

```cmake
add_executable(portal-pattern-test ../portal_src/test_pattern.cpp ${PORTAL_SOURCES})
target_compile_options(portal-pattern-test PRIVATE ${PORTAL_FLAGS})
target_link_libraries(portal-pattern-test lz4)
```

---

## Verification Plan

```bash
cd portal_build
make portal-pattern-test -j$(nproc)
./portal-pattern-test
```

Expected: all tests pass with 0 failures.
