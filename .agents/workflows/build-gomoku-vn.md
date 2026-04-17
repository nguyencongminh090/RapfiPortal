---
name: build-gomoku-vn
description: Step-by-step workflow for building the GomokuVN real-time web application.
---

# GomokuVN Development Workflow

Follow this **exact module order**. Do not skip ahead. Complete and manually test each module before proceeding to the next. Include a brief manual test checklist in comments at the end of each module.

---

## Phase 0 — Authentication & Login

### [0.1] Auth Schema & Backend
- Add `users` table to `server/db/schema.sql`:
  ```sql
  CREATE TABLE IF NOT EXISTS users (
    id TEXT PRIMARY KEY,          -- UUID v4
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,  -- bcrypt hash (cost 12)
    display_name TEXT NOT NULL,
    created_at TEXT NOT NULL
  );
  ```
- Dependencies: add `bcrypt` (cost 12), `jsonwebtoken` to `package.json`.
- Auth constants in `server/config.js`:
  - `JWT_SECRET` (read from env or hardcoded dev fallback)
  - `JWT_EXPIRY = '7d'`
  - `BCRYPT_ROUNDS = 12`
  - `GUEST_NAME_ADJECTIVES` and `GUEST_NAME_NOUNS` word lists (4-8 letter results).

### [0.2] Auth REST Endpoints
- Implement `server/routes/auth.js` with Express Router:
  - `POST /api/auth/register` — validate username (3-20 chars, alphanumeric+underscore), hash password with bcrypt, insert user, return JWT.
  - `POST /api/auth/login` — compare bcrypt hash, return JWT.
  - `POST /api/auth/guest` — generate a random guest display name (4-8 letters, format: `<Adj><Noun>`, e.g. "LionWolf", "BlueFox"), return a short-lived JWT with `isGuest: true`.
- JWT payload: `{ userId, username, displayName, isGuest }`.
- Mount router in `server/index.js` at `/api/auth`.

### [0.3] Auth Middleware
- Implement `server/middleware/auth.js` — verify JWT, attach `req.user` to request.
- Socket.io auth: in `SocketHandler`, read JWT from `socket.handshake.auth.token`, verify, attach to `socket.user`.
- Reject socket connection with `auth:error` if token is missing or invalid.

### [0.4] Login Page UI
- **Style:** Flat Design — clean, no gradients, strong typography, muted color palette.
- Build `client/login.html` (Vietnamese UI):
  - Two-tab layout: **Đăng nhập** / **Đăng ký**
  - Đăng nhập tab: username + password fields, "Đăng nhập" button.
  - Đăng ký tab: username + display name + password + confirm password, "Tạo tài khoản" button.
  - "Chơi như khách" button at bottom — calls `/api/auth/guest`, stores JWT, redirects to lobby.
  - Show inline validation errors in Vietnamese.
  - On success: store JWT in `localStorage`, redirect to `index.html` (lobby).
- Build `client/css/login.css` (flat design, no board preview).
- Build `client/js/login.js` — handles tab switching, form submit, JWT storage, redirect.

---

## Phase 1 — Foundation

### [1.1] Project Scaffold
- Initialize `package.json` with dependencies: `express`, `socket.io`, `better-sqlite3`, `uuid`, `bcrypt`, `jsonwebtoken`.
- Create the full folder structure:
  ```
  server/ client/ server/db/ server/managers/ server/generators/ server/socket/
  server/routes/ server/middleware/
  client/css/ client/js/
  ```
- Create `server/config.js` with all constants:
  - `MAX_ROOMS = 20`, `MAX_USERS_PER_ROOM = 10`
  - `IDLE_TIMEOUT_MS = 600_000` (10 min)
  - `DISCONNECT_GRACE_MS = 60_000` (60s)
  - `WALL_COUNT = 3`, `PORTAL_PAIR_COUNT = 2`
  - `WALL_RETRY_LIMIT = 100`, `PORTAL_RETRY_LIMIT = 100`
  - `CHAT_RATE_LIMIT = 5`, `CHAT_RATE_WINDOW_MS = 3000`
  - Default `boardSize`, `timerMode`, `timerSeconds`

### [1.2] SQLite Setup
- Create `server/db/schema.sql` with `users`, `games`, and `player_games` tables (see Phase 0 for `users` schema).
- Create `server/db/database.js` — initialize DB, run schema, export query helpers (`getUser`, `createUser`, `saveGame`).

### [1.3] Express Server
- Configure Express in `server/index.js` to serve static files from `client/`.
- Attach Socket.io to the HTTP server.

### [1.4] Socket.io Init
- Set up basic Socket.io connection/disconnection logging.
- Import and wire up `SocketHandler`.

