'use strict';

/**
 * auth.js — Authentication middleware.
 *
 * verifyToken(req, res, next)  — Express middleware: verifies JWT in
 *   Authorization header or req.body.token, attaches req.user on success.
 *
 * verifySocketToken(socket, next) — Socket.io middleware: reads token from
 *   socket.handshake.auth.token, attaches socket.user on success.
 */

const jwt    = require('jsonwebtoken');
const config = require('../config');
const logger = require('../utils/logger');

// ---------------------------------------------------------------------------
// Express middleware
// ---------------------------------------------------------------------------

/**
 * Verifies Bearer JWT from Authorization header.
 * Attaches decoded payload to req.user.
 * Responds 401 if token is missing or invalid.
 */
function verifyToken(req, res, next) {
  const authHeader = req.headers['authorization'];
  const token = authHeader && authHeader.startsWith('Bearer ')
    ? authHeader.slice(7)
    : null;

  if (!token) {
    return res.status(401).json({ error: 'Cần đăng nhập để thực hiện thao tác này.' });
  }

  try {
    req.user = jwt.verify(token, config.JWT_SECRET);
    next();
  } catch (err) {
    logger.warn('[Auth] Invalid token:', err.message);
    return res.status(401).json({ error: 'Phiên đăng nhập không hợp lệ. Vui lòng đăng nhập lại.' });
  }
}

// ---------------------------------------------------------------------------
// Socket.io middleware
// ---------------------------------------------------------------------------

/**
 * Reads JWT from socket.handshake.auth.token.
 * Attaches decoded payload to socket.user.
 * Calls next(new Error(...)) if token is missing or invalid — this causes
 * Socket.io to reject the connection.
 */
function verifySocketToken(socket, next) {
  const token = socket.handshake.auth && socket.handshake.auth.token;

  if (!token) {
    logger.warn('[Auth] Socket connection without token from', socket.handshake.address);
    return next(new Error('AUTH_REQUIRED'));
  }

  try {
    socket.user = jwt.verify(token, config.JWT_SECRET);
    next();
  } catch (err) {
    logger.warn('[Auth] Socket invalid token:', err.message);
    return next(new Error('AUTH_INVALID'));
  }
}

module.exports = { verifyToken, verifySocketToken };
