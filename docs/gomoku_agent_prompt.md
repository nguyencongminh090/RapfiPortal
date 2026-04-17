# AI Agent Prompt — Gomoku Multiplayer Website

> **Target Models:** Claude Opus 4.6 / Gemini 3.1 Pro  
> **IDE:** Antigravity  
> **Language:** Website in Vietnamese, codebase in English  

---

## 🎯 ROLE

You are a senior full-stack engineer specializing in real-time multiplayer web applications. You will architect and implement a production-quality Gomoku game website from scratch. Your code must be clean, modular, and optimized for a self-hosted environment with limited server resources.

---

## 📋 PROJECT OVERVIEW

**Project Name:** GomokuVN  
**Description:** A real-time multiplayer Gomoku platform hosted on a personal PC via Cloudflare Tunnel. The website UI is in Vietnamese. The platform supports custom game rules (WALL and PORTAL mechanics), lobby system, in-room chat, scoreboard, and game history.

**Constraints:**
- Self-hosted on personal PC (limited RAM/CPU)
- Tunneled via Cloudflare Tunnel (no dedicated domain)
- No paid services — all free tier or open source
- Must handle connection instability gracefully

---

## 🛠️ TECH STACK

| Layer | Technology | Reason |
|---|---|---|
| Runtime | Node.js (LTS) | Native async, Socket.io support |
| Real-time | Socket.io 4.x | Room management, reconnect, namespaces |
| Backend framework | Express.js | Lightweight HTTP + static serving |
| Database | SQLite via `better-sqlite3` | Zero-config, sync API, no separate process |
| Frontend | Vanilla JS + HTML/CSS | No build tool needed, low overhead |
| Tunnel | Cloudflare Tunnel | Free, stable static domain |

**Do NOT use:** React, Next.js, TypeScript, ORMs, Redis, or any cloud database.

---

## 🗂️ PROJECT STRUCTURE

```
gomoku-vn/
├── server/
│   ├── index.js                  # Entry point, Express + Socket.io init
│   ├── config.js                 # Constants (board sizes, timers, limits)
│   ├── db/
│   │   ├── schema.sql            # SQLite schema
│   │   └── database.js           # DB init + query helpers
│   ├── managers/
│   │   ├── RoomManager.js        # Room CRUD, lifecycle, idle cleanup
│   │   ├── GameEngine.js         # Move validation, win detection, draw
│   │   ├── TimerManager.js       # Server-side countdown (per-move / per-game)
│   │   └── ChatHandler.js        # Message broadcast per room
│   ├── generators/
│   │   ├── WallGenerator.js      # Random WALL placement logic
│   │   └── PortalGenerator.js    # Random PORTAL pair placement logic
│   └── socket/
│       └── SocketHandler.js      # Event routing to managers
├── client/
│   ├── index.html                # Lobby page
│   ├── room.html                 # Game room page
│   ├── css/
│   │   ├── main.css
│   │   ├── lobby.css
│   │   └── room.css
│   └── js/
│       ├── lobby.js              # Lobby UI logic
│       ├── room.js               # Room UI, board rendering
│       ├── board.js              # Canvas/SVG board renderer
│       └── socket-client.js      # Socket.io client wrapper
├── package.json
└── README.md
```

---

## 👥 ROLE SYSTEM

Use class inheritance for role modeling:

```js
class Guest {
  // Can: view board, chat, sit in empty slot
}

class Player extends Guest {
  // Occupies slot #1 or #2
  // Can: make moves, request draw, resign
}

class Host extends Player {
  // Can: change settings, boot users, transfer host
  // Host transfer: queue-based (next user who joined after current host)
}
```

**Rules:**
- A room has exactly **one** Host at all times
- If Host leaves, host is transferred to the next user in join-order queue
- Room is destroyed only when the **last user** leaves
- Players can stand up (become Guest) without leaving room

---

## 🏠 LOBBY

- Displays a live-updating list of all active rooms
- Each room entry shows: room ID, host name, player count, current state, active rule variant
- Users can: create a new room, click to join any existing room
- Rooms in `playing` state are still joinable (as Guest/spectator)

---

## 🎮 ROOM STATES

```
create → idle → playing → interrupted → (back to idle or destroyed)
                                  ↓ (60s timeout, no reconnect)
                              destroyed (if last user)
```

| State | Description |
|---|---|
| `create` | Room just created, host is only user |
| `idle` | Slot #1 or #2 is empty |
| `playing` | Both slots filled, game in progress |
| `interrupted` | One player disconnected, 60s grace period |
| `destroyed` | No users remain |

**Idle cleanup:** Rooms with zero activity for 10 minutes are auto-destroyed.

---

## ⚙️ ROOM SETTINGS (Host only)

