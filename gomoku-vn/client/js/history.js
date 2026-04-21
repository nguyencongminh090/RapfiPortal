/**
 * history.js — Game History & Replay Viewer for GomokuVN.
 *
 * - Fetches game list from /api/games
 * - Renders a paginated table
 * - Loads a game for replay with step-through controls
 * - Reuses BoardRenderer from board.js
 */

'use strict';

// ---------------------------------------------------------------------------
// Element refs
// ---------------------------------------------------------------------------
const viewList    = document.getElementById('view-list');
const viewReplay  = document.getElementById('view-replay');
const gameListEl  = document.getElementById('game-list');
const gameTotalEl = document.getElementById('game-total');
const paginationEl = document.getElementById('pagination');

// Replay elements
const replayBlack   = document.getElementById('replay-black');
const replayWhite   = document.getElementById('replay-white');
const replayResult  = document.getElementById('replay-result');
const replayMeta    = document.getElementById('replay-meta');
const replayCanvas  = document.getElementById('replay-canvas');
const moveCounter   = document.getElementById('move-counter');
const btnFirst      = document.getElementById('btn-first');
const btnPrev       = document.getElementById('btn-prev');
const btnNext       = document.getElementById('btn-next');
const btnLast       = document.getElementById('btn-last');
const btnPlay       = document.getElementById('btn-play');
const btnBack       = document.getElementById('replay-back');

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
let currentPage = 1;
let boardRenderer = null;
let replayData = null;     // { game, moves, walls, portals, boardSize }
let replayIndex = 0;       // Current move index (0 = empty board)
let autoPlayTimer = null;

// ---------------------------------------------------------------------------
// Load game list
// ---------------------------------------------------------------------------
async function loadGames(page = 1) {
  currentPage = page;
  try {
    const res = await fetch(`/api/games?page=${page}&limit=15`);
    const data = await res.json();

    if (!res.ok) {
      gameListEl.innerHTML = `<div class="game-list__empty">Lỗi: ${data.error || 'Không thể tải.'}</div>`;
      return;
    }

    const { games, pagination } = data;
    gameTotalEl.textContent = `(${pagination.total} ván)`;

    if (games.length === 0) {
      gameListEl.innerHTML = '<div class="game-list__empty">Chưa có ván đấu nào được ghi nhận.</div>';
      paginationEl.innerHTML = '';
      return;
    }

    renderGameTable(games);
    renderPagination(pagination);
  } catch {
    gameListEl.innerHTML = '<div class="game-list__empty">Không thể kết nối server.</div>';
  }
}

function renderGameTable(games) {
  let html = `
    <table class="game-table">
      <thead>
        <tr>
          <th>Thời gian</th>
          <th>Đen (X)</th>
          <th>Trắng (O)</th>
          <th>Kết quả</th>
          <th></th>
        </tr>
      </thead>
      <tbody>
  `;

  for (const g of games) {
    const time = formatTime(g.ended_at || g.started_at);
    const resultText = getResultText(g);
    const resultClass = g.winner === 'draw' ? 'result-draw' : 'result-win';

    html += `
      <tr>
        <td style="font-size:12px; color:var(--c-ink-3);">${time}</td>
        <td><strong>${escapeHtml(g.black_player_name)}</strong></td>
        <td>${escapeHtml(g.white_player_name)}</td>
        <td><span class="${resultClass}">${resultText}</span></td>
        <td><button class="btn-replay" onclick="openReplay('${g.id}')" type="button">Xem lại</button></td>
      </tr>
    `;
  }

  html += '</tbody></table>';
  gameListEl.innerHTML = html;
}

function renderPagination(p) {
  if (p.totalPages <= 1) {
    paginationEl.innerHTML = '';
    return;
  }

  let html = '';
  html += `<button ${p.page <= 1 ? 'disabled' : ''} onclick="loadGames(${p.page - 1})">‹</button>`;

  for (let i = 1; i <= p.totalPages; i++) {
    if (p.totalPages > 7 && Math.abs(i - p.page) > 2 && i !== 1 && i !== p.totalPages) {
      if (i === 2 || i === p.totalPages - 1) html += '<button disabled>…</button>';
      continue;
    }
    html += `<button class="${i === p.page ? 'active' : ''}" onclick="loadGames(${i})">${i}</button>`;
  }

  html += `<button ${p.page >= p.totalPages ? 'disabled' : ''} onclick="loadGames(${p.page + 1})">›</button>`;
  paginationEl.innerHTML = html;
}

