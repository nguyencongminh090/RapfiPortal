'use strict';

/**
 * ChatHandler.js — Per-room chat with rate limiting and sanitization.
 *
 * Rate limit: max CHAT_RATE_LIMIT messages per CHAT_RATE_WINDOW_MS per user.
 * Sanitization: strip all HTML tags from messages.
 *
 * Manual test checklist:
 *   [ ] Message broadcast reaches all room members
 *   [ ] Rate limit blocks 6th message within 3s window
 *   [ ] HTML tags are stripped from messages
 *   [ ] Empty messages are rejected
 *   [ ] Messages longer than 500 chars are truncated
 */

const config = require('../config');
const logger = require('../utils/logger');

// Per-user sliding window: userId → [timestamp, timestamp, ...]
const rateLimitMap = new Map();

const MAX_MESSAGE_LENGTH = 500;

/**
 * Check if a user is rate-limited.
 * Uses a sliding window: keep only timestamps within the last CHAT_RATE_WINDOW_MS.
 *
 * @param {string} userId
 * @returns {boolean} true if the message should be BLOCKED
 */
function isRateLimited(userId) {
  const now = Date.now();
  let timestamps = rateLimitMap.get(userId);

  if (!timestamps) {
    timestamps = [];
    rateLimitMap.set(userId, timestamps);
  }

  // Remove timestamps outside the window
  const cutoff = now - config.CHAT_RATE_WINDOW_MS;
  while (timestamps.length > 0 && timestamps[0] < cutoff) {
    timestamps.shift();
  }

  // Check limit
  if (timestamps.length >= config.CHAT_RATE_LIMIT) {
    return true; // Blocked
  }

  // Record this message
  timestamps.push(now);
  return false;
}

/**
 * Strip all HTML tags from a string.
 * @param {string} str
 * @returns {string}
 */
function sanitize(str) {
  if (typeof str !== 'string') return '';
  return str.replace(/<[^>]*>/g, '').trim();
}

/**
 * Process and broadcast a chat message to a room.
 *
 * @param {import('socket.io').Server} io
 * @param {import('socket.io').Socket} socket
 * @param {string} roomId
 * @param {string} text — raw message text from client
 */
function handleMessage(io, socket, roomId, text) {
  const user = socket.user;

  // Validate
  const clean = sanitize(text);
  if (!clean || clean.length === 0) {
    return; // Silently ignore empty messages
  }

  // Truncate
  const truncated = clean.length > MAX_MESSAGE_LENGTH
    ? clean.slice(0, MAX_MESSAGE_LENGTH) + '…'
    : clean;

  // Rate limit
  if (isRateLimited(user.userId)) {
    socket.emit('chat:error', {
      message: 'Bạn đang gửi tin nhắn quá nhanh. Vui lòng chờ một chút.',
    });
    return;
  }

  const payload = {
    from: user.displayName,
    fromId: user.userId,
    text: truncated,
    timestamp: Date.now(),
  };

  // Broadcast to all room members (including sender)
  io.to(roomId).emit('chat:message', payload);
}

/**
 * Clean up rate limit data for a user (on disconnect).
 * @param {string} userId
 */
function cleanupUser(userId) {
  rateLimitMap.delete(userId);
}

module.exports = { handleMessage, cleanupUser, sanitize };
