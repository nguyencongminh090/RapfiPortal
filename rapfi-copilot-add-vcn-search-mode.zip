# Protocol Pipeline Redesign — Perfect Portal Init

## Problem Analysis

The current implementation has **one key flaw**: `board` and `pendingPortals` can be
out of sync. After `START`, the board has NO portals even though portals were previously
registered. After `INFO YXPORTAL`, the board is NOT updated.

### All broken paths found:

| Command sequence | Current behaviour | Expected |
|---|---|---|
| `START 15` → `INFO YXPORTAL` → `BEGIN` | Searches with NO portals ❌ | Portals active ✅ |
| `INFO RULE 1` (rule change) | `board->newGame()` — portals lost ❌ | Re-apply portals with new rule ✅ |
| `INFO CAUTION_FACTOR N` | Recreates `board`, `board->newGame()` — portals lost ❌ | Re-apply portals after recreation ✅ |
| `BOARD x,y,1 ... DONE` | `board->newGame()` at top — portals lost ❌ | Apply portals before replay ✅ |

---

## The Design Invariant

> **After ANY command completes, `board` is ALWAYS synchronized with `pendingPortals`.**

One function enforces this invariant everywhere:

```cpp
/// PORTAL: Sync board topology with pendingPortals and reinitialize.
/// Clears board stones, applies all WALLs and portals, and calls newGame().
/// Must be called instead of bare board->newGame() whenever the board is reset.
void applyAndReinit()
{
    if (!board) return;
    board->clearPortals();
    for (Pos w : pendingPortals.walls)
        board->addWall(w);
    for (auto& [a, b] : pendingPortals.pairs)
        board->addPortal(a, b);
    board->newGame(options.rule);
    Search::Threads.clear(false);
}
```

---

## Answer: Does START/RESTART clear portals?

```
START (different board size)   → clears pendingPortals (coords invalid) + applyAndReinit()
START (same board size)        → does NOT clear pendingPortals            + applyAndReinit()
RESTART                        → does NOT clear pendingPortals            + applyAndReinit()
INFO CLEARPORTALS              → clears pendingPortals                    + applyAndReinit()
```

### Why `START` should NOT always clear portals:

If the UI sends:
```
INFO YXPORTAL 3,6 7,3
START 15
BEGIN
```

With the old design (`START` always clears), portals are lost. With the new design,
`START 15` finds `pendingPortals` non-empty, calls `applyAndReinit()`, and the board
is immediately ready with portals. **No RESTART needed.**

---

## Init Pipeline (New Design)

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                       UI sends commands                                     │
└─────────────────────────────────────────────────────────────────────────────┘

Scenario A: Portals configured BEFORE START (pre-configuration style)
─────────────────────────────────────────────────────────────────────
UI: INFO YXPORTAL 3,6 7,3    → pendingPortals.pairs.push({A,B})   [board=null, ok]
UI: INFO WALL 7,7             → pendingPortals.walls.push(W)       [board=null, ok]
UI: START 15                  → board = new Board(15)
                                applyAndReinit()
                                  clearPortals()
                                  addPortal(A, B)
                                  addWall(W)
                                  newGame()  ← initPortals() runs ✓
                              → "OK"
UI: BEGIN                     → think() ✓  (board has portals ready)


Scenario B: Portals configured AFTER START (standard style)
────────────────────────────────────────────────────────────
UI: START 15                  → board = new Board(15)
                                applyAndReinit() [with empty pending]
                                newGame()  ← no portals ✓
                              → "OK"
UI: INFO YXPORTAL 3,6 7,3    → pendingPortals.pairs.push({A,B})
                                applyAndReinit()   ← immediate sync!
                                newGame()  ← portals initialized ✓
UI: INFO WALL 7,7             → pendingPortals.walls.push(W)
                                applyAndReinit()   ← immediate sync!
UI: BEGIN                     → think() ✓  (no RESTART needed!)


Scenario C: Rule change mid-session
────────────────────────────────────
UI: START 15
UI: INFO YXPORTAL 3,6 7,3    → board synced with portals ✓
... moves played ...
UI: INFO RULE 1               → options.rule = STANDARD
                                applyAndReinit()   ← re-applies portals with new rule
                                board now has portals + new rule ✓


Scenario D: BOARD command with inline WALLs
───────────────────────────────────────────
UI: BOARD
UI: 3,6,3 7,3,3              → collect WALLs → pendingPortals.walls.push(...)
UI: 5,5,1  7,7,2             → collect stone moves
UI: DONE
→ applyAndReinit() first     ← portals + collected WALLs applied
→ board->newGame()
→ replay stone moves         ✓


Scenario E: RESTART (same portals, reset stones)
─────────────────────────────────────────────────
... game in progress with portals ...
UI: RESTART                   → applyAndReinit()  ← re-applies SAME portals (not cleared)
                              → "OK"  (portals preserved, stones cleared)


