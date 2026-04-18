'use strict';

/**
 * SocketHandler.js — Socket.io event routing hub.
 *
 * Wires all domain handlers to each incoming socket connection:
 *   - Lobby: subscribe/unsubscribe, room:create, room:join
 *   - Room:  leave, sit, stand, settings, ready, kick
 *   - Chat:  message
 *   - Game:  move, resign, draw_offer, draw_accept, draw_decline, rematch
 */

const logger          = require('../utils/logger');
const roomManager     = require('../managers/RoomManager');
const chatHandler     = require('../managers/ChatHandler');
const { GameEngine }  = require('../managers/GameEngine');
const TimerManager    = require('../managers/TimerManager');
const WallGenerator   = require('../generators/WallGenerator');
const PortalGenerator = require('../generators/PortalGenerator');
const database        = require('../db/database');
const config          = require('../config');

const LOBBY_ROOM = 'lobby';

// Per-room timer storage: roomId → TimerManager instance
const timerMap = new Map();

// Per-player disconnect grace timers: playerId → { timeout, roomId, countdown }
const disconnectTimers = new Map();

/**
 * Initialize the Socket.io event handler.
 * @param {import('socket.io').Server} io
 */
function init(io) {
  // [7.2] Socket event flood protection middleware
  io.use((socket, next) => {
    let eventCount = 0;
    const resetInterval = setInterval(() => { eventCount = 0; }, 1000);
    const origEmit = socket.onevent;
    socket.onevent = function(packet) {
      eventCount++;
      if (eventCount > config.MAX_EVENTS_PER_SECOND) {
        socket.emit('room:error', { message: 'Bạn đang gửi quá nhiều yêu cầu. Vui lòng chờ.' });
        return;
      }
      origEmit.call(this, packet);
    };
    socket.on('disconnect', () => clearInterval(resetInterval));
    next();
  });

  io.on('connection', (socket) => {
    const user = socket.user;
    logger.info(`[Socket] Connected: ${user.displayName} (${user.userId}) sid=${socket.id}`);

    // [5.2] Check if this is a reconnect during disconnect grace period
    if (cancelDisconnectGrace(io, socket)) {
      // Game resumed — skip normal reconnect flow
    }
    // Check if user was in a room (normal reconnect scenario)
    else {
      const existingRoom = roomManager.getRoomByUser(user.userId);
      if (existingRoom) {
        socket.join(existingRoom.roomId);
        const payload = roomManager.serializeRoom(existingRoom);
        // If game is active, include game state
        if (existingRoom.gameState) {
          payload.gameState = existingRoom.gameState.serialize();
          const timer = timerMap.get(existingRoom.roomId);
          if (timer) payload.timer = timer.getTimers();
        }
        socket.emit('room:joined', payload);
        logger.info(`[Socket] ${user.displayName} reconnected to room ${existingRoom.roomId}`);
      }
    }

    // ─── Lobby Events ────────────────────────────────────────────────
    registerLobbyHandlers(io, socket);

    // ─── Room Events ─────────────────────────────────────────────────
    registerRoomHandlers(io, socket);

    // ─── Game Events ─────────────────────────────────────────────────
    registerGameHandlers(io, socket);

    // ─── Chat Events ─────────────────────────────────────────────────
    registerChatHandlers(io, socket);

    // ─── Disconnect ──────────────────────────────────────────────────
    socket.on('disconnect', (reason) => {
      logger.info(`[Socket] Disconnected: ${user.displayName} (${user.userId}) reason=${reason}`);
      handleDisconnect(io, socket);
      chatHandler.cleanupUser(user.userId);
    });

    // [7.3] Catch-all error handler for this socket
    socket.on('error', (err) => {
      logger.error(`[Socket] Unhandled error for ${user.displayName}:`, err.stack || err.message);
      socket.emit('room:error', { message: 'Đã xảy ra lỗi. Vui lòng thử lại.' });
    });
  });

  io.engine.on('connection_error', (err) => {
    logger.warn('[Socket] Connection error:', err.message);
  });

  // [7.4] Wire idle room cleanup to also clear game timers
  setInterval(() => {
    for (const [roomId] of timerMap) {
      if (!roomManager.getRoom(roomId)) {
        cleanupRoomTimer(roomId);
        logger.info(`[Socket] Orphan timer cleaned for destroyed room ${roomId}`);
      }
    }
  }, 60_000);
}

// =============================================================================
// Lobby Handlers
// =============================================================================

