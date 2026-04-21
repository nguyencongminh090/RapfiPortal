/**
 * move-tree.js — MoveNode + MoveTree for variation analysis.
 *
 * Core data model for a game tree where:
 *   - Each node represents one move (or the root = empty board)
 *   - children[0] is always the "main line"
 *   - Additional children are variations/branches
 *   - Navigation: forward/back/jump to any node
 *   - Editing: add move, delete subtree, promote variation
 *
 * Board cell values (same as GameEngine):
 *   0 = empty, 1 = black, 2 = white, -1 = wall, -2 = portal
 */

'use strict';

// ---------------------------------------------------------------------------
// Coordinate notation helpers
// ---------------------------------------------------------------------------
const COL_LABELS = 'ABCDEFGHJKLMNOPQRST'; // Skip 'I' (Gomoku convention)

function coordToLabel(x, y) {
  const col = x < COL_LABELS.length ? COL_LABELS[x] : '?';
  const row = y + 1;
  return `${col}${row}`;
}

// ---------------------------------------------------------------------------
// MoveNode
// ---------------------------------------------------------------------------
let _nodeIdCounter = 0;

class MoveNode {
  /**
   * @param {{ x: number, y: number, color: string }|null} move
   * @param {MoveNode|null} parent
   */
  constructor(move, parent = null) {
    this.id = ++_nodeIdCounter;
    this.move = move;             // { x, y, color:'BLACK'|'WHITE' } or null for root
    this.parent = parent;
    this.children = [];
    this.comment = '';

    // Computed: depth from root (root = 0, first move = 1, etc.)
    this.depth = parent ? parent.depth + 1 : 0;
  }

  /** Is this the root node (empty board)? */
  get isRoot() { return this.move === null; }

  /** Is this a leaf node (no children)? */
  get isLeaf() { return this.children.length === 0; }

  /** Is this a variation (not the first child of its parent)? */
  get isVariation() {
    if (!this.parent) return false;
    return this.parent.children[0] !== this;
  }

  /** Get variation index (0 = main line). */
  get variationIndex() {
    if (!this.parent) return 0;
    return this.parent.children.indexOf(this);
  }

  /** Human-readable label: "3.F10" or "3a.H8" for variations. */
  get label() {
    if (this.isRoot) return 'Root';
    const num = this.depth;
    const coord = coordToLabel(this.move.x, this.move.y);
    if (!this.isVariation) return `${num}.${coord}`;
    // Variation letter: a, b, c, ...
    const varIdx = this.variationIndex;
    const letter = String.fromCharCode(96 + varIdx); // 1→a, 2→b, ...
    return `${num}${letter}.${coord}`;
  }

  /** Short label for tree display: just the coordinate. */
  get shortLabel() {
    if (this.isRoot) return '⊙';
    return coordToLabel(this.move.x, this.move.y);
  }
}

// ---------------------------------------------------------------------------
// MoveTree
// ---------------------------------------------------------------------------

class MoveTree {
  /**
   * @param {{ walls: Array, portals: Array, boardSize: number }} opts
   */
  constructor(opts = {}) {
    this.root = new MoveNode(null, null);
    this.currentNode = this.root;
    this.boardSize = opts.boardSize || 17;
    this.walls = opts.walls || [];
    this.portals = opts.portals || [];
  }

  // ─── Navigation ──────────────────────────────────────────────────

  /** Jump to any node. Returns true if the node changed. */
  goToNode(node) {
    if (!node || node === this.currentNode) return false;
    this.currentNode = node;
    return true;
  }

  /** Go to root (empty board). */
  goToStart() {
    return this.goToNode(this.root);
  }

  /** Go to parent node. Returns false if already at root. */
  goBack() {
    if (!this.currentNode.parent) return false;
    this.currentNode = this.currentNode.parent;
    return true;
  }

  /** Go to first child (main line). Returns false if at leaf. */
  goForward() {
    if (this.currentNode.children.length === 0) return false;
    this.currentNode = this.currentNode.children[0];
    return true;
  }

  /** Follow main line all the way to the last move. */
  goToEnd() {
    let changed = false;
    while (this.currentNode.children.length > 0) {
      this.currentNode = this.currentNode.children[0];
      changed = true;
    }
    return changed;
  }

  /** Go to a specific variation child. */
  goToVariation(index) {
    if (index < 0 || index >= this.currentNode.children.length) return false;
    this.currentNode = this.currentNode.children[index];
    return true;
  }

  // ─── Editing ─────────────────────────────────────────────────────

  /**
   * Add a move at the current position.
   * - If a child with the same (x, y, color) exists, navigate to it instead.
   * - Otherwise, create a new child (variation if siblings exist).
   * @param {number} x
   * @param {number} y
   * @param {string} color  'BLACK' or 'WHITE'
   * @returns {MoveNode} The new or existing child node
   */
  addMove(x, y, color) {
    // Check if this move already exists as a child
    for (const child of this.currentNode.children) {
      if (child.move.x === x && child.move.y === y && child.move.color === color) {
        this.currentNode = child;
        return child;
      }
    }

    // Create new node
    const node = new MoveNode({ x, y, color }, this.currentNode);
    this.currentNode.children.push(node);
    this.currentNode = node;
    return node;
  }

