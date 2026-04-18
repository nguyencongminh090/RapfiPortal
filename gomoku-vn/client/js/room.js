'use strict';

/**
 * room.js — Room page controller.
 *
 * Responsibilities:
 *   - Render player slots with roles and ready status
 *   - Render settings panel (editable for host, read-only for others)
 *   - Handle sit/stand/ready/leave/kick actions
 *   - In-room chat with system messages
 *   - Auto-join room via sessionStorage or socket reconnect
 *
 * Manual test checklist:
 *   [ ] Room data populates from room:joined event
 *   [ ] Player slots render correct name/role/ready state
 *   [ ] Sit button places user in empty slot
 *   [ ] Stand button vacates slot
 *   [ ] Ready/cancel-ready toggle works
 *   [ ] Host sees editable settings; others see read-only info
 *   [ ] Settings change resets ready states
 *   [ ] Chat messages appear in real time (including system)
 *   [ ] Kick button visible only to host, works correctly
 *   [ ] Leave button navigates back to lobby
 */

// ---------------------------------------------------------------------------
// Auth guard
// ---------------------------------------------------------------------------
(function authGuard() {
  if (!localStorage.getItem('gvn_token')) {
    window.location.replace('login.html');
  }
})();

// ---------------------------------------------------------------------------
// Socket client
// ---------------------------------------------------------------------------
const client = new SocketClient();
const myUser = client.getUserInfo();

if (!myUser) {
  window.location.replace('login.html');
}

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
let roomData = null;   // Latest room:joined / room:updated payload
let myRole   = null;   // 'host' | 'player' | 'guest'
let mySlot   = null;   // 1 | 2 | null
let isReady  = false;

// ---------------------------------------------------------------------------
// Element refs
// ---------------------------------------------------------------------------
const statusBanner   = document.getElementById('status-banner');
const roomIdNav      = document.getElementById('room-id-nav');
const btnLeave       = document.getElementById('btn-leave');
const slot1Content   = document.getElementById('slot-1-content');
const slot2Content   = document.getElementById('slot-2-content');
const slot1Card      = document.getElementById('slot-1');
const slot2Card      = document.getElementById('slot-2');
const actionButtons  = document.getElementById('action-buttons');
const settingsPanel  = document.getElementById('settings-panel');
const settingsToggle = document.getElementById('settings-toggle');
const settingsArrow  = document.getElementById('settings-arrow');
const settingsBody   = document.getElementById('settings-body');
const usersPanel     = document.getElementById('users-panel');
const usersList      = document.getElementById('users-list');
const chatMessages   = document.getElementById('chat-messages');
const chatInput      = document.getElementById('chat-input');
const btnSend        = document.getElementById('btn-send');
const scorePanel     = document.getElementById('score-panel');
const scoreBody      = document.getElementById('score-body');

client.bindStatusBanner(statusBanner);

// ---------------------------------------------------------------------------
// Settings toggle
// ---------------------------------------------------------------------------
settingsToggle.addEventListener('click', () => {
  settingsBody.classList.toggle('open');
  settingsArrow.classList.toggle('settings-panel__toggle--open');
});

// ---------------------------------------------------------------------------
// Leave room
// ---------------------------------------------------------------------------
btnLeave.addEventListener('click', () => {
  client.emit('room:leave');
});

// ---------------------------------------------------------------------------
// Chat
// ---------------------------------------------------------------------------
function sendChat() {
  const text = chatInput.value.trim();
  if (!text) return;
  client.emit('chat:message', { text });
  chatInput.value = '';
}

btnSend.addEventListener('click', sendChat);
chatInput.addEventListener('keydown', (e) => {
  if (e.key === 'Enter') sendChat();
});

// ---------------------------------------------------------------------------
// Socket events
// ---------------------------------------------------------------------------

// Initial room data (on connect/reconnect)
client.on('room:joined', (data) => {
  roomData = data;
  updateUI();
});

// Room state updates
client.on('room:updated', (data) => {
  roomData = data;
  updateUI();
});

// Left room
client.on('room:left', () => {
  window.location.href = 'index.html';
});

// Kicked from room
client.on('room:kicked', (data) => {
  alert(data.message || 'Bạn đã bị mời ra khỏi phòng.');
  window.location.href = 'index.html';
});