function registerLobbyHandlers(io, socket) {
  const user = socket.user;

  socket.on('lobby:subscribe', () => {
    socket.join(LOBBY_ROOM);
    socket.emit('lobby:update', { rooms: roomManager.listRooms() });
  });

  socket.on('lobby:unsubscribe', () => {
    socket.leave(LOBBY_ROOM);
  });

  socket.on('room:create', (payload = {}) => {
    const result = roomManager.createRoom(
      { userId: user.userId, displayName: user.displayName, isGuest: user.isGuest },
      payload.settings || {}
    );

    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const room = result.room;
    socket.leave(LOBBY_ROOM);
    socket.join(room.roomId);
    socket.emit('room:joined', roomManager.serializeRoom(room));
    broadcastLobbyUpdate(io);
  });

  socket.on('room:join', (payload = {}) => {
    if (!payload.roomId) {
      socket.emit('room:error', { message: 'Thiếu mã phòng.' });
      return;
    }

    const result = roomManager.joinRoom(
      { userId: user.userId, displayName: user.displayName, isGuest: user.isGuest },
      payload.roomId
    );

    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const room = result.room;
    socket.leave(LOBBY_ROOM);
    socket.join(room.roomId);

    const payload2 = roomManager.serializeRoom(room);
    // If game is active, include game state for joining spectators
    if (room.gameState) {
      payload2.gameState = room.gameState.serialize();
      const timer = timerMap.get(room.roomId);
      if (timer) payload2.timer = timer.getTimers();
    }
    socket.emit('room:joined', payload2);
    socket.to(room.roomId).emit('room:updated', roomManager.serializeRoom(room));
    broadcastLobbyUpdate(io);

    io.to(room.roomId).emit('chat:message', {
      from: null, fromId: null,
      text: `${user.displayName} đã vào phòng.`,
      timestamp: Date.now(), isSystem: true,
    });
  });
}

// =============================================================================
// Room Handlers
// =============================================================================

function registerRoomHandlers(io, socket) {
  const user = socket.user;

  socket.on('room:leave', () => {
    const roomId = roomManager.getRoomIdByUser(user.userId);
    if (!roomId) return;

    const room = roomManager.getRoom(roomId);
    if (room) {
      io.to(roomId).emit('chat:message', {
        from: null, fromId: null,
        text: `${user.displayName} đã rời phòng.`,
        timestamp: Date.now(), isSystem: true,
      });
    }

    const result = roomManager.leaveRoom(user.userId);
    socket.leave(roomId);
    socket.emit('room:left');

    if (result.destroyed) {
      cleanupRoomTimer(roomId);
      broadcastLobbyUpdate(io);
    } else if (result.room) {
      io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
      if (result.hostTransferred) {
        const newHost = result.room.users.get(result.room.host);
        io.to(roomId).emit('chat:message', {
          from: null, fromId: null,
          text: `${newHost ? newHost.displayName : '—'} là chủ phòng mới.`,
          timestamp: Date.now(), isSystem: true,
        });
      }
      broadcastLobbyUpdate(io);
    }
  });

  socket.on('room:sit', (payload = {}) => {
    const slot = parseInt(payload.slot, 10);
    const result = roomManager.sitDown(user.userId, slot);
    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }
    io.to(result.room.roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
  });

  socket.on('room:stand', () => {
    const result = roomManager.standUp(user.userId);
    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }
    io.to(result.room.roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
  });

  socket.on('room:settings', (payload = {}) => {
    const result = roomManager.updateSettings(user.userId, payload.settings || {});
    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }
    const roomId = result.room.roomId;
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
    io.to(roomId).emit('chat:message', {
      from: null, fromId: null,
      text: 'Cài đặt phòng đã được thay đổi.',
      timestamp: Date.now(), isSystem: true,
    });
  });

  socket.on('room:ready', () => {
    const result = roomManager.toggleReady(user.userId);
    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const roomId = result.room.roomId;
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));

    // Both players ready → START GAME
    if (result.allReady) {
      startGame(io, result.room);
    }
  });

  socket.on('room:kick', (payload = {}) => {
    const targetId = payload.userId;
    if (!targetId) {
      socket.emit('room:error', { message: 'Thiếu thông tin người dùng.' });
      return;
    }

    const roomId = roomManager.getRoomIdByUser(user.userId);
    const result = roomManager.kickUser(user.userId, targetId);
    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const kickedSockets = findSocketsByUserId(io, targetId);
    for (const s of kickedSockets) {
      s.leave(roomId);
      s.emit('room:kicked', { message: 'Bạn đã bị mời ra khỏi phòng.' });
    }

    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
  });
}

