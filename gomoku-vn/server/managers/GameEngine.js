'use strict';

/**
 * GameEngine.js — Core game logic for GomokuVN.
 *
 * Responsibilities:
 *   - Board initialization with walls and portals
 *   - Move validation (turn, bounds, cell type)
 *   - Win detection (FreeStyle: 5+ in a row, including portal traversal)
 *   - Draw detection (board full)
 *   - First-move zone enforcement (WALL rule)
 *   - Draw offer/accept/decline state
 *   - Resign handling
 *
 * Board cell values:
 *   0  = empty
 *   1  = black stone
 *   2  = white stone
 *  -1  = wall
 *  -2  = portal
 *
 * Manual test checklist:
 *   [ ] Board initializes with correct dimensions
 *   [ ] Walls placed at correct positions on board
 *   [ ] Portals placed at correct positions on board
 *   [ ] Move validation rejects: wrong turn, OOB, occupied, wall/portal
 *   [ ] First move must be in firstMoveZone (when walls exist)
 *   [ ] Win detection: horizontal, vertical, both diagonals
 *   [ ] Win detection with portal traversal (5+ distinct stones)
 *   [ ] Draw: board full with no winner
 *   [ ] Resign: immediate game end
 */

const { v4: uuidv4 } = require('uuid');
const logger = require('../utils/logger');

// Board cell constants
const EMPTY  =  0;
const BLACK  =  1;
const WHITE  =  2;
const WALL   = -1;
const PORTAL = -2;

// 4 line directions: horizontal, vertical, diagonal (\), anti-diagonal (/)
const DIRECTIONS = [
  { dx: 1, dy: 0 },  // horizontal →
  { dx: 0, dy: 1 },  // vertical ↓
  { dx: 1, dy: 1 },  // diagonal ↘
  { dx: 1, dy: -1 }, // anti-diagonal ↗
];

// ---------------------------------------------------------------------------
// GameEngine class
// ---------------------------------------------------------------------------

class GameEngine {
  /**
   * Create a new game.
   *
   * @param {object} opts
   * @param {string} opts.roomId
   * @param {number} opts.boardSize
   * @param {Array<{userId:string, displayName:string, color:string}>} opts.players
   *   players[0] = slot 1 (BLACK), players[1] = slot 2 (WHITE)
   * @param {Array<{x:number,y:number}>} opts.walls
   * @param {Array<{a:{x:number,y:number}, b:{x:number,y:number}}>} opts.portals
   * @param {Array<{x:number,y:number}>} opts.firstMoveZones
   */
  constructor(opts) {
    this.gameId    = uuidv4();
    this.roomId    = opts.roomId;
    this.boardSize = opts.boardSize;
    this.players   = opts.players; // [{ userId, displayName, color: 'BLACK'|'WHITE' }]
    this.walls     = opts.walls || [];
    this.portals   = opts.portals || [];
    this.firstMoveZones = opts.firstMoveZones || [];

    // Initialize board
    this.board = this._createBoard();

    // Place walls and portals on the board
    for (const w of this.walls) {
      this.board[w.y][w.x] = WALL;
    }
    for (const p of this.portals) {
      this.board[p.a.y][p.a.x] = PORTAL;
      this.board[p.b.y][p.b.x] = PORTAL;
    }

    // Build portal lookup map: "x,y" → partner position
    // Bidirectional: A→B and B→A
    this._portalMap = new Map();
    for (const p of this.portals) {
      this._portalMap.set(`${p.a.x},${p.a.y}`, p.b);
      this._portalMap.set(`${p.b.x},${p.b.y}`, p.a);
    }

    // Game state
    this.currentTurn = this.players[0].userId; // BLACK goes first
    this.moveHistory = [];
    this.moveCount   = 0;
    this.status      = 'ongoing'; // 'ongoing' | 'finished'
    this.result      = null;      // { winner, reason }
    this.drawOffer   = null;      // { from: userId } or null

    logger.info(`[GameEngine] Game ${this.gameId} created for room ${this.roomId}`);
  }

  // ---------------------------------------------------------------------------
  // Move
  // ---------------------------------------------------------------------------

