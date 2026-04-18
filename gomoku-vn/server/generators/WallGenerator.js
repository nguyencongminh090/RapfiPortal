'use strict';

/**
 * WallGenerator.js — Generate WALL cells for a board.
 *
 * Constraints (from spec):
 *   1. Distance from any board edge >= WALL_EDGE_MIN_DIST (3)
 *   2. Must NOT fall in center 3×3 zone
 *   3. Chebyshev distance between any 2 wall cells > 3 (i.e. >= WALL_MIN_CHEBYSHEV)
 *   4. Retry up to WALL_RETRY_LIMIT times if constraints fail
 *
 * Returns: { walls: [{x,y}], firstMoveZones: [{x,y}] }
 *   - firstMoveZones: the 8 cells surrounding each wall (for first-move rule)
 *
 * Manual test checklist:
 *   [ ] 3 walls generated for each valid board size
 *   [ ] All walls respect edge distance
 *   [ ] No wall in center 3×3 zone
 *   [ ] Chebyshev distance between pairs > 3
 *   [ ] firstMoveZones contains correct surrounding cells
 *   [ ] Returns null if generation fails after retry limit
 */

const config = require('../config');

/**
 * Chebyshev distance between two points.
 * @param {{x:number,y:number}} a
 * @param {{x:number,y:number}} b
 * @returns {number}
 */
function chebyshev(a, b) {
  return Math.max(Math.abs(a.x - b.x), Math.abs(a.y - b.y));
}

/**
 * Generate WALL cells for a board of given size.
 *
 * @param {number} boardSize — board dimension (e.g. 17, 19, 20)
 * @returns {{ walls: Array<{x:number,y:number}>, firstMoveZones: Array<{x:number,y:number}> } | null}
 *   null if generation fails after WALL_RETRY_LIMIT attempts.
 */
function generate(boardSize) {
  const edgeMin = config.WALL_EDGE_MIN_DIST;
  const centerZone = config.WALL_CENTER_ZONE;
  const minDist = config.WALL_MIN_CHEBYSHEV;
  const count = config.WALL_COUNT;

  // Valid placement range: [edgeMin, boardSize - 1 - edgeMin]
  const lo = edgeMin;
  const hi = boardSize - 1 - edgeMin;
  if (lo > hi) return null; // Board too small

  // Center of the board
  const cx = Math.floor((boardSize - 1) / 2);
  const cy = Math.floor((boardSize - 1) / 2);

  for (let attempt = 0; attempt < config.WALL_RETRY_LIMIT; attempt++) {
    const walls = [];
    let valid = true;

    for (let i = 0; i < count; i++) {
      // Random position in valid range
      const x = lo + Math.floor(Math.random() * (hi - lo + 1));
      const y = lo + Math.floor(Math.random() * (hi - lo + 1));

      // Check center 3×3 zone: |x - cx| <= centerZone AND |y - cy| <= centerZone
      if (Math.abs(x - cx) <= centerZone && Math.abs(y - cy) <= centerZone) {
        valid = false;
        break;
      }

      // Check Chebyshev distance from all previously placed walls
      for (const w of walls) {
        if (chebyshev({ x, y }, w) < minDist) {
          valid = false;
          break;
        }
      }
      if (!valid) break;

      walls.push({ x, y });
    }

    if (valid && walls.length === count) {
      // Compute firstMoveZones: 8 surrounding cells of each wall
      const zoneSet = new Set();
      const dirs = [-1, 0, 1];

      for (const w of walls) {
        for (const dx of dirs) {
          for (const dy of dirs) {
            if (dx === 0 && dy === 0) continue; // Skip the wall cell itself
            const nx = w.x + dx;
            const ny = w.y + dy;
            // Must be within board bounds
            if (nx >= 0 && nx < boardSize && ny >= 0 && ny < boardSize) {
              zoneSet.add(`${nx},${ny}`);
            }
          }
        }
      }

      // Remove any zone cell that is itself a wall
      for (const w of walls) {
        zoneSet.delete(`${w.x},${w.y}`);
      }

      const firstMoveZones = [];
      for (const key of zoneSet) {
        const [x, y] = key.split(',').map(Number);
        firstMoveZones.push({ x, y });
      }

      return { walls, firstMoveZones };
    }
  }

  return null; // Failed after retry limit
}

module.exports = { generate, chebyshev };
