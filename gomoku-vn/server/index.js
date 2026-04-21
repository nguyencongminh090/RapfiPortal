'use strict';

/**
 * index.js — GomokuVN Express + Socket.io entry point.
 *
 * Responsibilities:
 *   1. Create Express app + HTTP server
 *   2. Serve static client files from /client
 *   3. Mount REST routes (/api/auth)
 *   4. Initialize Socket.io with auth middleware
 *   5. Wire SocketHandler
 *   6. Start listening
 */

const express      = require('express');
const http         = require('http');
const { Server }   = require('socket.io');
const path         = require('path');

const config         = require('./config');
const logger         = require('./utils/logger');
const authRouter     = require('./routes/auth');
const { verifySocketToken } = require('./middleware/auth');
const socketHandler  = require('./socket/SocketHandler');

// ---------------------------------------------------------------------------
// Express
// ---------------------------------------------------------------------------
const app = express();

// Parse JSON bodies for REST endpoints
app.use(express.json());

// Serve client static files
app.use(express.static(path.join(__dirname, '..', 'client')));

// REST API routes
app.use('/api/auth', authRouter);

// Catch-all: serve login page for unknown routes (SPA-style fallback)
app.get('*', (req, res) => {
  // If not an API request, redirect to login
  if (!req.path.startsWith('/api')) {
    res.sendFile(path.join(__dirname, '..', 'client', 'login.html'));
  }
});

// ---------------------------------------------------------------------------
// HTTP Server
// ---------------------------------------------------------------------------
const server = http.createServer(app);

// ---------------------------------------------------------------------------
// Socket.io
// ---------------------------------------------------------------------------
const io = new Server(server, {
  cors: {
    origin: '*',   // Adjust in production (e.g. Cloudflare tunnel domain)
    methods: ['GET', 'POST'],
  },
});

// Apply JWT auth middleware to ALL socket connections
io.use(verifySocketToken);

// Wire up event handlers
socketHandler.init(io);

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
server.listen(config.HTTP_PORT, () => {
  logger.info(`[Server] Play3CR listening on http://localhost:${config.HTTP_PORT}`);
});

module.exports = { app, server, io };
