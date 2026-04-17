'use strict';

/**
 * SocketHandler.js — Socket.io event routing hub.
 *
 * Wires all domain handlers to each incoming socket connection:
 *   - Lobby: subscribe/unsubscribe, room:create, room:join
 *   - Room:  (Phase 3)
 *   - Game:  (Phase 4-5)
 *   - Chat:  (Phase 3)
 *
 * Also handles disconnect → leaveRoom cleanup.
 */

const logger      = require('../utils/logger');
const roomManager = require('../managers/RoomManager');

// Socket.io room name for lobby subscribers
const LOBBY_ROOM = 'lobby';

/**
 * Initialize the Socket.io event handler.
 * @param {import('socket.io').Server} io
 */
function init(io) {
  io.on('connection', (socket) => {
    const user = socket.user;
    logger.info(`[Socket] Connected: ${user.displayName} (${user.userId}) sid=${socket.id}`);

    // ─── Lobby Events ────────────────────────────────────────────────
    registerLobbyHandlers(io, socket);

    // ─── Room Events (Phase 3) ───────────────────────────────────────
    // registerRoomHandlers(io, socket);

    // ─── Game Events (Phase 4-5) ─────────────────────────────────────
    // registerGameHandlers(io, socket);

    // ─── Chat Events (Phase 3) ───────────────────────────────────────
    // registerChatHandlers(io, socket);

    // ─── Disconnect ──────────────────────────────────────────────────
    socket.on('disconnect', (reason) => {
      logger.info(`[Socket] Disconnected: ${user.displayName} (${user.userId}) reason=${reason}`);
      handleDisconnect(io, socket);
    });
  });

  // Relay Socket.io auth errors to client
  io.engine.on('connection_error', (err) => {
    logger.warn('[Socket] Connection error:', err.message);
  });
}

// =============================================================================
// Lobby Handlers
// =============================================================================

function registerLobbyHandlers(io, socket) {
  const user = socket.user;

  /**
   * lobby:subscribe — Client wants live lobby updates.
   * Adds the socket to the "lobby" Socket.io room so it receives broadcasts.
   */
  socket.on('lobby:subscribe', () => {
    socket.join(LOBBY_ROOM);
    // Immediately send current room list
    socket.emit('lobby:update', { rooms: roomManager.listRooms() });
  });

  /**
   * lobby:unsubscribe — Client no longer wants lobby updates.
   */
  socket.on('lobby:unsubscribe', () => {
    socket.leave(LOBBY_ROOM);
  });

  /**
   * room:create — Create a new room.
   * Payload: { settings: { boardSize, ruleWall, rulePortal, timerMode, timerSeconds } }
   */
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

    // Leave lobby subscription group and join the room's Socket.io room
    socket.leave(LOBBY_ROOM);
    socket.join(room.roomId);

    // Emit to the creator
    socket.emit('room:joined', roomManager.serializeRoom(room));

    // Broadcast updated room list to lobby subscribers
    broadcastLobbyUpdate(io);

    logger.info(`[Socket] ${user.displayName} created room ${room.roomId}`);
  });

  /**
   * room:join — Join an existing room.
   * Payload: { roomId: string }
   */
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

    // Leave lobby, join room
    socket.leave(LOBBY_ROOM);
    socket.join(room.roomId);

    // Emit to the joiner
    socket.emit('room:joined', roomManager.serializeRoom(room));

    // Notify other room members
    socket.to(room.roomId).emit('room:updated', roomManager.serializeRoom(room));

    // Update lobby
    broadcastLobbyUpdate(io);

    logger.info(`[Socket] ${user.displayName} joined room ${room.roomId}`);
  });
}

// =============================================================================
// Disconnect Handler
// =============================================================================

function handleDisconnect(io, socket) {
  const user = socket.user;
  const roomId = roomManager.getRoomIdByUser(user.userId);

  if (!roomId) return; // Not in any room

  const result = roomManager.leaveRoom(user.userId);

  if (result.destroyed) {
    // Room is gone — just update lobby
    broadcastLobbyUpdate(io);
  } else if (result.room) {
    // Notify remaining room members
    io.to(roomId).emit('room:updated', roomManager.serializeRoom(result.room));

    // If host transferred, send a system notification
    if (result.hostTransferred) {
      const newHostUser = result.room.users.get(result.room.host);
      io.to(roomId).emit('room:system_message', {
        message: `${newHostUser ? newHostUser.displayName : '—'} là chủ phòng mới.`,
      });
    }

    broadcastLobbyUpdate(io);
  }
}

// =============================================================================
// Helpers
// =============================================================================

/** Broadcast updated room list to all lobby subscribers. */
function broadcastLobbyUpdate(io) {
  io.to(LOBBY_ROOM).emit('lobby:update', { rooms: roomManager.listRooms() });
}

module.exports = { init };
