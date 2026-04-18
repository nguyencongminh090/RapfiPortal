/**
 * board.js — Canvas-based Board Renderer for GomokuVN.
 *
 * Papers Style: Matches the C++ BoardRenderer from gomoku-portal-ui.
 *   - Background: warm beige (#F5EDDA)
 *   - Grid: thin gray lines with thicker border
 *   - Black pieces: X cross (diagonal, round caps)
 *   - White pieces: O circle (white fill + black outline)
 *   - Walls: brick pattern (7 rows, 3 cols, alternating offset)
 *   - Portals: colored ring with center dot
 *   - Last move: golden yellow highlight square
 *   - Hover: gray semi-transparent rectangle
 *   - Star points: small dots at standard positions
 */

'use strict';

// Portal pair color palette (matches C++ kPortalColors)
const PORTAL_COLORS = [
  { r: 0.20, g: 0.70, b: 0.95 },  // 0: Cyan/Blue
  { r: 0.90, g: 0.40, b: 0.60 },  // 1: Pink/Magenta
  { r: 0.30, g: 0.80, b: 0.40 },  // 2: Green
  { r: 0.95, g: 0.60, b: 0.20 },  // 3: Orange
  { r: 0.65, g: 0.35, b: 0.85 },  // 4: Purple
  { r: 0.20, g: 0.75, b: 0.70 },  // 5: Teal
  { r: 0.85, g: 0.75, b: 0.20 },  // 6: Gold
  { r: 0.85, g: 0.25, b: 0.25 },  // 7: Red
];

// Star point positions for common board sizes
const STAR_POINTS = {
  15: [[3,3],[3,7],[3,11],[7,3],[7,7],[7,11],[11,3],[11,7],[11,11]],
  17: [[3,3],[3,8],[3,13],[8,3],[8,8],[8,13],[13,3],[13,8],[13,13]],
  19: [[3,3],[3,9],[3,15],[9,3],[9,9],[9,15],[15,3],[15,9],[15,15]],
  20: [[3,3],[3,9],[3,16],[9,3],[9,9],[9,16],[16,3],[16,9],[16,16]],
};

class BoardRenderer {
  /**
   * @param {HTMLCanvasElement} canvas
   * @param {{ boardSize: number, onCellClick: (x: number, y: number) => void }} opts
   */
  constructor(canvas, opts = {}) {
    this.canvas = canvas;
    this.ctx = canvas.getContext('2d');
    this.boardSize = opts.boardSize || 17;
    this.onCellClick = opts.onCellClick || null;

    // State (set externally via setState)
    this.board = null;
    this.walls = [];
    this.portals = [];
    this.firstMoveZones = [];
    this.showZones = false;
    this.lastMove = null;
    this.isMyTurn = false;
    this.interactive = false;
    this.myColor = null;

    // Geometry
    this.geo = { cellSize: 0, originX: 0, originY: 0, boardSize: this.boardSize };

    // Hover
    this._hoverCell = null;

    // Double-tap: pending cell awaiting confirmation
    this._pendingCell = null;

    // Bind events
    this.canvas.addEventListener('mousemove', (e) => this._onMouseMove(e));
    this.canvas.addEventListener('mouseleave', () => this._onMouseLeave());
    this.canvas.addEventListener('click', (e) => this._onClick(e));
    // Touch support for mobile
    this.canvas.addEventListener('touchend', (e) => this._onTouchEnd(e));
  }

  /** Update board state and redraw. */
  setState(s) {
    if (s.boardSize !== undefined) this.boardSize = s.boardSize;
    if (s.board !== undefined) this.board = s.board;
    if (s.walls !== undefined) this.walls = s.walls;
    if (s.portals !== undefined) this.portals = s.portals;
    if (s.firstMoveZones !== undefined) this.firstMoveZones = s.firstMoveZones;
    if (s.showZones !== undefined) this.showZones = s.showZones;
    if (s.lastMove !== undefined) this.lastMove = s.lastMove;
    if (s.isMyTurn !== undefined) this.isMyTurn = s.isMyTurn;
    if (s.interactive !== undefined) this.interactive = s.interactive;
    if (s.myColor !== undefined) this.myColor = s.myColor;
    this._draw();
  }

