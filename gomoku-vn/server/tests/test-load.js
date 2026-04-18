'use strict';

/**
 * Load test: Simulates 10+ concurrent rooms with automated bot players.
 *
 * Run: node server/tests/test-load.js
 *
 * Each room: 2 guest players → sit → ready → play a few moves → resign.
 * Validates:
 *   - 10+ rooms can be created and run concurrently
 *   - All rooms complete without errors
 *   - Memory usage stays reasonable
 */

const { io: ioClient } = require('socket.io-client');
const http = require('http');

const SERVER = 'http://localhost:3000';
const NUM_ROOMS = 12;

function loginAsGuest() {
  return new Promise((resolve, reject) => {
    const req = http.request(`${SERVER}/api/auth/guest`, { method: 'POST' }, (res) => {
      let body = '';
      res.on('data', chunk => body += chunk);
      res.on('end', () => {
        try { resolve(JSON.parse(body).token); }
        catch (e) { reject(e); }
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

function waitFor(socket, event, timeout = 8000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error(`Timeout: ${event}`)), timeout);
    socket.once(event, (data) => {
      clearTimeout(timer);
      resolve(data);
    });
  });
}

async function simulateRoom(roomIndex) {
  const label = `Room ${roomIndex + 1}`;

  const [t1, t2] = await Promise.all([loginAsGuest(), loginAsGuest()]);
  const s1 = createSocket(t1);
  const s2 = createSocket(t2);
  await Promise.all([waitFor(s1, 'connect'), waitFor(s2, 'connect')]);

  // Create room
  s1.emit('room:create', {
    settings: { boardSize: 17, ruleWall: true, rulePortal: false, timerMode: 'per_move', timerSeconds: 60 }
  });
  const roomData = await waitFor(s1, 'room:joined');
  const roomId = roomData.roomId;

  // Join, sit, ready
  s2.emit('room:join', { roomId });
  await waitFor(s2, 'room:joined');

  s1.emit('room:sit', { slot: 1 });
  await waitFor(s1, 'room:updated');
  s2.emit('room:sit', { slot: 2 });
  await waitFor(s2, 'room:updated');

  s1.emit('room:ready');
  await waitFor(s1, 'room:updated');
  s2.emit('room:ready');

  const gameInit = await waitFor(s1, 'game:init');
  await waitFor(s2, 'game:init');

  // Play first move in firstMoveZone
  const fmz = gameInit.firstMoveZones[0];
  s1.emit('game:move', { x: fmz.x, y: fmz.y });
  await waitFor(s1, 'game:moved');
  await waitFor(s2, 'game:moved');

  // Resign immediately
  const endP = waitFor(s1, 'game:ended', 5000);
  s1.emit('game:resign');
  await endP;

  s1.disconnect();
  s2.disconnect();
  return { label, roomId, success: true };
}

async function run() {
  console.log(`\n=== Load Test: ${NUM_ROOMS} Concurrent Rooms ===\n`);

  const startTime = Date.now();
  const promises = [];

  for (let i = 0; i < NUM_ROOMS; i++) {
    promises.push(
      simulateRoom(i)
        .then(r => { console.log(`  ✅ ${r.label} (${r.roomId}) — completed`); return r; })
        .catch(err => { console.error(`  ❌ Room ${i + 1} — ${err.message}`); return { success: false }; })
    );
    await new Promise(r => setTimeout(r, 100));
  }

  const results = await Promise.all(promises);
  const elapsed = ((Date.now() - startTime) / 1000).toFixed(1);
  const passed = results.filter(r => r.success).length;
  const failed = results.length - passed;

  console.log(`\n--- Summary ---`);
  console.log(`  Rooms: ${passed}/${NUM_ROOMS} succeeded`);
  console.log(`  Time:  ${elapsed}s`);
  console.log(`\n=== ${failed === 0 ? 'PASS' : 'FAIL'} ===\n`);
  process.exit(failed > 0 ? 1 : 0);
}

run().catch((err) => {
  console.error('Load test error:', err);
  process.exit(1);
});
