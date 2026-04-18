'use strict';

/**
 * Integration test: Two-player game flow via Socket.io.
 *
 * Run: node server/tests/test-game-flow.js
 *
 * Simulates:
 *   1. Two guest logins
 *   2. Player 1 creates a room (with Wall enabled)
 *   3. Player 2 joins the room
 *   4. Both sit and ready up
 *   5. Game starts → verify game:init event
 *   6. Players make moves → verify game:moved events
 *   7. Player 1 resigns → verify game:ended
 */

const { io: ioClient } = require('socket.io-client');
const http = require('http');

const SERVER = 'http://localhost:3000';

let passed = 0;
let failed = 0;

function assert(cond, msg) {
  if (cond) { passed++; console.log(`  ✅ ${msg}`); }
  else { failed++; console.error(`  ❌ FAIL: ${msg}`); }
}

function loginAsGuest() {
  return new Promise((resolve, reject) => {
    const req = http.request(`${SERVER}/api/auth/guest`, { method: 'POST' }, (res) => {
      let body = '';
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        try {
          const data = JSON.parse(body);
          resolve(data.token);
        } catch (e) { reject(e); }
      });
    });
    req.on('error', reject);
    req.end();
  });
}

function createSocket(token) {
  return ioClient(SERVER, {
    auth: { token },
    transports: ['websocket'],
    reconnection: false,
  });
}

function waitFor(socket, event, timeout = 5000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`Timeout waiting for ${event}`)), timeout);
    socket.once(event, (data) => {
      clearTimeout(timer);
      resolve(data);
    });
  });
}

async function run() {
  console.log('\n=== Integration Test: Two-Player Game Flow ===\n');

  // 1. Login as two guests
  const token1 = await loginAsGuest();
  const token2 = await loginAsGuest();
  assert(!!token1, 'Guest 1 logged in');
  assert(!!token2, 'Guest 2 logged in');

  // 2. Connect sockets
  const s1 = createSocket(token1);
  const s2 = createSocket(token2);

  await Promise.all([waitFor(s1, 'connect'), waitFor(s2, 'connect')]);
  assert(true, 'Both sockets connected');

  // 3. Player 1 creates room with Wall
  s1.emit('room:create', {
    settings: { boardSize: 17, ruleWall: true, rulePortal: false, timerMode: 'per_move', timerSeconds: 60 }
  });
  const roomData = await waitFor(s1, 'room:joined');
  const roomId = roomData.roomId;
  assert(!!roomId, `Room created: ${roomId}`);
  assert(roomData.settings.ruleWall === true, 'Wall rule enabled');

  // 4. Player 2 joins room
  s2.emit('room:join', { roomId });
  const joinData = await waitFor(s2, 'room:joined');
  assert(joinData.roomId === roomId, 'Player 2 joined same room');

  // 5. Both sit
  s1.emit('room:sit', { slot: 1 });
  await waitFor(s1, 'room:updated');
  s2.emit('room:sit', { slot: 2 });
  await waitFor(s2, 'room:updated');
  assert(true, 'Both players seated');

  // 6. Both ready
  s1.emit('room:ready');
  await waitFor(s1, 'room:updated');
  s2.emit('room:ready');

  // 7. Wait for game:init
  const gameInit = await waitFor(s1, 'game:init');
  assert(gameInit.boardSize === 17, 'Game board size correct');
  assert(gameInit.walls.length === 3, '3 walls generated');
  assert(gameInit.firstMoveZones.length > 0, 'First move zones present');
  assert(!!gameInit.currentTurn, 'Current turn set');
  assert(!!gameInit.timer, 'Timer initialized');
  assert(gameInit.timer.black === 60, 'Black timer = 60s');

  // 8. First move (must be in firstMoveZone due to WALL rule)
  const fmz = gameInit.firstMoveZones[0];
  s1.emit('game:move', { x: fmz.x, y: fmz.y });
  const moved1 = await waitFor(s1, 'game:moved');
  assert(moved1.color === 'BLACK', 'First move is BLACK');
  assert(moved1.nextTurn !== null, 'Next turn set');

  // 9. Second move (White) — find a safe empty cell
  // Avoid walls, portals, firstMoveZones, and the first move
  const usedSet = new Set();
  usedSet.add(`${fmz.x},${fmz.y}`);
  for (const w of gameInit.walls) usedSet.add(`${w.x},${w.y}`);
  for (const p of gameInit.portals) {
    usedSet.add(`${p.a.x},${p.a.y}`);
    usedSet.add(`${p.b.x},${p.b.y}`);
  }
  let safeX = -1, safeY = -1;
  for (let y = 0; y < 17 && safeX < 0; y++) {
    for (let x = 0; x < 17 && safeX < 0; x++) {
      if (!usedSet.has(`${x},${y}`)) { safeX = x; safeY = y; }
    }
  }
  // s2 also receives s1's move broadcast — drain it first
  await waitFor(s2, 'game:moved');

  s2.emit('game:move', { x: safeX, y: safeY });

  // Listen for either game:moved or game:error
  const moved2Result = await Promise.race([
    waitFor(s2, 'game:moved').then(d => ({ type: 'moved', data: d })),
    waitFor(s2, 'game:error').then(d => ({ type: 'error', data: d })),
  ]);

  if (moved2Result.type === 'error') {
    console.error('  ⚠ game:error on move 2:', moved2Result.data.message);
    console.error(`    Tried position (${safeX}, ${safeY})`);
    assert(false, 'Second move is WHITE (got error instead)');
  } else {
    const moved2 = moved2Result.data;
    assert(moved2.color === 'WHITE', 'Second move is WHITE');
  }

  // 10. Player 1 resigns
  const endedPromise = waitFor(s1, 'game:ended');
  s1.emit('game:resign');
  const ended = await endedPromise;
  assert(ended.result.reason === 'resign', 'Game ended by resign');
  assert(!!ended.scoreTable, 'Score table present');

  // Verify scores
  const p1Id = gameInit.players[0].userId;
  const p2Id = gameInit.players[1].userId;
  assert(ended.scoreTable[p1Id].loss === 1, 'P1 has 1 loss');
  assert(ended.scoreTable[p2Id].win === 1, 'P2 has 1 win');

  // Cleanup
  s1.disconnect();
  s2.disconnect();

  console.log(`\n=== Results: ${passed} passed, ${failed} failed ===`);
  process.exit(failed > 0 ? 1 : 0);
}

run().catch((err) => {
  console.error('Test error:', err);
  process.exit(1);
});