  /**
   * Validate and apply a move.
   *
   * @param {string} userId
   * @param {number} x — column index (0-based)
   * @param {number} y — row index (0-based)
   * @returns {{ error?: string, won?: boolean, draw?: boolean, color?: string, nextTurn?: string }}
   */
  makeMove(userId, x, y) {
    // Game must be ongoing
    if (this.status !== 'ongoing') {
      return { error: 'Ván đấu đã kết thúc.' };
    }

    // Must be this player's turn
    if (userId !== this.currentTurn) {
      return { error: 'Chưa đến lượt bạn.' };
    }

    // Bounds check
    if (x < 0 || x >= this.boardSize || y < 0 || y >= this.boardSize) {
      return { error: 'Vị trí ngoài bàn cờ.' };
    }

    // Cell must be empty
    const cell = this.board[y][x];
    if (cell !== EMPTY) {
      if (cell === WALL) return { error: 'Không thể đặt vào ô tường.' };
      if (cell === PORTAL) return { error: 'Không thể đặt vào ô cổng dịch chuyển.' };
      return { error: 'Ô này đã có quân.' };
    }

    // First-move zone enforcement (only when walls exist)
    if (this.walls.length > 0 && this.moveCount === 0) {
      const inZone = this.firstMoveZones.some(z => z.x === x && z.y === y);
      if (!inZone) {
        return { error: 'Nước đầu tiên phải đặt cạnh một ô tường.' };
      }
    }

    // Determine stone color
    const player = this.players.find(p => p.userId === userId);
    const color = player.color === 'BLACK' ? BLACK : WHITE;
    const colorStr = player.color;

    // Place stone
    this.board[y][x] = color;
    this.moveCount++;
    this.moveHistory.push({
      x, y,
      color: colorStr,
      timestamp: Date.now(),
    });

    // Cancel any pending draw offer on move
    this.drawOffer = null;

    // Check win
    const won = this._checkWin(x, y, color);
    if (won) {
      this.status = 'finished';
      this.result = { winner: userId, reason: 'win' };
      logger.info(`[GameEngine] Game ${this.gameId}: ${player.displayName} wins!`);
      return { won: true, color: colorStr, nextTurn: null };
    }

    // Check draw (board full)
    if (this._isBoardFull()) {
      this.status = 'finished';
      this.result = { winner: 'draw', reason: 'board_full' };
      logger.info(`[GameEngine] Game ${this.gameId}: Draw (board full)`);
      return { draw: true, color: colorStr, nextTurn: null };
    }

    // Switch turn
    const nextPlayer = this.players.find(p => p.userId !== userId);
    this.currentTurn = nextPlayer.userId;

    return {
      color: colorStr,
      nextTurn: nextPlayer.userId,
    };
  }

  // ---------------------------------------------------------------------------
  // Resign
  // ---------------------------------------------------------------------------

  /**
   * Player resigns. The other player wins.
   *
   * @param {string} userId
   * @returns {{ error?: string, winner?: string, loser?: string }}
   */
  resign(userId) {
    if (this.status !== 'ongoing') {
      return { error: 'Ván đấu đã kết thúc.' };
    }

    const player = this.players.find(p => p.userId === userId);
    if (!player) return { error: 'Bạn không phải người chơi.' };

    const opponent = this.players.find(p => p.userId !== userId);

    this.status = 'finished';
    this.result = { winner: opponent.userId, reason: 'resign' };

    logger.info(`[GameEngine] Game ${this.gameId}: ${player.displayName} resigned`);
    return { winner: opponent.userId, loser: userId };
  }

  // ---------------------------------------------------------------------------
  // Draw offer
  // ---------------------------------------------------------------------------

  /**
   * Offer a draw. Only the player whose turn it is can offer.
   * Only one pending draw offer may exist at a time.
   *
   * @param {string} userId
   * @returns {{ error?: string, offered?: boolean }}
   */
  offerDraw(userId) {
    if (this.status !== 'ongoing') return { error: 'Ván đấu đã kết thúc.' };

    const player = this.players.find(p => p.userId === userId);
    if (!player) return { error: 'Bạn không phải người chơi.' };

    if (this.drawOffer) return { error: 'Đã có lời đề nghị hoà đang chờ.' };

    this.drawOffer = { from: userId };
    return { offered: true };
  }

  /**
   * Accept a pending draw offer. Only the opponent can accept.
   *
   * @param {string} userId
   * @returns {{ error?: string, accepted?: boolean }}
   */
  acceptDraw(userId) {
    if (this.status !== 'ongoing') return { error: 'Ván đấu đã kết thúc.' };
    if (!this.drawOffer) return { error: 'Không có lời đề nghị hoà nào.' };
    if (this.drawOffer.from === userId) return { error: 'Bạn không thể tự chấp nhận.' };

    this.status = 'finished';
    this.result = { winner: 'draw', reason: 'agreement' };
    this.drawOffer = null;

    logger.info(`[GameEngine] Game ${this.gameId}: Draw by agreement`);
    return { accepted: true };
  }

  /**
   * Decline a pending draw offer.
   *
   * @param {string} userId
   * @returns {{ error?: string, declined?: boolean }}
   */
  declineDraw(userId) {
    if (!this.drawOffer) return { error: 'Không có lời đề nghị hoà nào.' };
    if (this.drawOffer.from === userId) return { error: 'Bạn không thể tự từ chối.' };

    this.drawOffer = null;
    return { declined: true };
  }

