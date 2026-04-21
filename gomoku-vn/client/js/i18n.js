'use strict';

/**
 * i18n.js — Lightweight internationalization for Play3CR.
 *
 * Usage:
 *   - Static HTML: add data-i18n="key" to elements
 *   - Placeholders:  add data-i18n-placeholder="key" to inputs
 *   - Dynamic JS:    t('key') or t('key', { name: 'Alice' })
 *   - Call applyI18n() after page load and after language change
 *
 * Supports: vi (Vietnamese, default), en (English)
 */

// ---------------------------------------------------------------------------
// Translation dictionaries
// ---------------------------------------------------------------------------
const TRANSLATIONS = {
  vi: {
    // ── Common / Nav ──────────────────────────────────────────
    'nav.logout':           'Đăng xuất',
    'nav.leave':            'Rời phòng',
    'nav.guest_badge':      'Khách',

    // ── Login page ────────────────────────────────────────────
    'login.title':          'Play3CR — Đăng nhập',
    'login.subtitle':       'Cờ Caro trực tuyến — Luật Wall & Portal',
    'login.tab_login':      'Đăng nhập',
    'login.tab_register':   'Đăng ký',
    'login.username':       'Tên đăng nhập',
    'login.password':       'Mật khẩu',
    'login.ph_username':    'Nhập tên đăng nhập',
    'login.ph_password':    'Nhập mật khẩu',
    'login.btn_login':      'Đăng nhập',
    'login.display_name':   'Tên hiển thị',
    'login.ph_display':     'Tên xuất hiện trong phòng',
    'login.ph_reg_user':    '3-20 ký tự, chữ/số/gạch dưới',
    'login.ph_reg_pass':    'Ít nhất 6 ký tự',
    'login.confirm_pass':   'Xác nhận mật khẩu',
    'login.ph_confirm':     'Nhập lại mật khẩu',
    'login.btn_register':   'Tạo tài khoản',
    'login.divider':        'hoặc',
    'login.btn_guest':      'Khách',
    'login.footer':         'Play3CR',

    // Login validation
    'login.err_username':       'Vui lòng nhập tên đăng nhập.',
    'login.err_password':       'Vui lòng nhập mật khẩu.',
    'login.err_reg_username':   'Tên đăng nhập 3-20 ký tự, chỉ gồm chữ cái, số và dấu gạch dưới.',
    'login.err_display':        'Tên hiển thị từ 2-24 ký tự.',
    'login.err_pass_short':     'Mật khẩu phải có ít nhất 6 ký tự.',
    'login.err_confirm':        'Mật khẩu xác nhận không khớp.',
    'login.err_login_fail':     'Đăng nhập thất bại. Vui lòng thử lại.',
    'login.err_register_fail':  'Đăng ký thất bại. Vui lòng thử lại.',
    'login.err_guest_fail':     'Không thể tạo phiên khách. Vui lòng thử lại.',
    'login.err_network':        'Không thể kết nối máy chủ. Vui lòng kiểm tra mạng.',

    // ── Lobby page ────────────────────────────────────────────
    'lobby.title':          'Play3CR — Sảnh',
    'lobby.room_list':      'Danh sách phòng',
    'lobby.btn_create':     'Tạo phòng',
    'lobby.no_rooms':       'Chưa có phòng nào. Hãy tạo phòng mới!',
    'lobby.th_room':        'Phòng',
    'lobby.th_host':        'Chủ phòng',
    'lobby.th_players':     'Người chơi',
    'lobby.th_status':      'Trạng thái',
    'lobby.th_rules':       'Luật',
    'lobby.btn_join':       'Tham gia',
    'lobby.state_playing':  'Đang chơi',
    'lobby.state_interrupted': 'Gián đoạn',
    'lobby.state_ready':    'Chờ sẵn sàng',
    'lobby.state_waiting':  'Đang chờ',
    'lobby.rule_basic':     'Cơ bản',

    // ── Create room modal ─────────────────────────────────────
    'modal.title':          'Tạo phòng mới',
    'modal.close':          'Đóng',
    'modal.room_name':      'Tên phòng',
    'modal.ph_room_name':   'Tùy chọn',
    'modal.board_size':     'Kích thước bàn',
    'modal.special_rules':  'Luật đặc biệt',
    'modal.rule_wall':      'Ô khoá',
    'modal.rule_portal':    'Hố đen',
    'modal.timer_mode':     'Bộ đếm giờ',
    'modal.per_move':       'Mỗi nước',
    'modal.per_game':       'Mỗi ván',
    'modal.time_label':     'Thời gian',
    'modal.time_unit':      'giây',
    'modal.btn_cancel':     'Huỷ',
    'modal.btn_confirm':    'Tạo phòng',

    // ── Room page ─────────────────────────────────────────────
    'room.title':           'Play3CR — Phòng chơi',
    'room.score_title':     'Bảng điểm',
    'room.th_name':         'Tên',
    'room.th_win':          'Thắng',
    'room.th_loss':         'Thua',
    'room.th_draw':         'Hoà',
    'room.placeholder':     'Chờ người chơi ngồi vào và sẵn sàng...',
    'room.tab_chat':        'Trò chuyện',
    'room.tab_spectators':  'Khán giả',
    'room.tab_settings':    'Cài đặt',
    'room.ph_chat':         'Nhập tin nhắn...',
    'room.btn_send':        'Gửi',

    // ── Room dynamic text ─────────────────────────────────────
    'room.not_ready':       'Chưa sẵn sàng',
    'room.ready':           'Sẵn sàng',
    'room.empty_slot':      'Trống',
    'room.click_to_sit':    'Nhấn để ngồi vào',
    'room.host':            'Chủ phòng',
    'room.btn_ready':       'Sẵn sàng',
    'room.btn_unready':     'Huỷ sẵn sàng',
    'room.btn_stand':       'Đứng dậy',
    'room.btn_kick':        'Mời ra',
    'room.confirm_leave':   'Ván đấu đang diễn ra. Bạn có chắc chắn muốn rời phòng? (Bạn sẽ bị xử thua)',
    'room.kicked':          'Bạn đã bị mời ra khỏi phòng.',
    'room.reconnected':     'Đối thủ đã kết nối lại! Ván đấu tiếp tục.',
    'room.disconnected':    '{name} mất kết nối. Chờ kết nối lại ({seconds}s)...',
    'room.rule_wall':       'Ô khoá',
    'room.rule_portal':     'Hố đen',
    'room.rule_basic':      'Cơ bản',
    'room.timer_per_move':  'Mỗi nước',
    'room.timer_per_game':  'Mỗi ván',

    // ── Game UI ───────────────────────────────────────────────
    'game.your_turn':       'Lượt của bạn',
    'game.opponent_turn':   'Đối thủ đang đi...',
    'game.finished':        'Kết thúc',
    'game.btn_resign':      'Đầu hàng',
    'game.btn_draw':        'Đề nghị hoà',
    'game.btn_time':        'Xin thêm giờ',
    'game.confirm_resign':  'Bạn có chắc muốn đầu hàng?',
    'game.draw_offer':      '{name} đề nghị hoà',
    'game.btn_accept':      'Chấp nhận',
    'game.btn_decline':     'Từ chối',
    'game.focus_title':     'Chế độ tập trung',

    // ── Game overlay ──────────────────────────────────────────
    'overlay.win':          'Bạn thắng!',
    'overlay.lose':         'Bạn thua',
    'overlay.draw':         'Hoà!',
    'overlay.reason_win':   'Xếp được 5+ quân liên tiếp',
    'overlay.reason_resign_w': 'Đối thủ đầu hàng',
    'overlay.reason_resign_l': 'Bạn đã đầu hàng',
    'overlay.reason_timeout_w': 'Đối thủ hết thời gian',
    'overlay.reason_timeout_l': 'Bạn đã hết thời gian',
    'overlay.reason_draw_agree': 'Hai bên đồng ý hoà',
    'overlay.reason_draw_full':  'Bàn cờ đã đầy',
    'overlay.btn_rematch':  'Chơi lại',
    'overlay.btn_close':    'Đóng',

    // ── Settings panel (room) ─────────────────────────────────
    'settings.board_size':  'Bàn cờ',
    'settings.rules':       'Luật chơi',
    'settings.timer':       'Bộ hẹn giờ',
  },

  en: {
    // ── Common / Nav ──────────────────────────────────────────
    'nav.logout':           'Log out',
    'nav.leave':            'Leave room',
    'nav.guest_badge':      'Guest',

    // ── Login page ────────────────────────────────────────────
    'login.title':          'Play3CR — Login',
    'login.subtitle':       'Online Gomoku — Wall & Portal Rules',
    'login.tab_login':      'Login',
    'login.tab_register':   'Register',
    'login.username':       'Username',
    'login.password':       'Password',
    'login.ph_username':    'Enter username',
    'login.ph_password':    'Enter password',
    'login.btn_login':      'Login',
    'login.display_name':   'Display name',
    'login.ph_display':     'Name shown in rooms',
    'login.ph_reg_user':    '3-20 chars, letters/numbers/underscore',
    'login.ph_reg_pass':    'At least 6 characters',
    'login.confirm_pass':   'Confirm password',
    'login.ph_confirm':     'Re-enter password',
    'login.btn_register':   'Create account',
    'login.divider':        'or',
    'login.btn_guest':      'Play as guest',
    'login.footer':         'Play3CR · Self-hosted · Local data',

    // Login validation
    'login.err_username':       'Please enter your username.',
    'login.err_password':       'Please enter your password.',
    'login.err_reg_username':   'Username must be 3-20 characters (letters, numbers, underscore only).',
    'login.err_display':        'Display name must be 2-24 characters.',
    'login.err_pass_short':     'Password must be at least 6 characters.',
    'login.err_confirm':        'Passwords do not match.',
    'login.err_login_fail':     'Login failed. Please try again.',
    'login.err_register_fail':  'Registration failed. Please try again.',
    'login.err_guest_fail':     'Could not create guest session. Please try again.',
    'login.err_network':        'Cannot connect to server. Please check your network.',

    // ── Lobby page ────────────────────────────────────────────
    'lobby.title':          'Play3CR — Lobby',
    'lobby.room_list':      'Room List',
    'lobby.btn_create':     'Create room',
    'lobby.no_rooms':       'No rooms yet. Create a new one!',
    'lobby.th_room':        'Room',
    'lobby.th_host':        'Host',
    'lobby.th_players':     'Players',
    'lobby.th_status':      'Status',
    'lobby.th_rules':       'Rules',
    'lobby.btn_join':       'Join',
    'lobby.state_playing':  'Playing',
    'lobby.state_interrupted': 'Interrupted',
    'lobby.state_ready':    'Ready to start',
    'lobby.state_waiting':  'Waiting',
    'lobby.rule_basic':     'Basic',

    // ── Create room modal ─────────────────────────────────────
    'modal.title':          'Create new room',
    'modal.close':          'Close',
    'modal.room_name':      'Room name',
    'modal.ph_room_name':   'Optional (auto-generated if empty)',
    'modal.board_size':     'Board size',
    'modal.special_rules':  'Special rules',
    'modal.rule_wall':      'Wall',
    'modal.rule_portal':    'Portal (Teleport)',
    'modal.timer_mode':     'Timer mode',
    'modal.per_move':       'Per move',
    'modal.per_game':       'Per game',
    'modal.time_label':     'Time',
    'modal.time_unit':      'sec',
    'modal.btn_cancel':     'Cancel',
    'modal.btn_confirm':    'Create room',

    // ── Room page ─────────────────────────────────────────────
    'room.title':           'Play3CR — Room',
    'room.score_title':     'Scoreboard',
    'room.th_name':         'Name',
    'room.th_win':          'W',
    'room.th_loss':         'L',
    'room.th_draw':         'D',
    'room.placeholder':     'Waiting for players to sit and ready...',
    'room.tab_chat':        'Chat',
    'room.tab_spectators':  'Spectators',
    'room.tab_settings':    'Settings',
    'room.ph_chat':         'Type a message...',
    'room.btn_send':        'Send',

    // ── Room dynamic text ─────────────────────────────────────
    'room.not_ready':       'Not ready',
    'room.ready':           'Ready',
    'room.empty_slot':      'Empty',
    'room.click_to_sit':    'Click to sit',
    'room.host':            'Host',
    'room.btn_ready':       'Ready',
    'room.btn_unready':     'Cancel ready',
    'room.btn_stand':       'Stand up',
    'room.btn_kick':        'Kick',
    'room.confirm_leave':   'A game is in progress. Are you sure you want to leave? (You will forfeit)',
    'room.kicked':          'You have been kicked from the room.',
    'room.reconnected':     'Opponent reconnected! Game resumes.',
    'room.disconnected':    '{name} disconnected. Waiting to reconnect ({seconds}s)...',
    'room.rule_wall':       'Wall',
    'room.rule_portal':     'Portal',
    'room.rule_basic':      'Basic',
    'room.timer_per_move':  'Per move',
    'room.timer_per_game':  'Per game',

    // ── Game UI ───────────────────────────────────────────────
    'game.your_turn':       'Your turn',
    'game.opponent_turn':   'Opponent\'s turn...',
    'game.finished':        'Finished',
    'game.btn_resign':      'Resign',
    'game.btn_draw':        'Offer draw',
    'game.btn_time':        'Request time',
    'game.confirm_resign':  'Are you sure you want to resign?',
    'game.draw_offer':      '{name} offers a draw',
    'game.btn_accept':      'Accept',
    'game.btn_decline':     'Decline',
    'game.focus_title':     'Focus mode',

    // ── Game overlay ──────────────────────────────────────────
    'overlay.win':          'You win!',
    'overlay.lose':         'You lose',
    'overlay.draw':         'Draw!',
    'overlay.reason_win':   'Five in a row',
    'overlay.reason_resign_w': 'Opponent resigned',
    'overlay.reason_resign_l': 'You resigned',
    'overlay.reason_timeout_w': 'Opponent ran out of time',
    'overlay.reason_timeout_l': 'You ran out of time',
    'overlay.reason_draw_agree': 'Both players agreed to draw',
    'overlay.reason_draw_full':  'Board is full',
    'overlay.btn_rematch':  'Rematch',
    'overlay.btn_close':    'Close',

    // ── Settings panel (room) ─────────────────────────────────
    'settings.board_size':  'Board',
    'settings.rules':       'Rules',
    'settings.timer':       'Timer',
  },
};

