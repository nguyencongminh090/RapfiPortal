/**
 * history.js — Game History & Replay Viewer with Tree Analysis.
 *
 * Features:
 *   - Paginated game list from /api/games
 *   - Replay viewer with step-through controls
 *   - Analysis mode: click board to add variations
 *   - Tree panel: visual move tree with click navigation
 *   - URL sharing via ?id=gameId
 *   - Keyboard shortcuts
 *
 * Reuses: BoardRenderer (board.js), MoveTree (move-tree.js), TreeView (tree-view.js)
 */

'use strict';

// ---------------------------------------------------------------------------
// Element refs
// ---------------------------------------------------------------------------
const viewList     = document.getElementById('view-list');
const viewReplay   = document.getElementById('view-replay');
const gameListEl   = document.getElementById('game-list');
const gameTotalEl  = document.getElementById('game-total');
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
const btnAnalysis   = document.getElementById('btn-analysis');
const treePanel     = document.getElementById('tree-panel');
const treeContainer = document.getElementById('tree-container');
const btnDeleteBranch = document.getElementById('btn-delete-branch');

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
let currentPage = 1;
let boardRenderer = null;
let autoPlayTimer = null;

// Tree-based state
let moveTree = null;      // MoveTree instance
let treeView = null;      // TreeView instance
let analysisMode = false;
let replayGameData = null; // Raw game data for info display

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
  if (p.totalPages <= 1) { paginationEl.innerHTML = ''; return; }

  let html = `<button ${p.page <= 1 ? 'disabled' : ''} onclick="loadGames(${p.page - 1})">‹</button>`;
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
    replayGameData = game;

    // Build MoveTree from the flat move history
    moveTree = MoveTree.fromMoveHistory(game.moves || [], {
      boardSize: game.board_size,
      walls: game.walls || [],
      portals: game.portals || [],
    });

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

    // Update URL for sharing
    history.replaceState(null, '', `history.html?id=${gameId}`);

    // Reset analysis mode
    setAnalysisMode(false);

    // Init board renderer (once)
    if (!boardRenderer) {
      boardRenderer = new BoardRenderer(replayCanvas, {
        boardSize: game.board_size,
        onCellClick: handleBoardClick,
      });
    }
    boardRenderer.boardSize = game.board_size;
    boardRenderer.interactive = false;

    // Init tree view (once)
    if (!treeView) {
      treeView = new TreeView(treeContainer, {
        onNodeClick: handleTreeNodeClick,
      });
    }

    // Let the DOM settle, then resize and render
    requestAnimationFrame(() => {
      boardRenderer.resize();
      syncBoardToTree();
    });
  } catch (err) {
    alert('Lỗi khi tải ván đấu.');
  }
}

function closeReplay() {
  stopAutoPlay();
  setAnalysisMode(false);
  viewReplay.style.display = 'none';
  viewList.style.display = '';
  moveTree = null;
  replayGameData = null;
  history.replaceState(null, '', 'history.html');
}

// ---------------------------------------------------------------------------
// Sync board display to current MoveTree position
// ---------------------------------------------------------------------------
function syncBoardToTree() {
  if (!moveTree || !boardRenderer) return;

  const { board, lastMove } = moveTree.getBoardState();
  const path = moveTree.getPath();
  const totalMainLine = countMainLine(moveTree.root);

  boardRenderer.setState({
    boardSize: moveTree.boardSize,
    board,
    walls: moveTree.walls,
    portals: moveTree.portals,
    lastMove,
    winLine: null,
    firstMoveZones: [],
    showZones: false,
    interactive: analysisMode,
    isMyTurn: analysisMode,
    myColor: analysisMode ? moveTree.getNextColor().toLowerCase() : null,
  });

  // Update counter
  moveCounter.textContent = `${path.length} / ${totalMainLine}`;

  // Update tree view
  if (treeView && treePanel.style.display !== 'none') {
    treeView.setTree(moveTree);
  }
}

/** Count total moves in the main line (following children[0]). */
function countMainLine(node) {
  let count = 0;
  let cur = node;
  while (cur.children.length > 0) {
    cur = cur.children[0];
    count++;
  }
  return count;
}

// ---------------------------------------------------------------------------
// Analysis Mode
// ---------------------------------------------------------------------------
function setAnalysisMode(on) {
  analysisMode = on;
  btnAnalysis.classList.toggle('active', on);
  treePanel.style.display = on ? '' : 'none';

  if (boardRenderer) {
    boardRenderer.interactive = on;
    boardRenderer.isMyTurn = on;
    if (on && moveTree) {
      boardRenderer.myColor = moveTree.getNextColor().toLowerCase();
    }
  }

  // Re-render tree when entering analysis mode
  if (on && treeView && moveTree) {
    treeView.setTree(moveTree);
  }
}

function toggleAnalysis() {
  setAnalysisMode(!analysisMode);
  // Let the DOM settle (tree panel show/hide), then resize board
  requestAnimationFrame(() => {
    if (boardRenderer) boardRenderer.resize();
    syncBoardToTree();
  });
}

// ---------------------------------------------------------------------------
// Board click handler (analysis mode)
// ---------------------------------------------------------------------------
function handleBoardClick(x, y) {
  if (!analysisMode || !moveTree) return;

  // Check if cell is available
  if (!moveTree.isCellAvailable(x, y)) return;

  const color = moveTree.getNextColor();
  moveTree.addMove(x, y, color);
  syncBoardToTree();
}