  // ---------------------------------------------------------------------------
  // Timeout
  // ---------------------------------------------------------------------------

  /**
   * Handle timeout. The timed-out player loses.
   *
   * @param {string} timedOutPlayerId
   */
  handleTimeout(timedOutPlayerId) {
    if (this.status !== 'ongoing') return;

    const opponent = this.players.find(p => p.userId !== timedOutPlayerId);
    this.status = 'finished';
    this.result = { winner: opponent.userId, reason: 'timeout' };

    logger.info(`[GameEngine] Game ${this.gameId}: ${timedOutPlayerId} timed out`);
  }

  // ---------------------------------------------------------------------------
  // Serialization
  // ---------------------------------------------------------------------------

  /** Serialize full game state for the game:init payload. */
  serialize() {
    return {
      gameId: this.gameId,
      roomId: this.roomId,
      boardSize: this.boardSize,
      board: this.board,
      players: this.players.map(p => ({
        userId: p.userId,
        displayName: p.displayName,
        color: p.color,
      })),
      walls: this.walls,
      portals: this.portals,
      firstMoveZones: this.firstMoveZones,
      currentTurn: this.currentTurn,
      moveHistory: this.moveHistory,
      moveCount: this.moveCount,
      status: this.status,
      result: this.result,
    };
  }

  // ---------------------------------------------------------------------------
  // Win detection
  // ---------------------------------------------------------------------------

  /**
   * Check if placing a stone at (x, y) of given color results in a win.
   * FreeStyle rule: 5 or more consecutive stones.
   * Includes portal traversal for extended lines.
   *
   * @param {number} x
   * @param {number} y
   * @param {number} color — BLACK (1) or WHITE (2)
   * @returns {boolean}
   */
  _checkWin(x, y, color) {
    for (const dir of DIRECTIONS) {
      const count = 1
        + this._countInDirection(x, y, dir.dx, dir.dy, color)
        + this._countInDirection(x, y, -dir.dx, -dir.dy, color);

      if (count >= 5) return true;
    }
    return false;
  }

  /**
   * Count consecutive stones of the given color in one direction,
   * with portal traversal support.
   *
   * Portal traversal: when the next cell is a PORTAL, teleport to the partner
   * and continue in the same direction. The portal cell itself does NOT count
   * as a stone. Prevents infinite loops by tracking visited positions.
   *
   * @param {number} startX — origin x (not counted)
   * @param {number} startY — origin y (not counted)
   * @param {number} dx — direction x
   * @param {number} dy — direction y
   * @param {number} color
   * @returns {number} count of consecutive same-color stones
   */
  _countInDirection(startX, startY, dx, dy, color) {
    let count = 0;
    let cx = startX + dx;
    let cy = startY + dy;

    // Track visited portal cells to prevent infinite loops
    const visited = new Set();
    visited.add(`${startX},${startY}`);

    while (true) {
      // Bounds check
      if (cx < 0 || cx >= this.boardSize || cy < 0 || cy >= this.boardSize) break;

      const posKey = `${cx},${cy}`;

      // Prevent infinite loop through portals
      if (visited.has(posKey)) break;
      visited.add(posKey);

      const cell = this.board[cy][cx];

      if (cell === color) {
        // Same color stone — count it and continue
        count++;
        cx += dx;
        cy += dy;
      } else if (cell === PORTAL) {
        // Portal cell — teleport to partner and continue in same direction
        // Portal itself is zero-width (not counted as a stone)
        const partner = this._portalMap.get(`${cx},${cy}`);
        if (!partner) break; // No partner? Shouldn't happen, but be safe

        const partnerKey = `${partner.x},${partner.y}`;
        if (visited.has(partnerKey)) break; // Already visited partner
        visited.add(partnerKey);

        // Continue from after the partner portal in the same direction
        cx = partner.x + dx;
        cy = partner.y + dy;
      } else {
        // Wall, empty, or opponent stone — stop
        break;
      }
    }

    return count;
  }

  // ---------------------------------------------------------------------------
  // Draw detection
  // ---------------------------------------------------------------------------

  /** Check if the board is completely full (no empty cells). */
  _isBoardFull() {
    for (let y = 0; y < this.boardSize; y++) {
      for (let x = 0; x < this.boardSize; x++) {
        if (this.board[y][x] === EMPTY) return false;
      }
    }
    return true;
  }

  // ---------------------------------------------------------------------------
  // Internal helpers
  // ---------------------------------------------------------------------------

  /** Create an empty board of the given size. */
  _createBoard() {
    const board = [];
    for (let y = 0; y < this.boardSize; y++) {
      board.push(new Array(this.boardSize).fill(EMPTY));
    }
    return board;
  }
}

// Export class and cell constants
module.exports = { GameEngine, EMPTY, BLACK, WHITE, WALL, PORTAL };
