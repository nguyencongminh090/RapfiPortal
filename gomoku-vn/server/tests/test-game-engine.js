'use strict';

/**
 * Quick smoke test for GameEngine, WallGenerator, PortalGenerator.
 * Run: node server/tests/test-game-engine.js
 */

const { GameEngine }    = require('../managers/GameEngine');
const WallGenerator     = require('../generators/WallGenerator');
const PortalGenerator   = require('../generators/PortalGenerator');

let passed = 0;
let failed = 0;

function assert(cond, msg) {
  if (cond) { passed++; console.log(`  ✅ ${msg}`); }
  else { failed++; console.error(`  ❌ FAIL: ${msg}`); }
}

// --- WallGenerator ---
console.log('\n=== WallGenerator ===');
const wResult = WallGenerator.generate(17);
assert(wResult !== null, 'Generates walls for 17x17');
assert(wResult.walls.length === 3, '3 walls generated');
assert(wResult.firstMoveZones.length > 0, 'firstMoveZones non-empty');

// Check constraints
for (const w of wResult.walls) {
  assert(w.x >= 3 && w.x <= 13, `Wall x=${w.x} within edge distance`);
  assert(w.y >= 3 && w.y <= 13, `Wall y=${w.y} within edge distance`);
  const cx = 8, cy = 8;
  assert(!(Math.abs(w.x-cx) <= 1 && Math.abs(w.y-cy) <= 1), `Wall (${w.x},${w.y}) not in center 3x3`);
}
for (let i = 0; i < wResult.walls.length; i++) {
  for (let j = i+1; j < wResult.walls.length; j++) {
    const d = WallGenerator.chebyshev(wResult.walls[i], wResult.walls[j]);
    assert(d >= 4, `Walls ${i},${j} Chebyshev distance=${d} >= 4`);
  }
}

// --- PortalGenerator ---
console.log('\n=== PortalGenerator ===');
const pResult = PortalGenerator.generate(17, wResult.walls);
assert(pResult !== null, 'Generates portals for 17x17');
assert(pResult.portals.length === 2, '2 portal pairs generated');

// Check constraints
const allPortalCells = pResult.portals.flatMap(p => [p.a, p.b]);
for (let i = 0; i < allPortalCells.length; i++) {
  for (let j = i+1; j < allPortalCells.length; j++) {
    const d = WallGenerator.chebyshev(allPortalCells[i], allPortalCells[j]);
    assert(d >= 5, `Portals ${i},${j} Chebyshev distance=${d} >= 5`);
  }
  for (const w of wResult.walls) {
    const d = WallGenerator.chebyshev(allPortalCells[i], w);
    assert(d >= 2, `Portal ${i} not adjacent to wall (dist=${d})`);
  }
}

// --- GameEngine: basic move + win ---
console.log('\n=== GameEngine: basic ===');
const engine = new GameEngine({
  roomId: 'test',
  boardSize: 17,
  players: [
    { userId: 'p1', displayName: 'Black', color: 'BLACK' },
    { userId: 'p2', displayName: 'White', color: 'WHITE' },
  ],
  walls: [],
  portals: [],
  firstMoveZones: [],
});

assert(engine.board.length === 17, 'Board has 17 rows');
assert(engine.board[0].length === 17, 'Board has 17 columns');

// Wrong turn
let r = engine.makeMove('p2', 8, 8);
assert(r.error !== undefined, 'Rejects wrong turn');

// Valid move
r = engine.makeMove('p1', 8, 8);
assert(!r.error, 'Black plays center');
assert(r.nextTurn === 'p2', 'Turn switches to white');

// Occupied cell
r = engine.makeMove('p2', 8, 8);
assert(r.error !== undefined, 'Rejects occupied cell');

// --- GameEngine: horizontal win ---
console.log('\n=== GameEngine: win detection ===');
const eng2 = new GameEngine({
  roomId: 'test2',
  boardSize: 17,
  players: [
    { userId: 'p1', displayName: 'Black', color: 'BLACK' },
    { userId: 'p2', displayName: 'White', color: 'WHITE' },
  ],
  walls: [], portals: [], firstMoveZones: [],
});

// Black: 0,0  1,0  2,0  3,0  4,0
// White: 0,1  1,1  2,1  3,1
const moves = [
  ['p1', 0, 0], ['p2', 0, 1],
  ['p1', 1, 0], ['p2', 1, 1],
  ['p1', 2, 0], ['p2', 2, 1],
  ['p1', 3, 0], ['p2', 3, 1],
  ['p1', 4, 0], // Win!
];
let lastResult;
for (const [uid, x, y] of moves) {
  lastResult = eng2.makeMove(uid, x, y);
}
assert(lastResult.won === true, 'Black wins with 5 in a row (horizontal)');
assert(eng2.status === 'finished', 'Game status is finished');

// --- GameEngine: resign ---
console.log('\n=== GameEngine: resign ===');
const eng3 = new GameEngine({
  roomId: 'test3', boardSize: 17,
  players: [
    { userId: 'p1', displayName: 'B', color: 'BLACK' },
    { userId: 'p2', displayName: 'W', color: 'WHITE' },
  ],
  walls: [], portals: [], firstMoveZones: [],
});
eng3.makeMove('p1', 0, 0);
const rr = eng3.resign('p2');
assert(rr.winner === 'p1', 'p1 wins when p2 resigns');

// --- GameEngine: draw offer ---
console.log('\n=== GameEngine: draw offer ===');
const eng4 = new GameEngine({
  roomId: 'test4', boardSize: 17,
  players: [
    { userId: 'p1', displayName: 'B', color: 'BLACK' },
    { userId: 'p2', displayName: 'W', color: 'WHITE' },
  ],
  walls: [], portals: [], firstMoveZones: [],
});
eng4.makeMove('p1', 0, 0);
const dr = eng4.offerDraw('p2');
assert(dr.offered === true, 'Draw offer accepted');
const da = eng4.acceptDraw('p1');
assert(da.accepted === true, 'Draw accepted by p1');
assert(eng4.result.winner === 'draw', 'Result is draw');

// --- GameEngine: first move zone enforcement ---
console.log('\n=== GameEngine: first move zone ===');
const wallPositions = [{ x: 5, y: 5 }];
const eng5 = new GameEngine({
  roomId: 'test5', boardSize: 17,
  players: [
    { userId: 'p1', displayName: 'B', color: 'BLACK' },
    { userId: 'p2', displayName: 'W', color: 'WHITE' },
  ],
  walls: wallPositions,
  portals: [],
  firstMoveZones: [
    {x:4,y:4},{x:5,y:4},{x:6,y:4},
    {x:4,y:5},{x:6,y:5},
    {x:4,y:6},{x:5,y:6},{x:6,y:6},
  ],
});
r = eng5.makeMove('p1', 0, 0);
assert(r.error !== undefined, 'First move outside zone rejected');
r = eng5.makeMove('p1', 4, 4);
assert(!r.error, 'First move in zone accepted');

// --- Summary ---
console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
process.exit(failed > 0 ? 1 : 0);
