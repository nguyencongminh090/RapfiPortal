'use strict';

/**
 * SocketHandler.js — Socket.io event routing hub.
 *
 * Wires all domain handlers to each incoming socket connection:
 *   - Lobby: subscribe/unsubscribe, room:create, room:join
 *   - Room:  leave, sit, stand, settings, ready, kick
 *   - Chat:  message
 *   - Game:  (Phase 4-5)
 */

const logger      = require('../utils/logger');
const roomManager = require('../managers/RoomManager');
const chatHandler = require('../managers/ChatHandler');

const LOBBY_ROOM = 'lobby';

/**
 * Initialize the Socket.io event handler.
 * @param {import('socket.io').Server} io
 */
function init(io) {
  io.on('connection', (socket) => {
    const user = socket.user;
    logger.info(`[Socket] Connected: ${user.displayName} (${user.userId}) sid=${socket.id}`);

    // Check if user was in a room (reconnect scenario)
    const existingRoom = roomManager.getRoomByUser(user.userId);
    if (existingRoom) {
      socket.join(existingRoom.roomId);
      socket.emit('room:joined', roomManager.serializeRoom(existingRoom));
      logger.info(`[Socket] ${user.displayName} reconnected to room ${existingRoom.roomId}`);
    }

    // ─── Lobby Events ────────────────────────────────────────────────
    registerLobbyHandlers(io, socket);

    // ─── Room Events ─────────────────────────────────────────────────
    registerRoomHandlers(io, socket);

    // ─── Chat Events ─────────────────────────────────────────────────
    registerChatHandlers(io, socket);

    // ─── Disconnect ──────────────────────────────────────────────────
    socket.on('disconnect', (reason) => {
      logger.info(`[Socket] Disconnected: ${user.displayName} (${user.userId}) reason=${reason}`);
      handleDisconnect(io, socket);
      chatHandler.cleanupUser(user.userId);
    });
  });

  io.engine.on('connection_error', (err) => {
    logger.warn('[Socket] Connection error:', err.message);
  });
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
    socket.emit('room:joined', roomManager.serializeRoom(room));
    socket.to(room.roomId).emit('room:updated', roomManager.serializeRoom(room));
    broadcastLobbyUpdate(io);

    // System message: user joined
    io.to(room.roomId).emit('chat:message', {
      from: null,
      fromId: null,
      text: `${user.displayName} đã vào phòng.`,
      timestamp: Date.now(),
      isSystem: true,
    });
  });
}

// =============================================================================
// Room Handlers
// =============================================================================

function registerRoomHandlers(io, socket) {
  const user = socket.user;

  /**
   * room:leave — Leave the current room.
   */
  socket.on('room:leave', () => {
    const roomId = roomManager.getRoomIdByUser(user.userId);
    if (!roomId) return;

    // System message before leaving
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

  /**
   * room:sit — Sit in player slot 1 or 2.
   * Payload: { slot: 1|2 }
   */
  socket.on('room:sit', (payload = {}) => {
    const slot = parseInt(payload.slot, 10);
    const result = roomManager.sitDown(user.userId, slot);

    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const roomId = result.room.roomId;
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
  });

  /**
   * room:stand — Stand up from player slot.
   */
  socket.on('room:stand', () => {
    const result = roomManager.standUp(user.userId);

    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const roomId = result.room.roomId;
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));
    broadcastLobbyUpdate(io);
  });

  /**
   * room:settings — Update room settings (Host only).
   * Payload: { settings: { ... } }
   */
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

  /**
   * room:ready — Toggle ready status (seated players only).
   */
  socket.on('room:ready', () => {
    const result = roomManager.toggleReady(user.userId);

    if (result.error) {
      socket.emit('room:error', { message: result.error });
      return;
    }

    const roomId = result.room.roomId;
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));

    // If both players are ready → Phase 4 will start the game here
    if (result.allReady) {
      io.to(roomId).emit('chat:message', {
        from: null, fromId: null,
        text: 'Cả hai người chơi đã sẵn sàng! Trò chơi sẽ bắt đầu...',
        timestamp: Date.now(), isSystem: true,
      });
      // TODO: Phase 4 — start game
    }
  });

  /**
   * room:kick — Host kicks a user.
   * Payload: { userId: string }
   */
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

    // Notify the kicked user
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
// Disconnect Handler
// =============================================================================

function handleDisconnect(io, socket) {
  const user = socket.user;
  const roomId = roomManager.getRoomIdByUser(user.userId);

  if (!roomId) return;

  const result = roomManager.leaveRoom(user.userId);

  if (result.destroyed) {
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

// =============================================================================
// Helpers
// =============================================================================

function broadcastLobbyUpdate(io) {
  io.to(LOBBY_ROOM).emit('lobby:update', { rooms: roomManager.listRooms() });
}

/** Find all connected sockets for a given userId. */
function findSocketsByUserId(io, userId) {
  const results = [];
  for (const [, s] of io.sockets.sockets) {
    if (s.user && s.user.userId === userId) {
      results.push(s);
    }
  }
  return results;
}

module.exports = { init };