  /** Resize canvas to fit container. */
  resize() {
    const parent = this.canvas.parentElement;
    if (!parent) return;
    const size = Math.min(parent.clientWidth, window.innerHeight - 200);
    const s = Math.max(size, 300);
    this.canvas.width = s;
    this.canvas.height = s;
    this._computeGeometry();
    this._draw();
  }

  // ─── Geometry ────────────────────────────────────────────────────

  _computeGeometry() {
    const w = this.canvas.width;
    const h = this.canvas.height;
    const n = this.boardSize;

    // Margin for coordinate labels
    let margin = Math.min(w, h) * 0.06;
    margin = Math.max(margin, 24);

    const availW = w - 2 * margin;
    const availH = h - 2 * margin;

    // Cell size: N cells fit in available space
    const cellSize = Math.min(availW, availH) / n;

    // Center the grid
    const gridW = n * cellSize;
    const gridH = n * cellSize;
    const originX = margin + (availW - gridW) / 2;
    const originY = margin + (availH - gridH) / 2;

    this.geo = { cellSize, originX, originY, boardSize: n };
  }

  /** Convert cell (x,y) to pixel center of the cell. */
  _cellToPixel(x, y) {
    const g = this.geo;
    return {
      px: g.originX + (x + 0.5) * g.cellSize,
      py: g.originY + (y + 0.5) * g.cellSize,
    };
  }

  /** Convert pixel to cell index. Returns null if out of bounds. */
  _pixelToCell(px, py) {
    const g = this.geo;
    const x = Math.floor((px - g.originX) / g.cellSize);
    const y = Math.floor((py - g.originY) / g.cellSize);
    if (x < 0 || x >= g.boardSize || y < 0 || y >= g.boardSize) return null;
    return { x, y };
  }

  // ─── Event Handlers ──────────────────────────────────────────────

  _getCanvasPos(e) {
    const rect = this.canvas.getBoundingClientRect();
    return {
      x: (e.clientX - rect.left) * (this.canvas.width / rect.width),
      y: (e.clientY - rect.top) * (this.canvas.height / rect.height),
    };
  }

  _onMouseMove(e) {
    if (!this.interactive || !this.isMyTurn) {
      if (this._hoverCell) { this._hoverCell = null; this._draw(); }
      return;
    }
    const pos = this._getCanvasPos(e);
    const cell = this._pixelToCell(pos.x, pos.y);
    const prev = this._hoverCell;
    this._hoverCell = cell;
    if (!prev && !cell) return;
    if (prev && cell && prev.x === cell.x && prev.y === cell.y) return;
    this._draw();
  }

  _onMouseLeave() {
    if (this._hoverCell) { this._hoverCell = null; this._draw(); }
  }

  _onClick(e) {
    if (!this.interactive || !this.isMyTurn || !this.onCellClick) return;
    const pos = this._getCanvasPos(e);
    const cell = this._pixelToCell(pos.x, pos.y);
    if (!cell) return;
    // Check the cell is empty and not a wall/portal
    if (this.board && this.board[cell.y] && this.board[cell.y][cell.x] === 0) {
      this._handleCellSelect(cell.x, cell.y);
    }
  }

  _onTouchEnd(e) {
    if (!this.interactive || !this.isMyTurn || !this.onCellClick) return;
    e.preventDefault();
    const touch = e.changedTouches[0];
    const rect = this.canvas.getBoundingClientRect();
    const px = (touch.clientX - rect.left) * (this.canvas.width / rect.width);
    const py = (touch.clientY - rect.top) * (this.canvas.height / rect.height);
    const cell = this._pixelToCell(px, py);
    if (!cell) return;
    if (this.board && this.board[cell.y] && this.board[cell.y][cell.x] === 0) {
      this._handleCellSelect(cell.x, cell.y);
    }
  }

  /** Double-tap logic: first tap highlights, second tap confirms. */
  _handleCellSelect(x, y) {
    if (this._pendingCell && this._pendingCell.x === x && this._pendingCell.y === y) {
      // Second tap on same cell → confirm
      this._pendingCell = null;
      this.onCellClick(x, y);
    } else {
      // First tap or different cell → set pending
      this._pendingCell = { x, y };
      this._draw();
    }
  }

