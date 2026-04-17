'use strict';

/**
 * lobby.js — Lobby UI controller.
 *
 * Responsibilities:
 *   - Render live room list from server
 *   - Create room modal (collect settings → emit room:create)
 *   - Join room (emit room:join → redirect to room page)
 *   - Display user info + logout
 *
 * Manual test checklist:
 *   [ ] No token → redirect to login
 *   [ ] Room list renders on lobby:update
 *   [ ] Empty state shows "Chưa có phòng nào"
 *   [ ] Create room modal opens/closes
 *   [ ] room:create emits correct settings payload
 *   [ ] room:joined → stores roomId → redirects to room.html
 *   [ ] room:error shows alert
 *   [ ] Logout clears token and redirects
 */

// ---------------------------------------------------------------------------
// Auth guard: redirect if no token
// ---------------------------------------------------------------------------
(function authGuard() {
  const token = localStorage.getItem('gvn_token');
  if (!token) {
    window.location.replace('login.html');
    return;
  }
})();

// ---------------------------------------------------------------------------
// Initialize Socket.io client
// ---------------------------------------------------------------------------
const client = new SocketClient();

// ---------------------------------------------------------------------------
// Element refs
// ---------------------------------------------------------------------------
const statusBanner  = document.getElementById('status-banner');
const navUser       = document.getElementById('nav-user');
const navBadge      = document.getElementById('nav-badge');
const btnLogout     = document.getElementById('btn-logout');
const roomCount     = document.getElementById('room-count');
const roomListEl    = document.getElementById('room-list');
const btnCreate     = document.getElementById('btn-create');
const modalOverlay  = document.getElementById('modal-create');
const modalClose    = document.getElementById('modal-close');
const modalCancel   = document.getElementById('modal-cancel');
const modalConfirm  = document.getElementById('modal-confirm');

// ---------------------------------------------------------------------------
// Display user info in nav
// ---------------------------------------------------------------------------
const userInfo = client.getUserInfo();
if (userInfo) {
  navUser.textContent = userInfo.displayName;
  navBadge.textContent = userInfo.isGuest ? 'Khách' : '';
  navBadge.style.display = userInfo.isGuest ? '' : 'none';
}

// Bind status banner
client.bindStatusBanner(statusBanner);

// ---------------------------------------------------------------------------
// Logout
// ---------------------------------------------------------------------------
btnLogout.addEventListener('click', () => {
  client.logout();
});

// ---------------------------------------------------------------------------
// Subscribe to lobby updates
// ---------------------------------------------------------------------------
client.emit('lobby:subscribe');

client.on('lobby:update', (data) => {
  renderRoomList(data.rooms || []);
});

client.on('room:error', (data) => {
  alert(data.message);  // Simple alert for now; can upgrade to banner later
});

// ---------------------------------------------------------------------------
// When successfully joined a room → redirect to room page
// ---------------------------------------------------------------------------
client.on('room:joined', (data) => {
  // Store room data for room.html to pick up
  sessionStorage.setItem('gvn_room', JSON.stringify(data));
  window.location.href = 'room.html';
});

// ---------------------------------------------------------------------------
// Room List Rendering
// ---------------------------------------------------------------------------