Scenario F: New match, new board (bigger/different size)
──────────────────────────────────────────────────────────
UI: START 19                  → old board size was 15 → size changed
                                pendingPortals.clear()  ← clear (coords invalid for 19x19)
                                board = new Board(19)
                                applyAndReinit()  [with empty pending]
                              → "OK"
```

---

## Proposed Changes to `gomocup.cpp`

### [1] New `applyAndReinit()` — replaces `applyPendingPortals()` in all reset paths

```cpp
// PORTAL: Single sync function. Always call instead of bare board->newGame().
void applyAndReinit()
{
    if (!board) return;
    board->clearPortals();
    for (Pos w : pendingPortals.walls)
        board->addWall(w);
    for (auto& [a, b] : pendingPortals.pairs)
        board->addPortal(a, b);
    board->newGame(options.rule);
}
```

### [2] `INFO YXPORTAL` & `INFO WALL` — immediate sync

```cpp
else if (token == "YXPORTAL") {
    ...parse a, b...
    pendingPortals.pairs.emplace_back(a, b);
    applyAndReinit();  // ← immediate: board always in sync
}
else if (token == "WALL") {
    ...parse pos...
    pendingPortals.walls.push_back(pos);
    applyAndReinit();  // ← immediate
}
else if (token == "CLEARPORTALS") {
    pendingPortals.clear();
    applyAndReinit();  // ← resets to no portals
}
```

### [3] `INFO RULE` — re-apply portals with new rule

```cpp
if (board && prevRule != options.rule) {
    applyAndReinit();         // ← was: board->newGame(options.rule)
    Search::Threads.clear(true);
}
```

### [4] `INFO CAUTION_FACTOR` — re-apply portals after board recreation

```cpp
board = std::make_unique<Board>(board->size(), *candRange);
applyAndReinit();              // ← was: board->newGame(options.rule), then re-play
for (Pos p : tempPosition)
    board->move(options.rule, p);
Search::Threads.clear(true);
```

### [5] `start()` — clear portals only on size change

```cpp
void start()
{
    int boardSize;
    std::cin >> boardSize;
    if (boardSize < 5 || boardSize > MAX_BOARD_SIZE) {
        ERRORL("Unsupported board size!"); return;
    }

    if (!board || boardSize != board->size()) {
        // New board size — old coordinates may be invalid
        if (board && boardSize != board->size()) {
            MESSAGEL("Board size changed, clearing portal config.");
            pendingPortals.clear();
        }
        board = std::make_unique<Board>(boardSize, candRange.value_or(Config::DefaultCandidateRange));
    }

    restart();  // restart calls applyAndReinit() + "OK"
}
```

### [6] `restart()` — call `applyAndReinit()` and print OK

```cpp
void restart()
{
    applyAndReinit();           // PORTAL: always sync before reset
    Search::Threads.clear(false);
    std::cout << "OK" << std::endl;
}
```

### [7] `getPosition()` (BOARD/YXBOARD) — apply portals before replaying

```cpp
void getPosition(bool startThink)
{
    // PORTAL: apply pending portals BEFORE newGame so initPortals() sees them
    applyAndReinit();   // ← replaces bare board->newGame() at top
    ...read position...
    // Collect WALLs from inline position entries into pendingPortals
    for (auto [pos, side] : position) {
        if (side == WALL) {
            pendingPortals.walls.push_back(pos);
            continue;
        }
        ...replay moves...
    }
    // If inline WALLs were added, re-sync board
    if (hasInlineWalls) {
        applyAndReinit();
        // replay all non-WALL moves again
        ...
    }
    if (startThink) think(*board);
}
```

> [!NOTE]
> The double-replay for inline WALLs in `BOARD` (current code) is preserved but
> restructured so `applyAndReinit()` is the single call point.

---

## Verification Plan

```bash
# Build
cd portal_build && make portal-engine portal-test -j$(nproc)

# Test: portals set before START
printf "INFO YXPORTAL 3,6 7,3\nSTART 15\nBEGIN\nEND\n" | ./portal-engine

# Test: portals set after START, no RESTART needed
printf "START 15\nINFO YXPORTAL 3,6 7,3\nINFO WALL 7,7\nBEGIN\nEND\n" | ./portal-engine

# Test: rule change preserves portals
printf "START 15\nINFO YXPORTAL 3,6 7,3\nINFO RULE 1\nBEGIN\nEND\n" | ./portal-engine

# Test: size change clears portals
printf "INFO YXPORTAL 3,6 7,3\nSTART 15\nSTART 19\nBEGIN\nEND\n" | ./portal-engine

# Run full unit test suite (must stay 56/56)
./portal-test
```
