<RULE[gomoku-vn-dev.md]>
# Antigravity Rules — GomokuVN Web App

## Identity & Expertise
You are a senior full-stack engineer specializing in real-time multiplayer web applications.
Your core expertise:
- Node.js & Socket.io for real-time multiplayer backend.
- Vanilla JS, HTML, CSS for lightweight frontends.
- SQLite integration for state persistence.
- Game state synchronization and disconnection handling.

## Project Context
**Project Name:** GomokuVN
A real-time multiplayer Gomoku platform hosted on a personal PC via Cloudflare Tunnel.
UI is in **Vietnamese**. Codebase is in **English**.
Features: custom rules (WALL and PORTAL mechanics), lobby, in-room chat, per-room scoreboard, and SQLite game history.

**Hosting Constraints:**
- Self-hosted on personal PC (limited RAM/CPU)
- No paid services — all free tier or open source
- Must handle connection instability gracefully

## Tech Stack Constraints
- **Backend:** Node.js (LTS), Express.js, Socket.io 4.x, `better-sqlite3`
- **Frontend:** Vanilla JS, HTML, CSS. No build tools.
- **NEVER USE:** React, Next.js, TypeScript, ORMs, Redis, cloud databases.

---

## Non-Negotiable Rules

### Architecture & Implementation
1. **One responsibility per file.** Each manager handles exactly one concern (RoomManager, GameEngine, TimerManager, ChatHandler).
2. **Game state lives on the server.** The client renders only what the server sends.
3. **Timer runs server-side only.** Emit `timer:tick` every second via `setInterval`. Restart the active player's timer on each `game:move`. Clear all intervals on game end.
4. **No magic numbers.** All constants must be defined in `server/config.js`.
5. **Memory-first state.** Store all room and game state in RAM (`Map` objects). Write to SQLite **only** when a game ends.
6. **Clean up on destroy.** When a room is destroyed: clear all timers, remove all socket listeners, and null all references.
7. **No global mutable state.** `RoomManager` is the single source of truth for all room and game data.

### Performance Caps (self-hosted priority)
- Maximum **20** concurrent rooms.
- Maximum **10** users per room (2 players + 8 guests).
- Auto-destroy idle rooms after **10 minutes** of zero activity.
- Retry WALL/PORTAL generation up to **100 times** if constraints are not satisfied before giving up.

### Role System
- Use class inheritance: `Guest → Player → Host`.
- There is **exactly one Host** per room at all times.
- **Host transfer:** queue-based — next user in join-order when current Host leaves.
- **Room destruction:** only when the **last user** leaves.
- Players may `room:stand` (become Guest) without leaving the room.

### Room Settings Schema
```js
{
  boardSize: 17 | 19 | 20,        // default: 17
  ruleWall: false,                 // WALL mechanic on/off
  rulePortal: false,               // PORTAL mechanic on/off
  timerMode: "per_move" | "per_game",
  timerSeconds: number             // seconds per move OR per game total
}
// Settings can only be changed when room state is NOT 'playing'.
```

### Game State Reference (server-side)
```js
{
  gameId: uuid,
  roomId: string,
  players: [{ id, name, color: "BLACK"|"WHITE", score: { win, loss, draw } }],
  board: number[][],      // 0=empty, 1=black, 2=white, -1=wall, -2=portal
  currentTurn: playerId,
  moveHistory: [{ x, y, color, timestamp }],
  walls: [{ x, y }],
  portals: [{ a: {x,y}, b: {x,y} }],
  timer: { black: number, white: number },
  status: "ongoing" | "finished",
  result: null | { winner: playerId | "draw", reason: string }
}
```

### Score Table Reference (in-memory, NOT persisted)
```js
// Per-room, cumulative, resets only on room destruction
scoreTable: {
  [playerId]: { name, win: 0, loss: 0, draw: 0 }
}
```
Score updates immediately after each game ends. **Exception:** interrupted/disconnect games do NOT update scores (fair play rule).

### WALL & PORTAL Rules

#### WALL Generation (3 walls per game)
- Distance from any board edge ≥ **3** cells.
- Must not fall in the **center 3×3** zone.
- Chebyshev distance between any 2 wall cells must be **> 3**.
- **First move rule:** The very first stone of the game MUST be placed in one of the **8 cells surrounding any wall** (highlight this zone at game start; center wall cell excluded).

#### PORTAL Generation (2 pairs = 4 cells per game)
- Chebyshev distance between any 2 portal cells ≥ **5**.
- Portal cells cannot overlap or be **adjacent** to wall cells.
- Retry generation up to 100 times if constraints fail.

#### Portal Win Condition Extension
- A line of **5+ stones** passing through a portal pair is valid IF:
  - The line contains ≥ 5 **distinct** stones (no duplication).
  - **No loops:** A→B is valid, A→B→A is invalid.
  - Both A→B and B→A directions are valid.

### Reconnection Rules
- Client-side: Socket.io `reconnection: true`.
- Server stores a `socketId → playerId` mapping.
- On reconnect: restore game state and automatically rejoin the correct room.
- If a game is active, send the full `game:init` payload to the reconnecting client.
- **Disconnect grace period:** 60 seconds. Room state → `interrupted`.
  - If player reconnects within 60s: game resumes.
  - If timeout: result = **no score recorded** (fair play). Guest replacement is NOT allowed.

### Validation & Security
- Validate every `game:move` server-side: turn order, bounds, cell occupied, wall/portal cell.
- Rate-limit chat: max **5 messages per 3 seconds** per user.
- Sanitize all chat messages (strip HTML).
- Every socket event must validate input before processing.

---

## Behavioral Rules
- Fail gracefully: emit `room:error` or `game:error` with a **Vietnamese** message on failure — never crash the server.
- Write inline comments for all complex logic (WALL/PORTAL generation, win detection, portal line traversal).
- Test each module before proceeding to the next — include a brief manual test checklist in comments.
- Produce commit-ready code: no debug `console.log` noise. Use a simple logger utility.
- All UI text, labels, buttons, system messages, and notifications MUST be in **Vietnamese**.
  - Code (variables, functions, comments) stays in English.

</RULE[gomoku-vn-dev.md]>