// ---------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------
const STORAGE_KEY = 'play3cr_lang';
let currentLang = localStorage.getItem(STORAGE_KEY) || 'vi';

// ---------------------------------------------------------------------------
// Core translate function
// ---------------------------------------------------------------------------
/**
 * Translate a key, with optional interpolation.
 * @param {string} key   — dot-separated key, e.g. 'login.btn_login'
 * @param {Object} [vars] — interpolation values, e.g. { name: 'Alice' }
 * @returns {string}
 */
function t(key, vars) {
  const dict = TRANSLATIONS[currentLang] || TRANSLATIONS['vi'];
  let text = dict[key] || TRANSLATIONS['vi'][key] || key;
  if (vars) {
    for (const [k, v] of Object.entries(vars)) {
      text = text.replace(new RegExp(`\\{${k}\\}`, 'g'), v);
    }
  }
  return text;
}

// ---------------------------------------------------------------------------
// Apply translations to DOM
// ---------------------------------------------------------------------------
/**
 * Scan all elements with data-i18n and data-i18n-placeholder attributes
 * and replace their text content / placeholder with the translated string.
 */
function applyI18n(root = document) {
  // Text content
  root.querySelectorAll('[data-i18n]').forEach(el => {
    const key = el.getAttribute('data-i18n');
    el.textContent = t(key);
  });

  // Placeholders
  root.querySelectorAll('[data-i18n-placeholder]').forEach(el => {
    const key = el.getAttribute('data-i18n-placeholder');
    el.placeholder = t(key);
  });

  // Title attributes
  root.querySelectorAll('[data-i18n-title]').forEach(el => {
    const key = el.getAttribute('data-i18n-title');
    el.title = t(key);
  });

  // Aria labels
  root.querySelectorAll('[data-i18n-aria]').forEach(el => {
    const key = el.getAttribute('data-i18n-aria');
    el.setAttribute('aria-label', t(key));
  });

  // Update <html lang>
  document.documentElement.lang = currentLang;
}