// =============================================================================
// Game Handlers
// =============================================================================

function registerGameHandlers(io, socket) {
  const user = socket.user;

  /**
   * game:move { x, y } — Place a stone.
   */
  socket.on('game:move', (payload = {}) => {
    const room = roomManager.getRoomByUser(user.userId);
    if (!room || !room.gameState) {
      socket.emit('game:error', { message: 'Không có ván đấu đang diễn ra.' });
      return;
    }

    const x = parseInt(payload.x, 10);
    const y = parseInt(payload.y, 10);

    // [7.1] Validate coordinates are valid numbers
    if (isNaN(x) || isNaN(y)) {
      socket.emit('game:error', { message: 'Toạ độ không hợp lệ.' });
      return;
    }

    const engine = room.gameState;
    const result = engine.makeMove(user.userId, x, y);

    if (result.error) {
      socket.emit('game:error', { message: result.error });
      return;
    }

    // Emit move to all room members
    const timer = timerMap.get(room.roomId);
    const movePayload = {
      x, y,
      color: result.color,
      nextTurn: result.nextTurn,
      moveCount: engine.moveCount,
      timer: timer ? timer.getTimers() : null,
    };

    if (result.won) {
      // Game won
      movePayload.gameOver = true;
      movePayload.result = engine.result;
      handleGameEnd(io, room);
    } else if (result.draw) {
      // Board full draw
      movePayload.gameOver = true;
      movePayload.result = engine.result;
      handleGameEnd(io, room);
    } else {
      // Switch timer
      if (timer) {
        const nextColor = result.nextTurn === room.gameState.players[0].userId ? 'black' : 'white';
        timer.switchTurn(nextColor);
        movePayload.timer = timer.getTimers();
      }
    }

    io.to(room.roomId).emit('game:moved', movePayload);
    room.lastActivity = Date.now();
  });

  /**
   * game:resign — Player resigns.
   */
  socket.on('game:resign', () => {
    const room = roomManager.getRoomByUser(user.userId);
    if (!room || !room.gameState) {
      socket.emit('game:error', { message: 'Không có ván đấu đang diễn ra.' });
      return;
    }

    const result = room.gameState.resign(user.userId);
    if (result.error) {
      socket.emit('game:error', { message: result.error });
      return;
    }

    // Save result before handleGameEnd nullifies gameState
    const gameResult = room.gameState.result;
    handleGameEnd(io, room);
    io.to(room.roomId).emit('game:ended', {
      result: gameResult,
      scoreTable: room.scoreTable,
    });
    io.to(room.roomId).emit('chat:message', {
      from: null, fromId: null,
      text: `${user.displayName} đã đầu hàng.`,
      timestamp: Date.now(), isSystem: true,
    });
  });

  /**
   * game:draw_offer — Offer a draw.
   */
  socket.on('game:draw_offer', () => {
    const room = roomManager.getRoomByUser(user.userId);
    if (!room || !room.gameState) return;

    const result = room.gameState.offerDraw(user.userId);
    if (result.error) {
      socket.emit('game:error', { message: result.error });
      return;
    }

    io.to(room.roomId).emit('game:draw_offered', { from: user.userId, fromName: user.displayName });
    io.to(room.roomId).emit('chat:message', {
      from: null, fromId: null,
      text: `${user.displayName} đề nghị hoà.`,
      timestamp: Date.now(), isSystem: true,
    });
  });

  /**
   * game:draw_accept — Accept a pending draw offer.
   */
  socket.on('game:draw_accept', () => {
    const room = roomManager.getRoomByUser(user.userId);
    if (!room || !room.gameState) return;

    const result = room.gameState.acceptDraw(user.userId);
    if (result.error) {
      socket.emit('game:error', { message: result.error });
      return;
    }

    const gameResult = room.gameState.result;
    handleGameEnd(io, room);
    io.to(room.roomId).emit('game:ended', {
      result: gameResult,
      scoreTable: room.scoreTable,
    });
    io.to(room.roomId).emit('chat:message', {
      from: null, fromId: null,
      text: 'Hai bên đồng ý hoà.',
      timestamp: Date.now(), isSystem: true,
    });
  });

  /**
   * game:draw_decline — Decline a pending draw offer.
   */
  socket.on('game:draw_decline', () => {
    const room = roomManager.getRoomByUser(user.userId);
    if (!room || !room.gameState) return;

    const result = room.gameState.declineDraw(user.userId);
    if (result.error) {
      socket.emit('game:error', { message: result.error });
      return;
    }

    io.to(room.roomId).emit('game:draw_declined', { by: user.userId });
    io.to(room.roomId).emit('chat:message', {
      from: null, fromId: null,
      text: `${user.displayName} từ chối hoà.`,
      timestamp: Date.now(), isSystem: true,
    });
  });

  /**
   * game:rematch — Request a rematch. Both players must emit.
   */
  socket.on('game:rematch', () => {
    const room = roomManager.getRoomByUser(user.userId);
    if (!room) return;

    if (room.state !== 'idle') {
      socket.emit('game:error', { message: 'Phòng đang trong trạng thái không hợp lệ.' });
      return;
    }

    // Toggle ready for a new game
    const result = roomManager.toggleReady(user.userId);
    if (result.error) {
      socket.emit('game:error', { message: result.error });
      return;
    }

    io.to(room.roomId).emit('room:updated', roomManager.serializeRoom(result.room));

    if (result.allReady) {
      startGame(io, result.room);
    }
  });
}