---

## Phase 2 — Lobby

### [2.1] RoomManager
- Implement: `createRoom`, `joinRoom`, `leaveRoom`, `listRooms`, idle cleanup via `setInterval`.
- Enforce `MAX_ROOMS` cap on `createRoom`.
- Auto-destroy room when last user leaves.
- Auto-destroy idle rooms after `IDLE_TIMEOUT_MS`.

### [2.2] Lobby Socket Events
- Handle client → server events:
  - `lobby:subscribe` — add client to lobby update group
  - `lobby:unsubscribe` — remove client from lobby updates
  - `room:create { settings }` — validate settings, create room, emit `lobby:update`
  - `room:join { roomId }` — join room, emit `room:joined`
- Emit `lobby:update { rooms[] }` to all subscribed clients on any room state change.
- Each room entry in `rooms[]` must include: roomId, host name, player count, state, active rule variant.

### [2.3] Lobby UI
- Build `client/index.html` (Vietnamese UI).
- Build `client/css/main.css` and `client/css/lobby.css`.
- Build `client/js/lobby.js`: live room list, "Tạo phòng" button, "Tham gia" per-room button.
- Build `client/js/socket-client.js`: Socket.io client wrapper with `reconnection: true`.

---

## Phase 3 — Room & Roles

### [3.1] Role System
- Implement classes in `server/managers/`:
  ```js
  class Guest { /* view board, chat, sit in empty slot */ }
  class Player extends Guest { /* make moves, request draw, resign */ }
  class Host extends Player { /* change settings, boot users, transfer host */ }
  ```
- Host is always assigned — transfer to next in join-order queue on leave.

### [3.2] Room State Machine
- States: `create → idle → playing → interrupted → destroyed`
- Enforce valid transitions; reject invalid operations per state.

### [3.3] Host Transfer Logic
- Maintain a join-order queue per room.
- On host leave: promote next user in queue to Host.

### [3.4] Room Socket Events
- Handle client → server events:
  - `room:leave` — leave room, trigger host transfer or room destruction
  - `room:sit { slot: 1|2 }` — occupy player slot (only valid in non-playing state)
  - `room:stand` — vacate slot, become Guest
  - `room:settings { settings }` — Host only; update room settings (blocked during `playing`)
  - `room:ready` — Player ready toggle
- Emit `room:updated { players, state, settings }` on any room change.
- Emit `room:error { message }` (Vietnamese) on invalid actions.

### [3.5] Room UI
- Build `client/room.html` (Vietnamese UI).
- Build `client/css/room.css`.
- Build `client/js/room.js`: player slots, ready status, settings panel (Host only), chat display.

### [3.6] ChatHandler
- Implement `server/managers/ChatHandler.js`.
- Rate-limit: max 5 messages per 3 seconds per user (use sliding window).
- Sanitize: strip all HTML from messages.
- Broadcast `chat:message { from, text, timestamp }` to all room members.

---

## Phase 4 — Game Engine

### [4.1] Board Initialization
- Implement `server/managers/GameEngine.js`.
- Initialize `board` as a 2D array of zeros based on `boardSize`.
- Build the full `gameState` object (exact shape from spec).

### [4.2] Move Validation
- Reject moves if: wrong turn, out of bounds, cell occupied, cell is wall/portal.

### [4.3] Win Detection
- FreeStyle rule: 5 **or more** consecutive stones in a row (H, V, diagonal) → WIN.
- Both exactly 5 and 6+ are valid wins.

### [4.4] Draw Detection
- Board completely filled with no winner.
- Draw agreement: opponent accepts `game:draw_offer`.

### [4.5] Draw Offer / Accept / Decline Flow
- Handle client → server events:
  - `game:draw_offer` — emit `game:draw_offered { from }` to room
  - `game:draw_accept` — trigger draw end, update scores
  - `game:draw_decline` — notify room, cancel offer
- Only one pending draw offer may exist at a time.

### [4.6] Resign Flow
- Handle `game:resign` — immediately end game, record result.

### [4.7] WallGenerator
- Implement `server/generators/WallGenerator.js`.
- Generate 3 walls per game with constraints:
  - Distance from any board edge ≥ 3
  - Not in center 3×3 zone
  - Chebyshev distance between any 2 walls > 3
- Retry up to `WALL_RETRY_LIMIT` times.
- Compute and return `firstMoveZones`: the 8 surrounding cells of each wall.

### [4.8] PortalGenerator
- Implement `server/generators/PortalGenerator.js`.
- Generate 2 portal pairs (4 cells) with constraints:
  - Chebyshev distance between any 2 portal cells ≥ 5
  - Cannot overlap or be adjacent to wall cells
