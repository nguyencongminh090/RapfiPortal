'use strict';

/**
 * PortalGenerator.js — Generate portal pairs for a board.
 *
 * Constraints (from spec):
 *   1. Chebyshev distance between any 2 portal cells >= PORTAL_MIN_CHEBYSHEV (5)
 *   2. Portal cells cannot overlap or be adjacent to wall cells (Chebyshev dist >= 2)
 *   3. Generate PORTAL_PAIR_COUNT pairs (2 pairs = 4 cells)
 *   4. Retry up to PORTAL_RETRY_LIMIT times
 *
 * Returns: { portals: [{ a: {x,y}, b: {x,y} }, ...] }
 *   Each pair has entry A and exit B (bidirectional).
 *
 * Manual test checklist:
 *   [ ] 2 portal pairs (4 cells total) generated
 *   [ ] All portal cells have Chebyshev distance >= 5 from each other
 *   [ ] No portal cell overlaps or is adjacent to a wall cell
 *   [ ] Portal cells are within board bounds
 *   [ ] Returns null if generation fails after retry limit
 */

const config = require('../config');
const { chebyshev } = require('./WallGenerator');

/**
 * Generate portal pairs for a board.
 *
 * @param {number} boardSize
 * @param {Array<{x:number,y:number}>} walls — existing wall positions
 * @returns {{ portals: Array<{ a: {x:number,y:number}, b: {x:number,y:number} }> } | null}
 */
function generate(boardSize, walls = []) {
  const pairCount = config.PORTAL_PAIR_COUNT;
  const minDist   = config.PORTAL_MIN_CHEBYSHEV;
  const edgeMin   = config.PORTAL_EDGE_MIN_DIST;
  const totalCells = pairCount * 2; // 4 cells total

  // Valid placement range: [edgeMin, boardSize - 1 - edgeMin]
  const lo = edgeMin;
  const hi = boardSize - 1 - edgeMin;
  if (lo > hi) return null; // Board too small

  for (let attempt = 0; attempt < config.PORTAL_RETRY_LIMIT; attempt++) {
    const cells = [];
    let valid = true;

    for (let i = 0; i < totalCells; i++) {
      const x = lo + Math.floor(Math.random() * (hi - lo + 1));
      const y = lo + Math.floor(Math.random() * (hi - lo + 1));
      const pos = { x, y };

      // Check not on a wall cell (Chebyshev dist 0)
      // and not adjacent to a wall cell (Chebyshev dist < 2)
      let tooCloseToWall = false;
      for (const w of walls) {
        if (chebyshev(pos, w) < 2) {
          tooCloseToWall = true;
          break;
        }
      }
      if (tooCloseToWall) {
        valid = false;
        break;
      }

      // Check Chebyshev distance from all other portal cells
      for (const c of cells) {
        if (chebyshev(pos, c) < minDist) {
          valid = false;
          break;
        }
      }
      if (!valid) break;

      cells.push(pos);
    }

    if (valid && cells.length === totalCells) {
      // Form pairs: [0,1] and [2,3]
      const portals = [];
      for (let i = 0; i < totalCells; i += 2) {
        portals.push({ a: cells[i], b: cells[i + 1] });
      }
      return { portals };
    }
  }

  return null; // Failed after retry limit
}

module.exports = { generate };