// =============================================================================
// Chat Handlers
// =============================================================================

function registerChatHandlers(io, socket) {
  const user = socket.user;

  socket.on('chat:message', (payload = {}) => {
    const roomId = roomManager.getRoomIdByUser(user.userId);
    if (!roomId) {
      socket.emit('chat:error', { message: 'Bạn cần vào phòng để chat.' });
      return;
    }
    chatHandler.handleMessage(io, socket, roomId, payload.text || '');
  });
}

// =============================================================================
// Game Lifecycle
// =============================================================================

/**
 * Start a new game when both players are ready.
 *
 * @param {import('socket.io').Server} io
 * @param {object} room — room object from RoomManager
 */
function startGame(io, room) {
  const roomId = room.roomId;
  const settings = room.settings;

  // Find seated players
  let slot1Player = null, slot2Player = null;
  for (const [, u] of room.users) {
    if (u.slot === 1) slot1Player = u;
    if (u.slot === 2) slot2Player = u;
  }

  if (!slot1Player || !slot2Player) {
    io.to(roomId).emit('game:error', { message: 'Thiếu người chơi.' });
    return;
  }

  // Generate walls (if enabled)
  let walls = [];
  let firstMoveZones = [];
  if (settings.ruleWall) {
    const wResult = WallGenerator.generate(settings.boardSize);
    if (!wResult) {
      io.to(roomId).emit('game:error', {
        message: 'Không thể tạo tường. Vui lòng thử lại.',
      });
      return;
    }
    walls = wResult.walls;
    firstMoveZones = wResult.firstMoveZones;
  }

  // Generate portals (if enabled)
  let portals = [];
  if (settings.rulePortal) {
    const pResult = PortalGenerator.generate(settings.boardSize, walls);
    if (!pResult) {
      io.to(roomId).emit('game:error', {
        message: 'Không thể tạo cổng dịch chuyển. Vui lòng thử lại.',
      });
      return;
    }
    portals = pResult.portals;
  }

  // Create game engine
  const engine = new GameEngine({
    roomId,
    boardSize: settings.boardSize,
    players: [
      { userId: slot1Player.userId, displayName: slot1Player.displayName, color: 'BLACK' },
      { userId: slot2Player.userId, displayName: slot2Player.displayName, color: 'WHITE' },
    ],
    walls,
    portals,
    firstMoveZones,
  });

  // Store game state on the room
  room.gameState = engine;
  room.state = 'playing';

  // Reset ready states
  for (const [, u] of room.users) {
    u.ready = false;
  }

  // Create timer
  const timer = new TimerManager({
    roomId,
    mode: settings.timerMode,
    seconds: settings.timerSeconds,
    blackPlayerId: slot1Player.userId,
    whitePlayerId: slot2Player.userId,
    onTick: (timers) => {
      io.to(roomId).emit('timer:tick', timers);
    },
    onTimeout: (timedOutPlayerId) => {
      // Handle timeout — the player who timed out loses
      engine.handleTimeout(timedOutPlayerId);
      handleGameEnd(io, room);

      const timedOutUser = room.users.get(timedOutPlayerId);
      io.to(roomId).emit('game:ended', {
        result: engine.result,
        scoreTable: room.scoreTable,
      });
      io.to(roomId).emit('chat:message', {
        from: null, fromId: null,
        text: `${timedOutUser ? timedOutUser.displayName : '—'} hết thời gian!`,
        timestamp: Date.now(), isSystem: true,
      });
    },
  });

  timerMap.set(roomId, timer);
  timer.start();

  // Emit game:init to all room members
  io.to(roomId).emit('game:init', {
    ...engine.serialize(),
    timer: timer.getTimers(),
  });

  // Update room display
  io.to(roomId).emit('room:updated', roomManager.serializeRoom(room));
  broadcastLobbyUpdate(io);

  io.to(roomId).emit('chat:message', {
    from: null, fromId: null,
    text: 'Ván đấu bắt đầu! Đen đi trước.',
    timestamp: Date.now(), isSystem: true,
  });

  logger.info(`[Socket] Game started in room ${roomId}`);
}