- Retry up to `PORTAL_RETRY_LIMIT` times.
- Implement portal win-condition extension in `GameEngine.js` (5+ distinct stones, no loops).

### [4.9] TimerManager
- Implement `server/managers/TimerManager.js`.
- Support `per_move` and `per_game` modes.
- Run `setInterval` every 1 second; emit `timer:tick { black, white }` to room.
- On `game:move`: restart the moving player's timer (per_move) or keep running (per_game).
- On timeout: auto-trigger game end with `reason: 'timeout'`.
- Clear interval on game end.

---

## Phase 5 — Game Flow

### [5.1] Game Lifecycle
- `game:start` (triggered when both players ready):
  - Generate walls and portals.
  - Initialize board and game state.
  - Start timer.
  - Emit `game:init { board, walls, portals, currentTurn, timer, firstMoveZones }`.
- `game:move { x, y }`:
  - Validate move server-side.
  - Update board and `moveHistory`.
  - Check win/draw.
  - Emit `game:moved { x, y, color, nextTurn, timer }`.
- Game end: emit `game:ended { winner, reason, scoreTable }`.
- `game:rematch`: both players must emit → reset board and restart.

### [5.2] Disconnect / Reconnect Handling
- On disconnect: room state → `interrupted`; start 60s countdown.
- Emit `game:interrupted { playerId, secondsLeft }` to room.
- Store `socketId → playerId` map for reconnect lookup.
- On reconnect within 60s: resume game, emit `game:resumed { playerId }` + full `game:init`.
- On 60s timeout: end game with **no score recorded**.

### [5.3] Score Table
- Update in-memory `scoreTable` after each completed game (excluding interrupted/timeout-no-result).
- Emit updated `scoreTable` in `game:ended` payload.

### [5.4] Game History Persistence
- On game end (normal/resign/timeout/draw): write full record to SQLite `games` and `player_games` tables.
- Fields: all from schema including JSON-serialized `moves`, `walls`, `portals`.

---

## Phase 6 — Frontend Polish

### [6.1] Board Renderer
- Build `client/js/board.js` using Canvas or SVG.
- Render: grid lines, stones (black/white), wall cells (distinct visual), portal pairs (matched color coding).
- Highlight `firstMoveZones` on game start (if WALL rule active).
- Animate stone placement with a subtle drop effect.

### [6.2] Game UI (room.js integration)
- Display current turn indicator ("Lượt của bạn" / opponent's turn).
- Wire up "Đầu hàng" (Resign), "Đề nghị hòa" (Draw offer) buttons.
- Display draw offer prompt with accept/decline options.
- Show "Đang chờ đối thủ..." when one slot is empty.

### [6.3] Timer Display
- Show countdown timers per player.
- Visual urgency cue when time is low (e.g., color change at < 10s).

### [6.4] Score Table Display
- Show per-room cumulative scores (win/loss/draw) for both players.
- Update live when `game:ended` is received.

### [6.5] Game End Overlay
- Show result overlay: "Bạn thắng!" / "Bạn thua!" / "Hòa!".
- Include reason string and a "Đấu lại" (Rematch) button.

### [6.6] Reconnection UX
- Show "Kết nối lại..." banner while Socket.io is reconnecting.
- Restore full board state from `game:init` on reconnect.

### [6.7] Responsive Layout
- Ensure board and UI work across desktop and mobile screen sizes.

---

## Phase 7 — Hardening

### [7.1] Input Validation
- Audit all socket event handlers — ensure every event validates input before processing.
- Validate `game:move`: turn, bounds, occupied, wall/portal cell.
- Validate room settings struct on `room:settings`.

### [7.2] Rate Limiting & Security
- Confirm chat rate limiter is active (5 msg / 3s / user).
- Confirm HTML sanitization on all user-supplied text.
- Add basic socket flood protection (reject excess events per second).

### [7.3] Error Handling & Logging
- Replace all `console.log` with a simple logger utility (e.g., `server/utils/logger.js`).
- All user-facing errors emitted as `room:error` or `game:error` with Vietnamese messages.
- Log server-side errors with stack traces.

### [7.4] Idle Room Cleanup
- Verify the idle cleanup `setInterval` runs correctly and does not leave dangling references.
- Confirm timers, listeners, and Maps are nulled on room destruction.

### [7.5] Load Testing
- Simulate **10+ concurrent rooms** with bots or manual sessions.
- Verify CPU/RAM stays within acceptable bounds.
- Confirm no memory leaks after rooms are destroyed.

---

**Reference:** Always cross-check with `gomoku-vn-dev.md` for constraints and `gomoku_agent_prompt.md` for the full specification.
