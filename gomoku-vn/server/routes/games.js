'use strict';

/**
 * games.js — REST API routes for game history.
 *
 * GET /api/games          — list recent games (paginated)
 * GET /api/games/:id      — get single game with full move data
 */

const express  = require('express');
const database = require('../db/database');

const router = express.Router();

// ---------------------------------------------------------------------------
// GET /api/games — List recent games
// ---------------------------------------------------------------------------
router.get('/', (req, res) => {
  try {
    const page  = Math.max(1, parseInt(req.query.page) || 1);
    const limit = Math.min(50, Math.max(1, parseInt(req.query.limit) || 20));
    const offset = (page - 1) * limit;

    const games = database.getRecentGames(limit, offset);
    const total = database.getGameCount();

    res.json({
      games,
      pagination: {
        page,
        limit,
        total,
        totalPages: Math.ceil(total / limit),
      },
    });
  } catch (err) {
    res.status(500).json({ error: 'Lỗi khi tải lịch sử ván đấu.' });
  }
});

// ---------------------------------------------------------------------------
// GET /api/games/:id — Get single game with full details
// ---------------------------------------------------------------------------
router.get('/:id', (req, res) => {
  try {
    const game = database.getGameById(req.params.id);
    if (!game) {
      return res.status(404).json({ error: 'Không tìm thấy ván đấu.' });
    }

    // Parse JSON fields
    game.moves   = JSON.parse(game.moves   || '[]');
    game.walls   = JSON.parse(game.walls   || '[]');
    game.portals = JSON.parse(game.portals || '[]');

    res.json({ game });
  } catch (err) {
    res.status(500).json({ error: 'Lỗi khi tải chi tiết ván đấu.' });
  }
});

module.exports = router;