function renderRoomList(rooms) {
  roomCount.textContent = rooms.length > 0 ? `(${rooms.length})` : '';

  if (rooms.length === 0) {
    roomListEl.innerHTML = `
      <div class="room-list__empty">
        <div class="room-list__empty-icon">🎮</div>
        Chưa có phòng nào. Hãy tạo phòng mới!
      </div>
    `;
    return;
  }

  let html = `
    <table class="room-table">
      <thead>
        <tr>
          <th>Phòng</th>
          <th>Chủ phòng</th>
          <th>Người chơi</th>
          <th>Trạng thái</th>
          <th>Luật</th>
          <th></th>
        </tr>
      </thead>
      <tbody>
  `;

  for (const room of rooms) {
    const stateLabel = getStateLabel(room.state, room.playerCount);
    const stateClass = getStateClass(room.state, room.playerCount);
    const ruleTags   = buildRuleTags(room);

    html += `
      <tr data-room-id="${room.roomId}">
        <td><span class="room-id">${escapeHtml(room.roomId)}</span></td>
        <td>${escapeHtml(room.hostName)}</td>
        <td>
          <span class="player-count ${room.playerCount >= 2 ? 'player-count--full' : 'player-count--open'}">
            ${room.playerCount}/2
          </span>
          <span style="color:var(--c-ink-3);font-size:11px;margin-left:4px">(${room.userCount})</span>
        </td>
        <td><span class="state-badge ${stateClass}">${stateLabel}</span></td>
        <td>${ruleTags}</td>
        <td>
          <button class="btn-join" onclick="joinRoom('${escapeAttr(room.roomId)}')" type="button">
            Tham gia
          </button>
        </td>
      </tr>
    `;
  }

  html += '</tbody></table>';
  roomListEl.innerHTML = html;
}

function getStateLabel(state, playerCount) {
  if (state === 'playing') return 'Đang chơi';
  if (state === 'interrupted') return 'Gián đoạn';
  if (playerCount >= 2) return 'Chờ sẵn sàng';
  return 'Đang chờ';
}

function getStateClass(state, playerCount) {
  if (state === 'playing') return 'state-badge--playing';
  if (playerCount >= 2) return 'state-badge--waiting';
  return 'state-badge--idle';
}

function buildRuleTags(room) {
  let tags = '';
  tags += `<span style="color:var(--c-ink-3);font-size:12px">${room.boardSize}×${room.boardSize}</span> `;
  if (room.ruleWall) tags += '<span class="rule-tag rule-tag--wall">Wall</span>';
  if (room.rulePortal) tags += '<span class="rule-tag rule-tag--portal">Portal</span>';
  if (!room.ruleWall && !room.rulePortal) tags += '<span style="color:var(--c-ink-3);font-size:11px">Cơ bản</span>';
  return `<div class="rule-tags">${tags}</div>`;
}

// ---------------------------------------------------------------------------
// Join Room
// ---------------------------------------------------------------------------
// Exposed globally for onclick
window.joinRoom = function(roomId) {
  client.emit('room:join', { roomId });
};

// ---------------------------------------------------------------------------
// Create Room Modal
// ---------------------------------------------------------------------------

function openModal() {
  modalOverlay.classList.add('visible');
}

function closeModal() {
  modalOverlay.classList.remove('visible');
}

btnCreate.addEventListener('click', openModal);
modalClose.addEventListener('click', closeModal);
modalCancel.addEventListener('click', closeModal);

// Close modal on overlay click (but not on modal body click)
modalOverlay.addEventListener('click', (e) => {
  if (e.target === modalOverlay) closeModal();
});

// Close modal on Escape key
document.addEventListener('keydown', (e) => {
  if (e.key === 'Escape' && modalOverlay.classList.contains('visible')) {
    closeModal();
  }
});

// Confirm → create room
modalConfirm.addEventListener('click', () => {
  const boardSize = parseInt(
    document.querySelector('input[name="boardSize"]:checked').value, 10
  );
  const timerMode = document.querySelector('input[name="timerMode"]:checked').value;
  const timerSeconds = parseInt(document.getElementById('timer-seconds').value, 10) || 60;
  const ruleWall   = document.getElementById('rule-wall').checked;
  const rulePortal = document.getElementById('rule-portal').checked;

  client.emit('room:create', {
    settings: { boardSize, ruleWall, rulePortal, timerMode, timerSeconds },
  });

  closeModal();
});

// ---------------------------------------------------------------------------
// Utility
// ---------------------------------------------------------------------------

function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str;
  return div.innerHTML;
}

function escapeAttr(str) {
  return str.replace(/'/g, "\\'").replace(/"/g, '\\"');
}
