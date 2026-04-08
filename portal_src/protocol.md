# Portal Gomoku Engine - Protocol Documentation

This document describes the communication protocol used by the Portal Gomoku Engine (a fork of Rapfi), explicitly designed for UI developers implementing frontend features for custom boards (Wall cells and Portals).

The engine communicates over Standard Input/Output (`stdin`/`stdout`), following an extended version of the standard [Gomocup / Piskvork protocol](http://gomocup.org/basic-protocol/).

---

## 1. Connection Pipeline & Handshake

When starting the engine, the UI must launch the executable and attach to its `stdin` and `stdout`.

### Initializing GUI Mode
Upon startup, the UI **must** immediately send:
```text
YXSHOWINFO
```
The engine will reply with version strings and engine configuration information, for example:
```text
MESSAGE MINT-P Rapfi 1.x.x
MESSAGE INFO MAX_THREAD_NUM 256
MESSAGE INFO MAX_HASH_SIZE 30
```
This command enables extended output and GUI mode features inside the engine.

---

## 2. Basic Gomocup Commands

These are standard Gomocup protocol commands essential for normal gameplay:

| Command | Engine Action | UI Usage |
| :--- | :--- | :--- |
| `START [size]` | Initializes a board of `size`x`size`. Maximum size is usually 20. | Send this when creating a new game. If `size` changes, all Portals and WALLs are cleared. |
| `RESTART` | Clears the board pieces and restarts the game with the same portal topology. Engine replies `OK`. | Send to replay a game on the exact same board setup. |
| `BEGIN` | Engine plays the first move on the empty board. | If playing as White and the engine is Black. |
| `TURN [x],[y]` | Opponent piece is placed at `[x],[y]`. The engine thinks and outputs its next move as `[A],[B]`. | Send this when the human user clicks a cell. |
| `TAKEBACK [x],[y]` | Undoes the last move. Engine replies `OK`. | Useful for undo buttons. It only undoes ONE ply. |
| `BOARD` | Followed by a list of `[x],[y],[color]` (color 1=SELF, 2=OPPO, 3=WALL) and terminated by `DONE`. Starts engine thinking. | Restoring a game from save or importing a position. |
| `YXBOARD` | Same as `BOARD`, but the engine does NOT automatically start thinking after `DONE`. | Use to set a board state silently. |
| `END` | Exits the engine. | Send before terminating the engine process. |

---

## 3. Extended Configuration / Portal Rules (IMPORTANT FOR UI)

The following parameters are passed through the standard `INFO` command. The format is always:
```text
INFO [KEY] [VALUE_OR_ARGS]
```

### 3.1. Setting up Custom Boards (Walls & Portals)

**IMPORTANT NOTE:**
The Engine uses `0`-based coordinates internally, so the top-left cell is `0,0`. Gomocup interface expects `x,y`. 
Whenever the UI configures a Portal or a Wall, the engine **immediately resets the game board** internally behind the scenes to apply the new topology (via `applyAndReinit()`). `RESTART` is not strictly necessary after setting portals, but it is highly recommended to follow a structured setup flow.

#### Add a Wall
```text
INFO WALL [x],[y]
```
Adds a single an immovable, line-blocking Wall at the specified coordinates.

#### Add a Portal Pair
```text
INFO YXPORTAL [Ax],[Ay] [Bx],[By]
```
Registers a two-way teleportation pair between `A` and `B`. Note the space between Point A and Point B.
- A line entering `A` exits from `B` in the exact same direction.
- Portals themselves are zero-width and unplayable (they function mostly like Walls for pieces, but strings pass through them seamlessly).

#### Clear All Board Modifications
```text
INFO CLEARPORTALS
```
Clears and resets the board. Removes all walls and portal pairs. The board goes back to a standard flat grid.

### 3.2. Example Pipeline for a Portal Game

If a UI wants to set up a new `15x15` game with 2 Walls and 1 Portal Pair:

**UI Sends:**
```text
YXSHOWINFO
START 15
INFO CLEARPORTALS
INFO WALL 3,3
INFO WALL 3,4
INFO YXPORTAL 5,5 10,10
```

*Game is now set up and empty. Human makes a move:*
**UI Sends:**
```text
TURN 7,7
```

*Engine thinks, and replies with its text:*
**Engine Replies:**
```text
8,8
```
*(Engine played at `8,8`)*

### 3.3. Inline WALLs via BOARD Command
As an alternative to `INFO WALL x,y`, walls can be loaded directly inside the `BOARD` replay block using color `3`.
```text
BOARD
7,7,1
8,8,2
2,3,3
...
DONE
```
Where `2,3,3` registers a WALL at `2,3`.

---

## 4. Other Useful Rapfi/Yixin Specific Parameters

You can adjust engine characteristics via `INFO`:

| Command | Effect |
| :--- | :--- |
| `INFO TIMEOUT_TURN [ms]` | Maximum time per move in milliseconds. |
| `INFO TIMEOUT_MATCH [ms]` | Maximum time for the entire game in milliseconds. |
| `INFO RULE [0/1/4/5/6]` | `0`: Freestyle, `1`: Standard (Free Open), `4`: Renju. |
| `INFO MAX_MEMORY [kb]` | Limit Engine Hash/Search memory in kilo-bytes. |
| `INFO THREAD_NUM [n]` | Set number of searchers/threads to use. |
| `INFO STRENGTH [0-100]` | Set difficulty strength (internal Rapfi param). |

## 5. UI Implementation Considerations & Edge Cases

1. **Wait State:** When you send `TURN X,Y` or `BEGIN`, the engine will block and think. During this time, wait for the engine to output `Ax,Ay` before allowing the human to move again.
2. **First Move Restrictions:** In a board with Portals or Walls, if the engine is forced to play the *very first move* (Ply 0), it will intentionally pick an empty cell **adjacent to an obstacle**. This is to prevent opening-book assertion crashes on zero-board situations.
3. **Interrupting:** If you need to stop the engine calculation midway through a turn, send `STOP`. The engine will halt processing and report its best move gathered so far. You can safely send `TAKEBACK` or a new command after.
4. **GUI Logging:** The engine frequently prints logs starting with `MESSAGE ...` or `DEBUG ...`. The UI backend parser should filter these and show them in an engine log console, keeping them strictly separated from positional return coordinates.