/**
 * Handle game end — stop timer, update scores, reset room state.
 *
 * @param {import('socket.io').Server} io
 * @param {object} room
 */
function handleGameEnd(io, room, opts = {}) {
  const roomId = room.roomId;
  const engine = room.gameState;
  const noScore = opts.noScore || false; // Interrupted games don't record score

  // Stop timer
  cleanupRoomTimer(roomId);

  // Update score table (only for completed games, not interrupted)
  if (engine && engine.result && !noScore) {
    const { winner, reason } = engine.result;

    for (const p of engine.players) {
      if (!room.scoreTable[p.userId]) {
        room.scoreTable[p.userId] = { name: p.displayName, win: 0, loss: 0, draw: 0 };
      }

      if (winner === 'draw') {
        room.scoreTable[p.userId].draw++;
      } else if (winner === p.userId) {
        room.scoreTable[p.userId].win++;
      } else {
        room.scoreTable[p.userId].loss++;
      }
    }
  }

  // [5.4] Persist game to SQLite
  if (engine && engine.result) {
    try {
      const black = engine.players.find(p => p.color === 'BLACK');
      const white = engine.players.find(p => p.color === 'WHITE');
      database.saveGame({
        gameId: engine.gameId,
        roomId: engine.roomId,
        players: engine.players.map(p => ({
          id: p.userId,
          name: p.displayName,
          color: p.color,
          isGuest: true, // All players treated as guest for now
        })),
        result: engine.result,
        boardSize: engine.boardSize,
        ruleWall: engine.walls.length > 0,
        rulePortal: engine.portals.length > 0,
        moveHistory: engine.moveHistory,
        walls: engine.walls,
        portals: engine.portals,
        startedAt: engine.moveHistory.length > 0 ? engine.moveHistory[0].timestamp : Date.now(),
        endedAt: Date.now(),
      });
    } catch (err) {
      logger.warn('[Socket] Failed to persist game:', err.message);
    }
  }

  // Reset room state
  room.state = 'idle';
  room.gameState = null;

  // Reset ready states for all seated players
  for (const [, u] of room.users) {
    u.ready = false;
  }

  io.to(roomId).emit('room:updated', roomManager.serializeRoom(room));
  broadcastLobbyUpdate(io);
}

// =============================================================================
// Disconnect Handler — with 60s grace period for in-game disconnects [5.2]
// =============================================================================

function handleDisconnect(io, socket) {
  const user = socket.user;
  const roomId = roomManager.getRoomIdByUser(user.userId);
  if (!roomId) return;

  const room = roomManager.getRoom(roomId);

  // [5.2] If a game is active and this user is a player, start grace period
  if (room && room.gameState && room.gameState.status === 'ongoing') {
    const isPlayer = room.gameState.players.some(p => p.userId === user.userId);
    if (isPlayer) {
      startDisconnectGrace(io, room, user);
      return; // Don't remove from room yet
    }
  }

  // Normal disconnect (not in game, or spectator)
  const result = roomManager.leaveRoom(user.userId);

  if (result.destroyed) {
    cleanupRoomTimer(roomId);
    broadcastLobbyUpdate(io);
  } else if (result.room) {
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    io.to(roomId).emit('chat:message', {
      from: null, fromId: null,
      text: `${user.displayName} đã mất kết nối.`,
      timestamp: Date.now(), isSystem: true,
    });

    if (result.hostTransferred) {
      const newHost = result.room.users.get(result.room.host);
      io.to(roomId).emit('chat:message', {
        from: null, fromId: null,
        text: `${newHost ? newHost.displayName : '—'} là chủ phòng mới.`,
        timestamp: Date.now(), isSystem: true,
      });
    }

    broadcastLobbyUpdate(io);
  }
}