```js
{
  boardSize: 17 | 19 | 20,          // default: 17
  ruleWall: false,                   // WALL mechanic on/off
  rulePortal: false,                 // PORTAL mechanic on/off
  timerMode: "per_move" | "per_game",
  timerSeconds: number               // seconds per move OR per game total
}
```

Settings can only be changed when state is NOT `playing`.

---

## 🎲 GAME STATE (server-side only)

```js
{
  gameId: uuid,
  roomId: string,
  players: [
    { id, name, color: "BLACK"|"WHITE", score: { win, loss, draw } }
  ],
  board: number[][],        // 0=empty, 1=black, 2=white, -1=wall, -2=portal
  currentTurn: playerId,
  moveHistory: [{ x, y, color, timestamp }],
  walls: [{ x, y }],       // WALL positions for this game
  portals: [               // PORTAL pairs
    { a: {x,y}, b: {x,y} }
  ],
  timer: { black: number, white: number },
  status: "ongoing" | "finished",
  result: null | { winner: playerId|"draw", reason: string }
}
```

---

## 📐 GAME RULES

### Win Condition
- **FreeStyle:** 5 or more consecutive stones in a row (horizontal, vertical, diagonal) → WIN
- Exactly 5 and 6+ are both valid

### WALL Mechanic
- **Generation:** 3 walls per game, randomly placed
  - Distance from board edge ≥ 3
  - Not in center 3×3 zone
  - Chebyshev distance between any 2 walls > 3
- **Effect:** Cannot place stone on wall cell
- **First move rule:** The very first stone of the game MUST be placed in one of the 8 cells surrounding any wall (highlighted 3×3 area, center excluded)
- **Visual:** Highlight the 8 surrounding cells at game start

### PORTAL Mechanic
- **Generation:** 2 portal pairs (4 cells total)
  - Chebyshev distance between any 2 portal cells ≥ 5
  - Cannot overlap or be adjacent to wall cells
- **Effect:** Cannot place stone on portal cell
- **Win condition extension:** A line of 5+ passing through a portal pair is valid IF:
  - The line contains ≥ 5 **distinct** stones (no duplication)
  - No loop: A→B is valid, A→B→A is invalid
  - Both A→B and B→A directions are valid

### Draw Conditions
1. Player proposes draw → opponent accepts
2. Board is completely filled with no winner

### Disconnect Rule
- On disconnect: state → `interrupted`, 60s countdown begins
- If player reconnects within 60s: game resumes
- If timeout: game result = **no score recorded** (fair play)
- Guest replacement of disconnected player: **NOT allowed**

---

## 📊 SCORE TABLE (per room, cumulative)

```js
// In-memory per room, NOT persisted to DB
scoreTable: {
  [playerId]: { name, win: 0, loss: 0, draw: 0 }
}
```

Score updates immediately after each game ends (except interrupted/no-result games).
Score is cumulative within a room session — players can rotate in and out.

---

## 💾 GAME HISTORY (SQLite)

### Schema

```sql
CREATE TABLE games (
  id TEXT PRIMARY KEY,
  room_id TEXT,
  black_player_id TEXT,
  white_player_id TEXT,
  black_player_name TEXT,
  white_player_name TEXT,
  winner TEXT,              -- player_id | 'draw' | null
  reason TEXT,              -- 'normal' | 'resign' | 'timeout' | 'draw_agreement' | 'board_full'
  board_size INTEGER,
  rule_wall INTEGER,        -- 0 or 1
  rule_portal INTEGER,      -- 0 or 1
  moves TEXT,               -- JSON array of {x, y, color, timestamp}
  walls TEXT,               -- JSON array of {x, y}
  portals TEXT,             -- JSON array of {a:{x,y}, b:{x,y}}
  started_at TEXT,
  ended_at TEXT
);

CREATE TABLE player_games (
  player_id TEXT,
  game_id TEXT,
  FOREIGN KEY (game_id) REFERENCES games(id)
);
```

**Lookup pattern:** Query `player_games` by `player_id` → join `games` for full records.

---

## 📡 SOCKET EVENT SCHEMA

### Client → Server

```
lobby:subscribe                       # Start receiving lobby updates
lobby:unsubscribe

room:create       { settings }
room:join         { roomId }
room:leave
room:sit          { slot: 1|2 }      # Sit in player slot
room:stand                            # Stand up (become guest)
room:settings     { settings }        # Host only
room:ready                            # Player ready toggle

game:move         { x, y }
game:resign
game:draw_offer
game:draw_accept
game:draw_decline
game:rematch                          # Both must send to trigger rematch

chat:message      { text }
```

### Server → Client

```
lobby:update      { rooms[] }

room:joined       { roomId, state, players, settings }
room:updated      { players, state, settings }
room:error        { message }

game:init         { board, walls, portals, currentTurn, timer, firstMoveZones }
game:moved        { x, y, color, nextTurn, timer }
game:ended        { winner, reason, scoreTable }
game:interrupted  { playerId, secondsLeft }
game:resumed      { playerId }
game:draw_offered { from }

chat:message      { from, text, timestamp }
timer:tick        { black, white }
```