// ---------------------------------------------------------------------------
// Tree node click handler
// ---------------------------------------------------------------------------
function handleTreeNodeClick(node) {
  if (!moveTree) return;
  moveTree.goToNode(node);
  syncBoardToTree();
}

// ---------------------------------------------------------------------------
// Navigation controls
// ---------------------------------------------------------------------------
function goFirst() {
  if (!moveTree) return;
  moveTree.goToStart();
  syncBoardToTree();
}

function goPrev() {
  if (!moveTree) return;
  moveTree.goBack();
  syncBoardToTree();
}

function goNext() {
  if (!moveTree) return;
  moveTree.goForward();
  syncBoardToTree();
}

function goLast() {
  if (!moveTree) return;
  moveTree.goToEnd();
  syncBoardToTree();
}

function toggleAutoPlay() {
  if (autoPlayTimer) stopAutoPlay();
  else startAutoPlay();
}

function startAutoPlay() {
  if (!moveTree) return;
  if (moveTree.currentNode.isLeaf) moveTree.goToStart();
  
  btnPlay.textContent = '⏸';
  btnPlay.classList.add('playing');
  autoPlayTimer = setInterval(() => {
    if (!moveTree.goForward()) {
      stopAutoPlay();
      return;
    }
    syncBoardToTree();
  }, 600);
}

function stopAutoPlay() {
  if (autoPlayTimer) { clearInterval(autoPlayTimer); autoPlayTimer = null; }
  btnPlay.textContent = '⏵';
  btnPlay.classList.remove('playing');
}

// ---------------------------------------------------------------------------
// Delete current branch
// ---------------------------------------------------------------------------
function deleteBranch() {
  if (!moveTree || !moveTree.currentNode || moveTree.currentNode.isRoot) return;
  if (!confirm('Xoá nhánh này và tất cả các nước sau?')) return;
  moveTree.deleteNode(moveTree.currentNode);
  syncBoardToTree();
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
btnAnalysis.addEventListener('click', toggleAnalysis);
if (btnDeleteBranch) btnDeleteBranch.addEventListener('click', deleteBranch);

// Keyboard shortcuts
document.addEventListener('keydown', (e) => {
  if (!moveTree || viewReplay.style.display === 'none') return;
  // Don't capture if typing in an input
  if (e.target.tagName === 'INPUT' || e.target.tagName === 'TEXTAREA') return;

  switch (e.key) {
    case 'ArrowLeft':  stopAutoPlay(); goPrev(); e.preventDefault(); break;
    case 'ArrowRight': stopAutoPlay(); goNext(); e.preventDefault(); break;
    case 'Home':       stopAutoPlay(); goFirst(); e.preventDefault(); break;
    case 'End':        stopAutoPlay(); goLast(); e.preventDefault(); break;
    case ' ':          e.preventDefault(); toggleAutoPlay(); break;
    case 'Escape':     closeReplay(); break;
    case 'a': case 'A': toggleAnalysis(); break;
    case 'Delete':     if (analysisMode) deleteBranch(); break;
  }
});

// Expose for inline onclick
window.openReplay = openReplay;
window.loadGames  = loadGames;

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------
/**
 * Resolve winner display name from game record.
 * Handles both new data (player IDs stored) and old data (player IDs null).
 */
function resolveWinnerName(g) {
  if (!g.winner || g.winner === 'draw') return null;
  // Match by player ID (new data)
  if (g.winner === g.black_player_id) return g.black_player_name;
  if (g.winner === g.white_player_id) return g.white_player_name;
  // Fallback: winner might be a name directly, or match via name
  if (g.winner === g.black_player_name) return g.black_player_name;
  if (g.winner === g.white_player_name) return g.white_player_name;
  // Last resort: show winner raw value truncated
  return g.winner.length > 15 ? g.black_player_name : g.winner;
}

function getResultText(g) {
  if (!g.winner || g.winner === 'draw') return 'Hoà';
  const name = resolveWinnerName(g);
  return name ? `${name} thắng` : 'Có người thắng';
}

function getResultTextFull(g) {
  const reasonMap = {
    normal: '5 liên tiếp',
    resign: 'Đầu hàng',
    timeout: 'Hết giờ',
    draw_agreement: 'Đồng ý hoà',
    board_full: 'Bàn cờ đầy',
  };

  if (!g.winner || g.winner === 'draw') {
    return `Hoà — ${reasonMap[g.reason] || g.reason || ''}`;
  }

  const name = resolveWinnerName(g) || 'Người chơi';
  const reason = reasonMap[g.reason] || g.reason || '';
  return `${name} thắng${reason ? ' — ' + reason : ''}`;
}

function formatTime(isoStr) {
  if (!isoStr) return '—';
  try {
    const d = new Date(typeof isoStr === 'number' ? isoStr : isoStr);
    if (isNaN(d.getTime())) return '—';
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
// Init
// ---------------------------------------------------------------------------
const urlParams = new URLSearchParams(window.location.search);
const urlGameId = urlParams.get('id');
if (urlGameId) {
  loadGames(1);
  openReplay(urlGameId);
} else {
  loadGames(1);
}