// Room error
client.on('room:error', (data) => {
  if (!roomData) {
    // Not in a room yet — error during create/join, go back to lobby
    alert(data.message);
    window.location.href = 'index.html';
    return;
  }
  appendSystemMessage(`⚠ ${data.message}`);
});

// Chat messages
client.on('chat:message', (msg) => {
  appendChatMessage(msg);
});

client.on('chat:error', (data) => {
  appendSystemMessage(`⚠ ${data.message}`);
});

// ---------------------------------------------------------------------------
// Room entry: process intent from lobby OR reconnect
// ---------------------------------------------------------------------------
// The lobby stores an intent in sessionStorage (create or join).
// This page's socket will execute the intent once connected.
// This avoids the race where the lobby socket disconnects and destroys
// the room before this page's socket can take over.
// ---------------------------------------------------------------------------

let intentProcessed = false;

function processRoomIntent() {
  if (intentProcessed) return;
  intentProcessed = true;

  const raw = sessionStorage.getItem('gvn_room_intent');
  sessionStorage.removeItem('gvn_room_intent');

  if (raw) {
    try {
      const intent = JSON.parse(raw);
      if (intent.action === 'create') {
        client.emit('room:create', { settings: intent.settings || {} });
      } else if (intent.action === 'join') {
        client.emit('room:join', { roomId: intent.roomId });
      }
    } catch { /* ignore parse error */ }
  }
  // If no intent, SocketHandler's reconnect logic will auto-emit room:joined
  // if the user was already in a room.
}

// Process intent when socket connects (may fire immediately or after reconnect)
client.on('connect', () => {
  processRoomIntent();
});
// Also try immediately in case connect already fired before this listener
if (client.socket && client.socket.connected) {
  processRoomIntent();
}

// ---------------------------------------------------------------------------
// UI Update (main render function)
// ---------------------------------------------------------------------------

function updateUI() {
  if (!roomData) return;

  // Update nav
  roomIdNav.textContent = roomData.roomId;
  document.title = `GomokuVN — ${roomData.roomId}`;

  // Find my user in the room
  const me = roomData.users.find(u => u.userId === myUser.userId);
  myRole = me ? me.role : null;
  mySlot = me ? me.slot : null;
  isReady = me ? me.ready : false;

  // Render slots
  renderSlot(1, slot1Content, slot1Card);
  renderSlot(2, slot2Content, slot2Card);

  // Render action buttons
  renderActionButtons();

  // Render settings
  renderSettings();

  // Render guest users list
  renderUsersList();

  // Render score table
  renderScoreTable();
}

// ---------------------------------------------------------------------------
// Render player slot
// ---------------------------------------------------------------------------

function renderSlot(slotNum, contentEl, cardEl) {
  const player = roomData.users.find(u => u.slot === slotNum);
  cardEl.classList.toggle('slot-card--active', !!player);

  if (!player) {
    contentEl.innerHTML = `
      <div class="slot-card__empty">Trống</div>
    `;
    return;
  }

  const roleBadge = player.role === 'host'
    ? '<span class="slot-card__role slot-card__role--host">Chủ phòng</span>'
    : '';

  const guestBadge = player.isGuest
    ? '<span class="slot-card__role slot-card__role--guest-badge">Khách</span>'
    : '';

  const readyClass = player.ready ? '--ready' : '';

  contentEl.innerHTML = `
    <div class="slot-card__name">${escapeHtml(player.displayName)}</div>
    <div class="slot-card__status">
      <span class="ready-dot ready-dot${readyClass}"></span>
      <span class="ready-text ready-text${readyClass}">
        ${player.ready ? 'Sẵn sàng' : 'Chưa sẵn sàng'}
      </span>
      ${roleBadge}
      ${guestBadge}
    </div>
  `;
}

// ---------------------------------------------------------------------------
// Render action buttons
// ---------------------------------------------------------------------------

