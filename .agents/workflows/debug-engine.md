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
| WALL doesn't block a line | `getKeyAt()` bitKey not set for WALL in `newGame()` |
| Cells on far side of WALL get wrong eval | `move()` update zone in `board.cpp` crossing WALL boundary |
| Patterns wrong near board edge but not WALL | Not a portal bug — likely existing Rapfi issue |
| Engine plays strategically wrong near WALLs | Classical eval correction missing — use `/enhance-search` workflow |

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
in `board.cpp::move()` is likely too narrow:

```cpp
// In Board::move<R,MT>() — check whether the pattern update loop stops at WALLs:
// The loop that refreshes pattern4[] for affected cells must not cross WALL boundaries.
for (int i = 1; i <= HALF_LINE_LEN; i++) {
    Pos next = pos + DIRECTION[dir] * i;
    if (!board.isInBound(next) || board.cell(next).piece == WALL) break;
    // ... refresh next's pattern
}

// Portal extension zone:
// After a stone near portal A, cells near portal B must also be updated.
// Check: does move() iterate over portalUpdateZone[][] entries?
```

## Step 6 — Fix and Confirm

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
7. If the bug is strategic (not pattern/win detection) → use `/enhance-search` workflow instead.