// ---------------------------------------------------------------------------
// Replay viewer
// ---------------------------------------------------------------------------
async function openReplay(gameId) {
  try {
    const res = await fetch(`/api/games/${gameId}`);
    const data = await res.json();

    if (!res.ok || !data.game) {
      alert(data.error || 'Không thể tải ván đấu.');
      return;
    }

    const game = data.game;
    replayData = {
      game,
      moves: game.moves || [],
      walls: game.walls || [],
      portals: game.portals || [],
      boardSize: game.board_size,
    };
    replayIndex = 0;

    // Fill info
    replayBlack.textContent = `✕ ${game.black_player_name}`;
    replayWhite.textContent = `○ ${game.white_player_name}`;
    replayResult.textContent = getResultTextFull(game);

    const rules = [];
    if (game.rule_wall) rules.push('Wall');
    if (game.rule_portal) rules.push('Portal');
    const ruleStr = rules.length > 0 ? rules.join(' + ') : 'Cơ bản';
    replayMeta.textContent = `${game.board_size}×${game.board_size} | ${ruleStr} | ${formatTime(game.ended_at)}`;

    // Switch view FIRST (so parent has dimensions when we call resize)
    viewList.style.display = 'none';
    viewReplay.style.display = '';

    // Update URL for sharing (without page reload)
    history.replaceState(null, '', `history.html?id=${gameId}`);

    // Init board renderer (once)
    if (!boardRenderer) {
      boardRenderer = new BoardRenderer(replayCanvas, {
        boardSize: replayData.boardSize,
        onCellClick: null,
      });
    }
    boardRenderer.boardSize = replayData.boardSize;
    boardRenderer.interactive = false;

    // Let the DOM settle, then resize and render
    requestAnimationFrame(() => {
      boardRenderer.resize();
      renderReplayBoard();
    });
  } catch (err) {
    alert('Lỗi khi tải ván đấu.');
  }
}

function closeReplay() {
  stopAutoPlay();
  viewReplay.style.display = 'none';
  viewList.style.display = '';
  replayData = null;
  // Clear URL param
  history.replaceState(null, '', 'history.html');
}

function renderReplayBoard() {
  if (!replayData || !boardRenderer) return;

  const { boardSize, walls, portals, moves } = replayData;

  // Build board state up to replayIndex
  const board = [];
  for (let y = 0; y < boardSize; y++) {
    board[y] = [];
    for (let x = 0; x < boardSize; x++) {
      board[y][x] = 0;
    }
  }

  // Place walls
  for (const w of walls) {
    if (w.x >= 0 && w.x < boardSize && w.y >= 0 && w.y < boardSize) {
      board[w.y][w.x] = -1;
    }
  }

  // Place portal cells
  for (const p of portals) {
    if (p.a) board[p.a.y][p.a.x] = -2;
    if (p.b) board[p.b.y][p.b.x] = -2;
  }

  // Place moves up to current index
  let lastMove = null;
  for (let i = 0; i < replayIndex; i++) {
    const m = moves[i];
    if (m && m.x >= 0 && m.x < boardSize && m.y >= 0 && m.y < boardSize) {
      board[m.y][m.x] = m.color === 'BLACK' ? 1 : 2;
      lastMove = { x: m.x, y: m.y };
    }
  }

  boardRenderer.setState({
    boardSize,
    board,
    walls,
    portals,
    lastMove,
    winLine: null,
    firstMoveZones: [],
    showZones: false,
    interactive: false,
    isMyTurn: false,
  });

  // Update counter
  moveCounter.textContent = `${replayIndex} / ${moves.length}`;
}

// Controls
function goFirst() { replayIndex = 0; renderReplayBoard(); }
function goPrev()  { if (replayIndex > 0) { replayIndex--; renderReplayBoard(); } }
function goNext()  { if (replayData && replayIndex < replayData.moves.length) { replayIndex++; renderReplayBoard(); } }
function goLast()  { if (replayData) { replayIndex = replayData.moves.length; renderReplayBoard(); } }