  /**
   * Delete a node and all its descendants.
   * Cannot delete the root.
   * If deleting the current node, moves current to parent.
   * @param {MoveNode} node
   * @returns {boolean}
   */
  deleteNode(node) {
    if (node.isRoot || !node.parent) return false;

    const parent = node.parent;
    const idx = parent.children.indexOf(node);
    if (idx === -1) return false;

    // If current node is within the deleted subtree, move to parent
    let cur = this.currentNode;
    while (cur) {
      if (cur === node) {
        this.currentNode = parent;
        break;
      }
      cur = cur.parent;
    }

    parent.children.splice(idx, 1);
    return true;
  }

  /**
   * Promote a variation to main line (move it to children[0]).
   * @param {MoveNode} node
   * @returns {boolean}
   */
  promoteVariation(node) {
    if (!node.parent || !node.isVariation) return false;
    const parent = node.parent;
    const idx = parent.children.indexOf(node);
    if (idx <= 0) return false;

    parent.children.splice(idx, 1);
    parent.children.unshift(node);
    return true;
  }

  // ─── Board State ─────────────────────────────────────────────────

  /**
   * Get the path of moves from root to a given node.
   * @param {MoveNode} [node] — defaults to currentNode
   * @returns {MoveNode[]}
   */
  getPath(node) {
    node = node || this.currentNode;
    const path = [];
    let cur = node;
    while (cur && !cur.isRoot) {
      path.unshift(cur);
      cur = cur.parent;
    }
    return path;
  }

  /**
   * Build the board state at a given node.
   * @param {MoveNode} [node] — defaults to currentNode
   * @returns {{ board: number[][], lastMove: {x,y}|null }}
   */
  getBoardState(node) {
    node = node || this.currentNode;
    const size = this.boardSize;

    // Initialize empty board
    const board = [];
    for (let y = 0; y < size; y++) {
      board[y] = new Array(size).fill(0);
    }

    // Place walls
    for (const w of this.walls) {
      if (w.x >= 0 && w.x < size && w.y >= 0 && w.y < size) {
        board[w.y][w.x] = -1;
      }
    }

    // Place portals
    for (const p of this.portals) {
      if (p.a) board[p.a.y][p.a.x] = -2;
      if (p.b) board[p.b.y][p.b.x] = -2;
    }

    // Place moves along the path
    const path = this.getPath(node);
    let lastMove = null;
    for (const n of path) {
      const m = n.move;
      if (m && m.x >= 0 && m.x < size && m.y >= 0 && m.y < size) {
        board[m.y][m.x] = m.color === 'BLACK' ? 1 : 2;
        lastMove = { x: m.x, y: m.y };
      }
    }

    return { board, lastMove };
  }

  /**
   * Determine the next color to play at the current position.
   * @returns {'BLACK'|'WHITE'}
   */
  getNextColor() {
    if (this.currentNode.isRoot) return 'BLACK';
    return this.currentNode.move.color === 'BLACK' ? 'WHITE' : 'BLACK';
  }

  /**
   * Check if a cell is available for a new move at the current position.
   * @param {number} x
   * @param {number} y
   * @returns {boolean}
   */
  isCellAvailable(x, y) {
    const { board } = this.getBoardState();
    return board[y] && board[y][x] === 0;
  }

  // ─── Serialization ───────────────────────────────────────────────

  /**
   * Build a MoveTree from a flat move history array (from DB).
   * @param {Array<{x,y,color}>} moves
   * @param {object} opts — { boardSize, walls, portals }
   * @returns {MoveTree}
   */
  static fromMoveHistory(moves, opts = {}) {
    const tree = new MoveTree(opts);
    for (const m of moves) {
      tree.addMove(m.x, m.y, m.color);
    }
    // Reset to root after building
    tree.goToStart();
    return tree;
  }

  /**
   * Export tree to JSON (for future persistence).
   * @returns {object}
   */
  toJSON() {
    function serializeNode(node) {
      const obj = {
        move: node.move,
        comment: node.comment || undefined,
      };
      if (node.children.length > 0) {
        obj.children = node.children.map(serializeNode);
      }
      return obj;
    }
    return {
      boardSize: this.boardSize,
      walls: this.walls,
      portals: this.portals,
      tree: serializeNode(this.root),
    };
  }

  /**
   * Import tree from JSON.
   * @param {object} data
   * @returns {MoveTree}
   */
  static fromJSON(data) {
    const tree = new MoveTree({
      boardSize: data.boardSize,
      walls: data.walls || [],
      portals: data.portals || [],
    });

    function buildNode(obj, parent) {
      const node = new MoveNode(obj.move, parent);
      if (obj.comment) node.comment = obj.comment;
      if (obj.children) {
        for (const childObj of obj.children) {
          const child = buildNode(childObj, node);
          node.children.push(child);
        }
      }
      return node;
    }

    if (data.tree && data.tree.children) {
      for (const childObj of data.tree.children) {
        const child = buildNode(childObj, tree.root);
        tree.root.children.push(child);
      }
    }

    return tree;
  }
}