function renderActionButtons() {
  if (!myRole) {
    actionButtons.innerHTML = '';
    return;
  }

  let html = '';

  if (mySlot === null) {
    // User is a guest/spectator — show sit buttons
    const slot1Taken = roomData.users.some(u => u.slot === 1);
    const slot2Taken = roomData.users.some(u => u.slot === 2);

    if (!slot1Taken) {
      html += `<button class="btn-slot" onclick="sitDown(1)">Ngồi vào — Đen ●</button>`;
    }
    if (!slot2Taken) {
      html += `<button class="btn-slot" onclick="sitDown(2)">Ngồi vào — Trắng ○</button>`;
    }
  } else {
    // User is seated
    if (isReady) {
      html += `<button class="btn-slot btn-slot--cancel-ready" onclick="toggleReady()">Huỷ sẵn sàng</button>`;
    } else {
      html += `<button class="btn-slot btn-slot--ready" onclick="toggleReady()">Sẵn sàng</button>`;
    }
    if (roomData.state !== 'playing') {
      html += `<button class="btn-slot btn-slot--stand" onclick="standUp()">Đứng dậy</button>`;
    }
  }

  actionButtons.innerHTML = html;
}

// ---------------------------------------------------------------------------
// Render settings panel
// ---------------------------------------------------------------------------

function renderSettings() {
  const s = roomData.settings;

  if (myRole === 'host' && roomData.state !== 'playing') {
    // Host sees editable settings
    settingsBody.innerHTML = `
      <div class="setting-row">
        <span class="setting-label">Kích thước bàn</span>
        <div class="pill-group">
          <input type="radio" name="r-boardSize" id="r-bs-17" value="17" ${s.boardSize === 17 ? 'checked' : ''} onchange="updateSettings()" />
          <label for="r-bs-17">17×17</label>
          <input type="radio" name="r-boardSize" id="r-bs-19" value="19" ${s.boardSize === 19 ? 'checked' : ''} onchange="updateSettings()" />
          <label for="r-bs-19">19×19</label>
          <input type="radio" name="r-boardSize" id="r-bs-20" value="20" ${s.boardSize === 20 ? 'checked' : ''} onchange="updateSettings()" />
          <label for="r-bs-20">20×20</label>
        </div>
      </div>
      <div class="setting-row">
        <span class="setting-label">Luật đặc biệt</span>
        <div class="toggle-row">
          <span class="toggle-name">Tường (Wall)</span>
          <label class="toggle-switch">
            <input type="checkbox" id="r-wall" ${s.ruleWall ? 'checked' : ''} onchange="updateSettings()" />
            <span class="toggle-slider"></span>
          </label>
        </div>
        <div class="toggle-row">
          <span class="toggle-name">Cổng (Portal)</span>
          <label class="toggle-switch">
            <input type="checkbox" id="r-portal" ${s.rulePortal ? 'checked' : ''} onchange="updateSettings()" />
            <span class="toggle-slider"></span>
          </label>
        </div>
      </div>
      <div class="setting-row">
        <span class="setting-label">Bộ đếm giờ</span>
        <div class="pill-group">
          <input type="radio" name="r-timerMode" id="r-tm-move" value="per_move" ${s.timerMode === 'per_move' ? 'checked' : ''} onchange="updateSettings()" />
          <label for="r-tm-move">Mỗi nước</label>
          <input type="radio" name="r-timerMode" id="r-tm-game" value="per_game" ${s.timerMode === 'per_game' ? 'checked' : ''} onchange="updateSettings()" />
          <label for="r-tm-game">Mỗi ván</label>
        </div>
      </div>
      <div class="setting-row">
        <span class="setting-label">Thời gian</span>
        <div class="timer-input">
          <input type="number" id="r-timer" value="${s.timerSeconds}" min="10" max="3600" step="5" onchange="updateSettings()" />
          <span class="unit">giây</span>
        </div>
      </div>
    `;
    settingsBody.classList.add('open');
    settingsArrow.classList.add('settings-panel__toggle--open');
  } else {
    // Non-host or playing: read-only info
    const ruleNames = [];
    if (s.ruleWall) ruleNames.push('Tường');
    if (s.rulePortal) ruleNames.push('Cổng');
    const ruleText = ruleNames.length > 0 ? ruleNames.join(', ') : 'Cơ bản';
    const timerText = s.timerMode === 'per_move' ? 'Mỗi nước' : 'Mỗi ván';

    settingsBody.innerHTML = `
      <div class="settings-info">
        <div class="settings-info__row">
          <span class="settings-info__label">Bàn</span>
          <span class="settings-info__value">${s.boardSize}×${s.boardSize}</span>
        </div>
        <div class="settings-info__row">
          <span class="settings-info__label">Luật</span>
          <span class="settings-info__value">${ruleText}</span>
        </div>
        <div class="settings-info__row">
          <span class="settings-info__label">Thời gian</span>
          <span class="settings-info__value">${s.timerSeconds}s — ${timerText}</span>
        </div>
      </div>
    `;
    settingsBody.classList.add('open');
    settingsArrow.classList.add('settings-panel__toggle--open');
  }
}