  /** Clear pending cell (called externally after a move is placed). */
  clearPending() {
    this._pendingCell = null;
  }

  // ─── Main Draw ───────────────────────────────────────────────────

  _draw() {
    const ctx = this.ctx;
    const g = this.geo;
    if (!g.cellSize) return;

    ctx.clearRect(0, 0, this.canvas.width, this.canvas.height);

    // 1. Background + grid
    this._drawBackground();
    this._drawStarPoints();
    this._drawCoordinates();

    // 2. First move zone highlights (behind everything)
    if (this.showZones && this.firstMoveZones && this.firstMoveZones.length > 0) {
      this._drawFirstMoveZones();
    }

    // 3. Last move highlight (behind pieces)
    if (this.lastMove) {
      this._drawLastMoveHighlight(this.lastMove.x, this.lastMove.y);
    }

    // 4. Hover highlight
    if (this._hoverCell && this.board) {
      const hx = this._hoverCell.x, hy = this._hoverCell.y;
      if (this.board[hy] && this.board[hy][hx] === 0) {
        this._drawHoverHighlight(hx, hy);
      }
    }

    // 4b. Pending cell highlight (double-tap preview)
    if (this._pendingCell && this.board) {
      const px = this._pendingCell.x, py = this._pendingCell.y;
      if (this.board[py] && this.board[py][px] === 0) {
        this._drawPendingHighlight(px, py);
      }
    }

    // 5. Draw all cells
    if (this.board) {
      for (let y = 0; y < g.boardSize; y++) {
        for (let x = 0; x < g.boardSize; x++) {
          const val = this.board[y][x];
          if (val === 1) this._drawBlackPiece(x, y);
          else if (val === 2) this._drawWhitePiece(x, y);
          else if (val === -1) this._drawWall(x, y);
          else if (val === -2) this._drawPortal(x, y);
        }
      }
    }
  }

  // ─── Background & Grid ──────────────────────────────────────────

  _drawBackground() {
    const ctx = this.ctx;
    const g = this.geo;
    const gridW = g.boardSize * g.cellSize;
    const gridH = g.boardSize * g.cellSize;

    // Paper-style background: warm beige (#F5EDDA)
    ctx.fillStyle = 'rgb(245, 237, 218)';
    ctx.fillRect(0, 0, this.canvas.width, this.canvas.height);

    // Grid lines
    ctx.strokeStyle = 'rgba(179, 166, 140, 0.6)';
    ctx.lineWidth = 0.8;
    ctx.beginPath();
    for (let i = 0; i <= g.boardSize; i++) {
      // Vertical
      const vx = g.originX + i * g.cellSize;
      ctx.moveTo(vx, g.originY);
      ctx.lineTo(vx, g.originY + gridH);
      // Horizontal
      const hy = g.originY + i * g.cellSize;
      ctx.moveTo(g.originX, hy);
      ctx.lineTo(g.originX + gridW, hy);
    }
    ctx.stroke();

    // Board border (thicker)
    ctx.strokeStyle = 'rgba(140, 128, 102, 0.8)';
    ctx.lineWidth = 1.5;
    ctx.strokeRect(g.originX, g.originY, gridW, gridH);
  }

  // ─── Star Points ────────────────────────────────────────────────

  _drawStarPoints() {
    const ctx = this.ctx;
    const g = this.geo;
    const stars = STAR_POINTS[g.boardSize];
    if (!stars) return;

    ctx.fillStyle = 'rgb(115, 102, 89)';
    const dotR = g.cellSize * 0.08;

    for (const [sx, sy] of stars) {
      const { px, py } = this._cellToPixel(sx, sy);
      ctx.beginPath();
      ctx.arc(px, py, dotR, 0, Math.PI * 2);
      ctx.fill();
    }
  }

  // ─── Coordinates ────────────────────────────────────────────────