function toggleAutoPlay() {
  if (autoPlayTimer) {
    stopAutoPlay();
  } else {
    startAutoPlay();
  }
}

function startAutoPlay() {
  if (!replayData) return;
  // If at end, restart from beginning
  if (replayIndex >= replayData.moves.length) replayIndex = 0;
  
  btnPlay.textContent = '⏸';
  btnPlay.classList.add('playing');
  autoPlayTimer = setInterval(() => {
    if (replayIndex >= replayData.moves.length) {
      stopAutoPlay();
      return;
    }
    replayIndex++;
    renderReplayBoard();
  }, 600);
}

function stopAutoPlay() {
  if (autoPlayTimer) {
    clearInterval(autoPlayTimer);
    autoPlayTimer = null;
  }
  btnPlay.textContent = '⏵';
  btnPlay.classList.remove('playing');
}

// ---------------------------------------------------------------------------
// Event listeners
// ---------------------------------------------------------------------------
btnFirst.addEventListener('click', () => { stopAutoPlay(); goFirst(); });
btnPrev.addEventListener('click',  () => { stopAutoPlay(); goPrev(); });
btnNext.addEventListener('click',  () => { stopAutoPlay(); goNext(); });
btnLast.addEventListener('click',  () => { stopAutoPlay(); goLast(); });
btnPlay.addEventListener('click',  toggleAutoPlay);
btnBack.addEventListener('click',  closeReplay);

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
  if (!replayData || viewReplay.style.display === 'none') return;
  if (e.key === 'ArrowLeft')  { stopAutoPlay(); goPrev(); }
  if (e.key === 'ArrowRight') { stopAutoPlay(); goNext(); }
  if (e.key === 'Home')       { stopAutoPlay(); goFirst(); }
  if (e.key === 'End')        { stopAutoPlay(); goLast(); }
  if (e.key === ' ')          { e.preventDefault(); toggleAutoPlay(); }
  if (e.key === 'Escape')     closeReplay();
});

// Expose for onclick
window.openReplay = openReplay;
window.loadGames  = loadGames;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
function getResultText(g) {
  if (!g.winner || g.winner === 'draw') return 'Hoà';
  // winner is a userId — match against player IDs
  if (g.winner === g.black_player_id) return g.black_player_name + ' thắng';
  if (g.winner === g.white_player_id) return g.white_player_name + ' thắng';
  // Fallback: winner might match the name or be the first player
  return 'Có người thắng';
}

function getResultTextFull(g) {
  const reasonMap = {
    normal: 'Thắng bình thường',
    resign: 'Đối thủ đầu hàng',
    timeout: 'Hết thời gian',
    draw_agreement: 'Đồng ý hoà',
    board_full: 'Bàn cờ đầy',
  };

  if (!g.winner || g.winner === 'draw') {
    return `Hoà — ${reasonMap[g.reason] || g.reason || ''}`;
  }

  let winnerName;
  if (g.winner === g.black_player_id) winnerName = g.black_player_name;
  else if (g.winner === g.white_player_id) winnerName = g.white_player_name;
  else winnerName = 'Người chơi';

  return `${winnerName} thắng — ${reasonMap[g.reason] || g.reason || ''}`;
}

function formatTime(isoStr) {
  if (!isoStr) return '—';
  try {
    const d = new Date(typeof isoStr === 'number' ? isoStr : isoStr);
    const now = new Date();
    const isToday = d.toDateString() === now.toDateString();
    if (isToday) {
      return d.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });
    }
    return d.toLocaleDateString('vi-VN', { day: '2-digit', month: '2-digit', year: 'numeric' })
      + ' ' + d.toLocaleTimeString('vi-VN', { hour: '2-digit', minute: '2-digit' });
  } catch {
    return '—';
  }
}

function escapeHtml(str) {
  const div = document.createElement('div');
  div.textContent = str || '';
  return div.innerHTML;
}

// ---------------------------------------------------------------------------
// Init: load first page, or open replay if URL has ?id=...
// ---------------------------------------------------------------------------
const urlParams = new URLSearchParams(window.location.search);
const urlGameId = urlParams.get('id');
if (urlGameId) {
  loadGames(1); // Load list in background
  openReplay(urlGameId);
} else {
  loadGames(1);
}