// ---------------------------------------------------------------------------
// Render guest users list
// ---------------------------------------------------------------------------

function renderUsersList() {
  const guests = roomData.users.filter(u => u.slot === null);

  if (guests.length === 0) {
    usersPanel.style.display = 'none';
    return;
  }
  usersPanel.style.display = '';

  let html = '';
  for (const g of guests) {
    const kickBtn = (myRole === 'host' && g.userId !== myUser.userId && roomData.state !== 'playing')
      ? `<button class="btn-kick" onclick="kickUser('${escapeAttr(g.userId)}')">Mời ra</button>`
      : '';
    const hostBadge = g.role === 'host'
      ? ' <span class="slot-card__role slot-card__role--host">CP</span>'
      : '';
    html += `
      <li>
        <span class="user-name">${escapeHtml(g.displayName)}${hostBadge}</span>
        ${kickBtn}
      </li>
    `;
  }
  usersList.innerHTML = html;
}

// ---------------------------------------------------------------------------
// Render score table
// ---------------------------------------------------------------------------

function renderScoreTable() {
  const st = roomData.scoreTable;
  if (!st || Object.keys(st).length === 0) {
    scorePanel.style.display = 'none';
    return;
  }
  scorePanel.style.display = '';

  let html = '';
  for (const [, entry] of Object.entries(st)) {
    html += `
      <tr>
        <td>${escapeHtml(entry.name || '—')}</td>
        <td>${entry.win || 0}</td>
        <td>${entry.loss || 0}</td>
        <td>${entry.draw || 0}</td>
      </tr>
    `;
  }
  scoreBody.innerHTML = html;
}

// ---------------------------------------------------------------------------
// Chat rendering
// ---------------------------------------------------------------------------

function appendChatMessage(msg) {
  const div = document.createElement('div');

  if (msg.isSystem) {
    div.className = 'chat-msg chat-msg--system';
    div.textContent = msg.text;
  } else {
    div.className = 'chat-msg';
    const nameSpan = document.createElement('span');
    nameSpan.className = 'chat-msg__name';
    nameSpan.textContent = msg.from + ':';
    div.appendChild(nameSpan);
    div.appendChild(document.createTextNode(' ' + msg.text));
  }

  chatMessages.appendChild(div);
  // Auto-scroll to bottom
  chatMessages.scrollTop = chatMessages.scrollHeight;
}

function appendSystemMessage(text) {
  appendChatMessage({ from: null, text, isSystem: true, timestamp: Date.now() });
}

// ---------------------------------------------------------------------------
// Action handlers (exposed globally for onclick)
// ---------------------------------------------------------------------------

window.sitDown = function(slot) {
  client.emit('room:sit', { slot });
};

window.standUp = function() {
  client.emit('room:stand');
};

window.toggleReady = function() {
  client.emit('room:ready');
};

window.kickUser = function(userId) {
  client.emit('room:kick', { userId });
};

// Debounced settings update to avoid flooding
let settingsTimeout = null;
window.updateSettings = function() {
  if (settingsTimeout) clearTimeout(settingsTimeout);
  settingsTimeout = setTimeout(() => {
    const boardSizeEl = document.querySelector('input[name="r-boardSize"]:checked');
    const timerModeEl = document.querySelector('input[name="r-timerMode"]:checked');
    const timerEl     = document.getElementById('r-timer');
    const wallEl      = document.getElementById('r-wall');
    const portalEl    = document.getElementById('r-portal');

    if (!boardSizeEl || !timerModeEl) return;

    client.emit('room:settings', {
      settings: {
        boardSize: parseInt(boardSizeEl.value, 10),
        ruleWall: wallEl ? wallEl.checked : false,
        rulePortal: portalEl ? portalEl.checked : false,
        timerMode: timerModeEl.value,
        timerSeconds: timerEl ? parseInt(timerEl.value, 10) : 60,
      },
    });
  }, 300);
};

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

function escapeAttr(str) {
  return String(str).replace(/'/g, "\\'").replace(/"/g, '\\"');
}
