'use strict';

/**
 * SocketHandler.js — Stub entry point for all Socket.io event routing.
 *
 * This file will grow in Phase 2-5. For now it handles:
 *   - connection / disconnect logging
 *   - auth error forwarding to client
 *
 * Each domain (lobby, room, game, chat) will be wired here as it is implemented.
 */

const logger = require('../utils/logger');

/**
 * Initialize the Socket.io event handler.
 * @param {import('socket.io').Server} io
 */
function init(io) {
  io.on('connection', (socket) => {
    const user = socket.user;
    logger.info(`[Socket] Connected: ${user.displayName} (${user.userId}) sid=${socket.id}`);

    socket.on('disconnect', (reason) => {
      logger.info(`[Socket] Disconnected: ${user.displayName} (${user.userId}) reason=${reason}`);
    });

    // Placeholder: future handlers will be registered here
    // e.g. lobbyHandler(io, socket);
    //      roomHandler(io, socket);
    //      gameHandler(io, socket);
    //      chatHandler(io, socket);
  });

  // Relay Socket.io auth errors back to client before the connection is established
  io.engine.on('connection_error', (err) => {
    logger.warn('[Socket] Connection error:', err.message);
  });
}

module.exports = { init };