  _drawCoordinates() {
    const ctx = this.ctx;
    const g = this.geo;
    const fontSize = g.cellSize * 0.35;
    const labelOffset = g.cellSize * 0.55;

    ctx.fillStyle = 'rgb(77, 77, 77)';
    ctx.font = `bold ${fontSize}px sans-serif`;
    ctx.textAlign = 'center';
    ctx.textBaseline = 'middle';

    // Column letters (A, B, C, ...)
    for (let x = 0; x < g.boardSize; x++) {
      const ch = String.fromCharCode(65 + x);
      const px = g.originX + (x + 0.5) * g.cellSize;
      const py = g.originY - labelOffset;
      ctx.fillText(ch, px, py);
    }

    // Row numbers (1, 2, 3, ...)
    ctx.textAlign = 'center';
    for (let y = 0; y < g.boardSize; y++) {
      const text = String(y + 1);
      const px = g.originX - labelOffset;
      const py = g.originY + (y + 0.5) * g.cellSize;
      ctx.fillText(text, px, py);
    }
  }

  // ─── Highlights ─────────────────────────────────────────────────

  _drawLastMoveHighlight(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);
    const half = g.cellSize * 0.45;

    // Golden yellow highlight square
    ctx.fillStyle = 'rgba(250, 224, 77, 0.75)';
    ctx.fillRect(px - half, py - half, half * 2, half * 2);
  }

  _drawHoverHighlight(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);
    const half = g.cellSize * 0.5;

    ctx.fillStyle = 'rgba(204, 204, 204, 0.5)';
    ctx.fillRect(px - half, py - half, half * 2, half * 2);
  }

  /** Draw pending cell: semi-transparent preview stone + green pulsing ring. */
  _drawPendingHighlight(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);
    const half = g.cellSize * 0.45;

    // Green highlight background
    ctx.fillStyle = 'rgba(72, 135, 95, 0.3)';
    ctx.fillRect(px - half, py - half, half * 2, half * 2);

    // Green border ring
    ctx.strokeStyle = 'rgba(72, 135, 95, 0.8)';
    ctx.lineWidth = 2;
    ctx.strokeRect(px - half, py - half, half * 2, half * 2);

    // Draw semi-transparent preview piece
    const r = g.cellSize * 0.32;
    if (this.myColor === 'BLACK') {
      // X cross preview
      ctx.globalAlpha = 0.45;
      ctx.strokeStyle = '#1A1714';
      ctx.lineWidth = Math.max(g.cellSize * 0.11, 1.5);
      ctx.lineCap = 'round';
      ctx.beginPath();
      ctx.moveTo(px - r, py - r); ctx.lineTo(px + r, py + r);
      ctx.moveTo(px + r, py - r); ctx.lineTo(px - r, py + r);
      ctx.stroke();
      ctx.globalAlpha = 1;
    } else if (this.myColor === 'WHITE') {
      // O circle preview
      ctx.globalAlpha = 0.45;
      ctx.fillStyle = '#FFFFFF';
      ctx.beginPath();
      ctx.arc(px, py, r, 0, Math.PI * 2);
      ctx.fill();
      ctx.strokeStyle = '#1A1714';
      ctx.lineWidth = Math.max(g.cellSize * 0.07, 1);
      ctx.stroke();
      ctx.globalAlpha = 1;
    }
  }

  _drawFirstMoveZones() {
    const ctx = this.ctx;
    const g = this.geo;

    ctx.fillStyle = 'rgba(100, 180, 100, 0.25)';
    for (const z of this.firstMoveZones) {
      const { px, py } = this._cellToPixel(z.x, z.y);
      const half = g.cellSize * 0.48;
      ctx.fillRect(px - half, py - half, half * 2, half * 2);
    }
  }

  // ─── Black Piece — X Cross ──────────────────────────────────────

  _drawBlackPiece(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);

    // Diagonal cross — 60% visual extent
    const arm = g.cellSize * 0.22;
    const lw = g.cellSize * 0.16;

    ctx.strokeStyle = 'rgb(26, 26, 26)';
    ctx.lineWidth = lw;
    ctx.lineCap = 'round';

    ctx.beginPath();
    // Diagonal 1: top-left to bottom-right
    ctx.moveTo(px - arm, py - arm);
    ctx.lineTo(px + arm, py + arm);
    // Diagonal 2: top-right to bottom-left
    ctx.moveTo(px + arm, py - arm);
    ctx.lineTo(px - arm, py + arm);
    ctx.stroke();
  }

  // ─── White Piece — O Circle ─────────────────────────────────────

  _drawWhitePiece(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);

    // Circle — 60% visual extent
    const radius = g.cellSize * 0.26;
    const lw = g.cellSize * 0.08;

    // White fill
    ctx.fillStyle = 'rgb(255, 255, 255)';
    ctx.beginPath();
    ctx.arc(px, py, radius, 0, Math.PI * 2);
    ctx.fill();

    // Bold black outline
    ctx.strokeStyle = 'rgb(26, 26, 26)';
    ctx.lineWidth = lw;
    ctx.beginPath();
    ctx.arc(px, py, radius, 0, Math.PI * 2);
    ctx.stroke();
  }

  // ─── Wall — Brick Pattern ──────────────────────────────────────

  _drawWall(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);
    const half = g.cellSize * 0.45;
    const w = half * 2;
    const h = half * 2;
    const left = px - half;
    const top = py - half;

    const rows = 7;
    const cols = 3;
    const rowH = h / rows;
    const colW = w / cols;

    // Brick background
    ctx.fillStyle = 'rgb(204, 89, 64)';
    ctx.fillRect(left, top, w, h);

    // Mortar lines
    ctx.strokeStyle = 'rgb(230, 217, 191)';
    ctx.lineWidth = 1.0;

    // Horizontal mortar
    ctx.beginPath();
    for (let r = 1; r < rows; r++) {
      const ly = top + r * rowH;
      ctx.moveTo(left, ly);
      ctx.lineTo(left + w, ly);
    }
    ctx.stroke();

    // Vertical mortar (offset every other row)
    ctx.beginPath();
    for (let r = 0; r < rows; r++) {
      const y0 = top + r * rowH;
      const y1 = y0 + rowH;
      const xOff = (r % 2 === 0) ? 0 : colW * 0.5;

      for (let c = 1; c < cols; c++) {
        const lx = left + c * colW + xOff;
        if (lx > left && lx < left + w) {
          ctx.moveTo(lx, y0);
          ctx.lineTo(lx, y1);
        }
      }
      // Offset line on odd rows
      if (r % 2 !== 0) {
        const lx = left + colW * 0.5;
        ctx.moveTo(lx, y0);
        ctx.lineTo(lx, y1);
      }
    }
    ctx.stroke();

    // Subtle border
    ctx.strokeStyle = 'rgba(128, 51, 38, 0.6)';
    ctx.lineWidth = 1.0;
    ctx.strokeRect(left, top, w, h);
  }

  // ─── Portal — Colored Ring with Center Dot ─────────────────────

  _drawPortal(x, y) {
    const ctx = this.ctx;
    const g = this.geo;
    const { px, py } = this._cellToPixel(x, y);

    // Find which portal pair this belongs to
    let pairIdx = 0;
    if (this.portals) {
      for (let i = 0; i < this.portals.length; i++) {
        const p = this.portals[i];
        if ((p.a.x === x && p.a.y === y) || (p.b.x === x && p.b.y === y)) {
          pairIdx = i;
          break;
        }
      }
    }

    const col = PORTAL_COLORS[pairIdx % PORTAL_COLORS.length];
    const ringRadius = g.cellSize * 0.32;
    const ringWidth = g.cellSize * 0.08;
    const dotRadius = g.cellSize * 0.08;

    // Outer glow
    ctx.fillStyle = `rgba(${col.r*255|0}, ${col.g*255|0}, ${col.b*255|0}, 0.25)`;
    ctx.beginPath();
    ctx.arc(px, py, ringRadius + ringWidth, 0, Math.PI * 2);
    ctx.fill();

    // Main ring
    ctx.strokeStyle = `rgb(${col.r*255|0}, ${col.g*255|0}, ${col.b*255|0})`;
    ctx.lineWidth = ringWidth;
    ctx.beginPath();
    ctx.arc(px, py, ringRadius, 0, Math.PI * 2);
    ctx.stroke();

    // Center dot
    ctx.fillStyle = 'rgb(38, 38, 38)';
    ctx.beginPath();
    ctx.arc(px, py, dotRadius, 0, Math.PI * 2);
    ctx.fill();
  }
}
