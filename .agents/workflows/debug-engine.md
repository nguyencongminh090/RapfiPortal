---
description: Debug incorrect pattern evaluation, wrong win detection, or WALL/Portal logic issues in portal_src
---

# Workflow: Debug Portal Engine Issues

// turbo-all

## Step 1 — Classify the Bug

Before doing anything, classify what's wrong:

| Symptom | Likely culprit |
|---------|----------------|
| Win not detected through portal | `getKeyAt()` / `buildPortalKey()` |
| Win detected that shouldn't be (false win) | Loop detection in `buildPortalKey()` |
| WALL doesn't block a line | `initIndexTable()` not using WALL distance |
| Cells on far side of WALL get wrong eval | `move()` update zone crossing WALL boundary |
| Patterns wrong near board edge but not WALL | Not a portal bug — likely existing Rapfi issue |
| Engine plays obviously wrong near WALLs | NNUE encoding bug in `Accumulator::initIndexTable()` |
| Engine plays well tactically but poor strategy | NNUE value head not adapted for WALL board |

## Step 2 — Reproduce in Test Binary

Always try to reproduce the issue in `portal-pattern-test` or `portal-test` first.
These binaries run in microseconds — far faster than debugging through a live game.

```bash
# Run the pattern test suite
./portal_src/build/portal-pattern-test

# Run the portal mechanics suite
./portal_src/build/portal-test
```

If the test binary doesn't cover your case, **add a test case to `test_pattern.cpp`
or `test_portal.cpp` before debugging** — this prevents the bug from regressing.

## Step 3 — Isolate the Position

Construct the minimal board position that triggers the bug:

```cpp
// Minimal test skeleton (add to test_portal.cpp or test_pattern.cpp)
{
    Board board(17);  // board size
    board.newGame(Rule::FREESTYLE);
    
    // Set WALLs: call board.addWall(Pos(x, y)) or equivalent
    // Set portals: call board.addPortal(Pos(ax, ay), Pos(bx, by))
    
    // Place stones
    board.move(Rule::FREESTYLE, Pos(x, y));  // BLACK
    board.move(Rule::FREESTYLE, Pos(x, y));  // WHITE
    // ...
    
    // Assert expected pattern
    auto key = board.getKeyAt<0>(Pos(cx, cy), 0);  // direction 0 = horizontal
    assert(key == expected_key && "WALL blocking failed");
}
```

## Step 4 — Check the Key Pipeline

For pattern/win bugs, trace the key computation chain:

1. **`getKeyAt(pos, dir)`** — does it use fast path or `buildPortalKey`?
   - Check `board.portalAffected[dir][pos]` — is it set correctly?
   - If using fast path where it shouldn't: WALLs/portals not registered in `newGame()`

2. **`buildPortalKey(pos, dir)`** — is the virtual window correct?
   - Add a debug print: list each physical cell in the window and its piece
   - Verify WALL cells produce `WALL` (0b00) in the key bits
   - Verify portal cells are skipped (zero-width)
   - Check loop detection: collinear portals must not produce infinite walks

3. **`PATTERN2x[key]`** — is the resulting pattern code correct?
   - Pattern enums: `DEAD(0)`, `F5(highest)` — check the value is sane

4. **`StateInfo::p4Count`** — is the win-condition counter correct?
   - After placing the 5th stone in a portal-connected line, `p4Count[color][A_FIVE]` must be ≥ 1

## Step 5 — Check Update Zone

If the bug only appears after several moves (not in a static position), the update zone
in `move()` is likely too narrow:

```cpp
// Suspect: WALL cells blocking the update sweep
// In Accumulator::move() — check whether the sweep stops at WALLs:
for (int i = 1; i <= half; i++) {
    Pos next = pos + DIRECTION[dir] * i;
    if (board->cell(next).piece == WALL) break;  // this break must be present
    // ... update next's patterns
}

// Suspect: portal extension zone missing
// After a stone near portal A, cells near portal B must also be updated
// Check: does move() call updateZoneAroundPortal()?
```

## Step 6 — Check NNUE Encoding

If the tactical logic is correct but evaluation near WALLs is obviously wrong (e.g.,
engine ignores a forced win), the NNUE input is corrupted:

```bash
# Add a debug mode to Accumulator to print indexTable values near a WALL
# Compare indexTable[cellNearWall][dir] vs indexTable[cellNearEdge][dir]
# They should differ — if they're identical, initIndexTable() hasn't been fixed
```

Verify in `Accumulator::initIndexTable()` (line ~255 in `mix9svqnnue.cpp`):
- The distance calculation must stop at WALL cells, NOT just board edges.
- Check: `wallAwareDist()` helper is called with the board pointer.

## Step 7 — Fix and Confirm

1. Make the minimal fix.
2. Add `// PORTAL: bugfix — [description]` comment on changed lines.
3. Rebuild: `ninja -C portal_src/build`
4. Re-run test binaries: `./portal_src/build/portal-pattern-test && ./portal_src/build/portal-test`
5. Confirm the specific minimal test case now passes.
6. Run a quick 5-move game via piskvork protocol to confirm no crash:
   ```
   START 17
   YXBOARD
   ... (WALL positions)
   DONE
   BEGIN
   ```