// ---------------------------------------------------------------------------
// Language switching
// ---------------------------------------------------------------------------
function setLanguage(lang) {
  if (!TRANSLATIONS[lang]) return;
  currentLang = lang;
  localStorage.setItem(STORAGE_KEY, lang);
  applyI18n();

  // Update page title based on current page
  updatePageTitle();

  // Dispatch event so JS modules can re-render dynamic content
  window.dispatchEvent(new CustomEvent('langchange', { detail: { lang } }));
}

function getLanguage() {
  return currentLang;
}

function updatePageTitle() {
  const path = window.location.pathname;
  if (path.includes('login')) {
    document.title = t('login.title');
  } else if (path.includes('room')) {
    document.title = t('room.title');
  } else {
    document.title = t('lobby.title');
  }
}

// ---------------------------------------------------------------------------
// Language Switcher UI — injects a toggle button
// ---------------------------------------------------------------------------
function createLangSwitcher(container) {
  if (!container) return;

  const btn = document.createElement('button');
  btn.className = 'lang-switch';
  btn.type = 'button';
  btn.id = 'btn-lang';
  btn.textContent = currentLang === 'vi' ? 'EN' : 'VI';
  btn.title = currentLang === 'vi' ? 'Switch to English' : 'Chuyển sang Tiếng Việt';

  btn.addEventListener('click', () => {
    const newLang = currentLang === 'vi' ? 'en' : 'vi';
    setLanguage(newLang);
    btn.textContent = newLang === 'vi' ? 'EN' : 'VI';
    btn.title = newLang === 'vi' ? 'Switch to English' : 'Chuyển sang Tiếng Việt';
  });

  container.appendChild(btn);
}

// ---------------------------------------------------------------------------
// Auto-init on load
// ---------------------------------------------------------------------------
document.addEventListener('DOMContentLoaded', () => {
  applyI18n();

  // Inject language switcher into topnav or login card header
  const topnavRight = document.querySelector('.topnav__right');
  const cardLogo = document.querySelector('.card__logo');

  if (topnavRight) {
    createLangSwitcher(topnavRight);
  } else if (cardLogo) {
    createLangSwitcher(cardLogo);
  }
});
