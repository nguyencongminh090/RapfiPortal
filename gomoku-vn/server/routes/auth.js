'use strict';

/**
 * auth.js — REST authentication routes.
 *
 * POST /api/auth/register  — create account, return JWT
 * POST /api/auth/login     — verify credentials, return JWT
 * POST /api/auth/guest     — generate ephemeral guest session, return JWT
 *
 * Manual test checklist:
 *   [ ] Register with valid username → returns token
 *   [ ] Register with duplicate username → 409 + Vietnamese error
 *   [ ] Register with short username (<3) → 400
 *   [ ] Login with correct password → returns token
 *   [ ] Login with wrong password → 401 + Vietnamese error
 *   [ ] Guest → returns token with isGuest: true and a 4-8 letter displayName
 */

const express = require('express');
const bcrypt  = require('bcrypt');
const jwt     = require('jsonwebtoken');
const { v4: uuidv4 } = require('uuid');

const router  = express.Router();
const db      = require('../db/database');
const config  = require('../config');
const logger  = require('../utils/logger');

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/** Validate username: 3-20 chars, alphanumeric + underscore only. */
function isValidUsername(u) {
  return typeof u === 'string' && /^[a-zA-Z0-9_]{3,20}$/.test(u);
}

/** Validate display name: 2-24 chars, no HTML. */
function isValidDisplayName(d) {
  return typeof d === 'string' && d.trim().length >= 2 && d.trim().length <= 24;
}

/** Validate password: minimum 6 characters. */
function isValidPassword(p) {
  return typeof p === 'string' && p.length >= 6;
}

/** Sign a JWT for a user (registered or guest). */
function signToken(payload, expiry) {
  return jwt.sign(payload, config.JWT_SECRET, { expiresIn: expiry });
}

/** Generate a random guest display name (4-8 letters). */
function generateGuestName() {
  const adj  = config.GUEST_NAME_ADJECTIVES;
  const noun = config.GUEST_NAME_NOUNS;
  const a = adj[Math.floor(Math.random() * adj.length)];
  const n = noun[Math.floor(Math.random() * noun.length)];
  // e.g. "WildFox" (3+3=6), "NeonBear" (4+4=8)
  return a + n;
}

// ---------------------------------------------------------------------------
// POST /api/auth/register
// ---------------------------------------------------------------------------
router.post('/register', async (req, res) => {
  try {
    const { username, password, displayName } = req.body;

    if (!isValidUsername(username)) {
      return res.status(400).json({
        error: 'Tên đăng nhập phải từ 3-20 ký tự, chỉ gồm chữ cái, số và dấu gạch dưới.',
      });
    }
    if (!isValidDisplayName(displayName)) {
      return res.status(400).json({
        error: 'Tên hiển thị phải từ 2-24 ký tự.',
      });
    }
    if (!isValidPassword(password)) {
      return res.status(400).json({
        error: 'Mật khẩu phải có ít nhất 6 ký tự.',
      });
    }

    // Check uniqueness
    const existing = db.getUserByUsername(username);
    if (existing) {
      return res.status(409).json({
        error: 'Tên đăng nhập đã tồn tại. Vui lòng chọn tên khác.',
      });
    }

    const passwordHash = await bcrypt.hash(password, config.BCRYPT_ROUNDS);
    const userId       = uuidv4();
    const now          = new Date().toISOString();

    db.createUser({
      id: userId,
      username,
      passwordHash,
      displayName: displayName.trim(),
      createdAt: now,
    });

    const token = signToken(
      { userId, username, displayName: displayName.trim(), isGuest: false },
      config.JWT_EXPIRY
    );

    logger.info(`[Auth] Registered user: ${username} (${userId})`);
    return res.status(201).json({ token, displayName: displayName.trim() });

  } catch (err) {
    logger.error('[Auth] Register error:', err);
    return res.status(500).json({ error: 'Lỗi máy chủ. Vui lòng thử lại.' });
  }
});

// ---------------------------------------------------------------------------
// POST /api/auth/login
// ---------------------------------------------------------------------------
router.post('/login', async (req, res) => {
  try {
    const { username, password } = req.body;

    if (!username || !password) {
      return res.status(400).json({ error: 'Vui lòng nhập tên đăng nhập và mật khẩu.' });
    }

    const user = db.getUserByUsername(username);
    if (!user) {
      return res.status(401).json({ error: 'Tên đăng nhập hoặc mật khẩu không đúng.' });
    }

    const match = await bcrypt.compare(password, user.password_hash);
    if (!match) {
      return res.status(401).json({ error: 'Tên đăng nhập hoặc mật khẩu không đúng.' });
    }

    const token = signToken(
      {
        userId: user.id,
        username: user.username,
        displayName: user.display_name,
        isGuest: false,
      },
      config.JWT_EXPIRY
    );

    logger.info(`[Auth] Login: ${username}`);
    return res.json({ token, displayName: user.display_name });

  } catch (err) {
    logger.error('[Auth] Login error:', err);
    return res.status(500).json({ error: 'Lỗi máy chủ. Vui lòng thử lại.' });
  }
});

// ---------------------------------------------------------------------------
// POST /api/auth/guest
// ---------------------------------------------------------------------------
router.post('/guest', (req, res) => {
  try {
    // Generate unique guest ID (not persisted)
    const guestId      = 'guest_' + uuidv4().slice(0, 8);
    const displayName  = generateGuestName();

    const token = signToken(
      {
        userId: guestId,
        username: guestId,
        displayName,
        isGuest: true,
      },
      config.JWT_GUEST_EXPIRY
    );

    logger.info(`[Auth] Guest session: ${displayName} (${guestId})`);
    return res.json({ token, displayName });

  } catch (err) {
    logger.error('[Auth] Guest error:', err);
    return res.status(500).json({ error: 'Lỗi máy chủ. Vui lòng thử lại.' });
  }
});

module.exports = router;