---

## 🔧 KEY IMPLEMENTATION NOTES

### Performance (self-hosted priority)
- Cap max concurrent rooms at **20**
- Cap users per room at **10** (2 players + 8 guests)
- Auto-destroy idle rooms after **10 minutes**
- Timer ticks every **1 second** via `setInterval`, clear on game end
- Store all game state in **RAM** (Map objects), only write to SQLite on game end

### Timer Architecture
- Timer runs **server-side only** — never trust client time
- On each `game:move`, restart the active player's timer
- Emit `timer:tick` every second to all room members
- On timeout: auto-trigger game end with `reason: 'timeout'`

### Reconnection
- Socket.io `reconnection: true` on client
- Server stores `socketId → playerId` mapping
- On reconnect: restore game state and rejoin room automatically
- Send full `game:init` payload on reconnect if game is active

### WALL/PORTAL Generation (server-side)
- Generate fresh walls/portals at `game:start`, store in game state
- Validate placements with constraint checks before accepting
- Retry generation up to 100 times if constraints not met

### Security
- Validate every `game:move` server-side (turn, bounds, occupied cell, wall/portal cell)
- Rate-limit chat: max 5 messages per 3 seconds per user
- Sanitize all chat messages (strip HTML)

---

## 🌐 UI LANGUAGE

All UI text, labels, buttons, notifications, and system messages must be in **Vietnamese**.

Key UI strings:
- "Tạo phòng" (Create room)
- "Tham gia" (Join)
- "Sẵn sàng" (Ready)
- "Đầu hàng" (Resign)
- "Đề nghị hòa" (Offer draw)
- "Đang chờ đối thủ..." (Awaiting opponent)
- "Lượt của bạn" (Your turn)
- "Bạn thắng!" / "Bạn thua!" / "Hòa!"
- "Kết nối lại..." (Reconnecting...)

---

## 📋 DEVELOPMENT WORKFLOW

Follow this exact module order. Do not skip ahead. Complete and test each module before proceeding.

```
Phase 1 — Foundation
  [1.1] Project scaffold (package.json, folder structure, config.js)
  [1.2] SQLite setup (schema.sql, database.js)
  [1.3] Express server + static file serving
  [1.4] Socket.io initialization + basic connection logging

Phase 2 — Lobby
  [2.1] RoomManager (create, join, leave, list, cleanup)
  [2.2] Lobby socket events (lobby:subscribe, room:create, room:join)
  [2.3] Lobby UI (room list, create button, join flow)

Phase 3 — Room & Roles
  [3.1] Role system (Guest, Player, Host classes)
  [3.2] Room state machine
  [3.3] Host transfer logic
  [3.4] Room UI (slots, chat, settings panel)
  [3.5] ChatHandler

Phase 4 — Game Engine
  [4.1] Board initialization
  [4.2] Move validation (bounds, turn, cell state)
  [4.3] Win detection (FreeStyle 5+, all directions)
  [4.4] Draw detection (board full, draw agreement)
  [4.5] WallGenerator + first move zone logic
  [4.6] PortalGenerator + portal win condition extension
  [4.7] TimerManager (per-move and per-game modes)

Phase 5 — Game Flow
  [5.1] Full game lifecycle (start → move → end → rematch)
  [5.2] Disconnect/reconnect handling (60s grace)
  [5.3] Score table updates
  [5.4] Game history persistence to SQLite

Phase 6 — Frontend Polish
  [6.1] Board renderer (canvas or SVG, wall/portal visuals)
  [6.2] Timer display
  [6.3] Score table display
  [6.4] Game end overlay
  [6.5] Responsive layout

Phase 7 — Hardening
  [7.1] Input validation & rate limiting
  [7.2] Error handling & logging
  [7.3] Idle room cleanup
  [7.4] Load testing (simulate 10+ concurrent rooms)
```

---

## 📏 CODING RULES

1. **One responsibility per file.** Each manager handles exactly one concern.
2. **Game state lives on the server.** Client renders only what server sends.
3. **No magic numbers.** All constants in `config.js`.
4. **Validate before executing.** Every socket event must validate input before processing.
5. **Fail gracefully.** Emit `room:error` or `game:error` with a Vietnamese message on failure — never crash the server.
6. **Comment complex logic.** WALL/PORTAL generation and win detection must have inline comments explaining the algorithm.
7. **No global mutable state.** Use `RoomManager` as the single source of truth for all room/game data.
8. **Clean up on destroy.** Clear all timers, remove all socket listeners, and null all references when a room is destroyed.
9. **Test each phase** before moving to the next — include a brief manual test checklist in comments.
10. **Commit-ready code only.** No `console.log` debug noise in final output, use a simple logger utility.