/**
 * [5.2] Start 60s disconnect grace period for a player in an active game.
 */
function startDisconnectGrace(io, room, user) {
  const roomId = room.roomId;
  const graceSec = Math.floor(config.DISCONNECT_GRACE_MS / 1000);

  // Pause the game timer
  const timer = timerMap.get(roomId);
  if (timer) timer.stop();

  // Set room state to interrupted
  room.state = 'interrupted';

  io.to(roomId).emit('game:interrupted', {
    playerId: user.userId,
    playerName: user.displayName,
    secondsLeft: graceSec,
  });
  io.to(roomId).emit('chat:message', {
    from: null, fromId: null,
    text: `${user.displayName} mất kết nối. Chờ kết nối lại (${graceSec}s)...`,
    timestamp: Date.now(), isSystem: true,
  });

  // Start countdown
  let remaining = graceSec;
  const countdown = setInterval(() => {
    remaining--;
    if (remaining <= 0) {
      clearInterval(countdown);
    }
  }, 1000);

  // Timeout: end game with no score
  const timeout = setTimeout(() => {
    clearInterval(countdown);
    disconnectTimers.delete(user.userId);

    // Game over — no score recorded (fair play)
    if (room.gameState) {
      room.gameState.status = 'finished';
      room.gameState.result = { winner: null, reason: 'disconnect' };
      handleGameEnd(io, room, { noScore: true });

      io.to(roomId).emit('game:ended', {
        result: { winner: null, reason: 'disconnect' },
        scoreTable: room.scoreTable,
      });
      io.to(roomId).emit('chat:message', {
        from: null, fromId: null,
        text: `${user.displayName} không kết nối lại. Ván đấu huỷ, không ghi điểm.`,
        timestamp: Date.now(), isSystem: true,
      });
    }

    // Now actually remove the player
    const result = roomManager.leaveRoom(user.userId);
    if (result.destroyed) cleanupRoomTimer(roomId);
    else if (result.room) io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
  }, config.DISCONNECT_GRACE_MS);

  disconnectTimers.set(user.userId, { timeout, countdown, roomId });
  logger.info(`[Socket] Grace period started for ${user.displayName} in room ${roomId}`);
}

/**
 * [5.2] Cancel disconnect grace and resume game when player reconnects.
 */
function cancelDisconnectGrace(io, socket) {
  const user = socket.user;
  const entry = disconnectTimers.get(user.userId);
  if (!entry) return false;

  clearTimeout(entry.timeout);
  clearInterval(entry.countdown);
  disconnectTimers.delete(user.userId);

  const room = roomManager.getRoom(entry.roomId);
  if (!room || !room.gameState) return false;

  // Resume game
  room.state = 'playing';

  // Re-join socket room
  socket.join(entry.roomId);

  // Restart timer
  const timer = timerMap.get(entry.roomId);
  if (timer) timer.start();

  // Emit resume + full state
  io.to(entry.roomId).emit('game:resumed', { playerId: user.userId });
  socket.emit('game:init', {
    ...room.gameState.serialize(),
    timer: timer ? timer.getTimers() : null,
  });
  io.to(entry.roomId).emit('room:updated', roomManager.serializeRoom(room));

  io.to(entry.roomId).emit('chat:message', {
    from: null, fromId: null,
    text: `${user.displayName} đã kết nối lại! Ván đấu tiếp tục.`,
    timestamp: Date.now(), isSystem: true,
  });

  logger.info(`[Socket] ${user.displayName} reconnected, game resumed in room ${entry.roomId}`);
  return true;
}

// =============================================================================
// Helpers
// =============================================================================

function broadcastLobbyUpdate(io) {
  io.to(LOBBY_ROOM).emit('lobby:update', { rooms: roomManager.listRooms() });
}

function findSocketsByUserId(io, userId) {
  const results = [];
  for (const [, s] of io.sockets.sockets) {
    if (s.user && s.user.userId === userId) {
      results.push(s);
    }
  }
  return results;
}

/** Clean up timer for a room. */
function cleanupRoomTimer(roomId) {
  const timer = timerMap.get(roomId);
  if (timer) {
    timer.destroy();
    timerMap.delete(roomId);
  }
}

module.exports = { init };
