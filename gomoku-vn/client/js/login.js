'use strict';

/**
 * login.js — Login page controller.
 *
 * Responsibilities:
 *   - Tab switching (Đăng nhập / Đăng ký)
 *   - Client-side field validation with Vietnamese messages
 *   - API calls to /api/auth/{login,register,guest}
 *   - Store JWT in localStorage, redirect to index.html on success
 */

// ---------------------------------------------------------------------------
// Constants
// ---------------------------------------------------------------------------
const API_BASE = '';   // Same origin

// ---------------------------------------------------------------------------
// Redirect if already logged in
// ---------------------------------------------------------------------------
(function checkExistingSession() {
  const token = localStorage.getItem('gvn_token');
  if (token) {
    // Quick sanity check — don't bother decoding, server will reject if expired
    window.location.replace('index.html');
  }
})();

// ---------------------------------------------------------------------------
// Element refs
// ---------------------------------------------------------------------------
const alertBanner    = document.getElementById('alert-banner');

// Login form
const formLogin      = document.getElementById('form-login');
const loginUsername  = document.getElementById('login-username');
const loginPassword  = document.getElementById('login-password');
const btnLogin       = document.getElementById('btn-login');

// Register form
const formRegister   = document.getElementById('form-register');
const regUsername    = document.getElementById('reg-username');
const regDisplay     = document.getElementById('reg-display');
const regPassword    = document.getElementById('reg-password');
const regConfirm     = document.getElementById('reg-confirm');
const btnRegister    = document.getElementById('btn-register');

// Guest
const btnGuest       = document.getElementById('btn-guest');

// ---------------------------------------------------------------------------
// Tab switching
// ---------------------------------------------------------------------------
function switchTab(which) {
  const tabs   = document.querySelectorAll('.tab-btn');
  const panels = document.querySelectorAll('.panel');

  tabs.forEach(t => {
    const active = t.id === `tab-${which}`;
    t.classList.toggle('active', active);
    t.setAttribute('aria-selected', active ? 'true' : 'false');
  });
  panels.forEach(p => {
    p.classList.toggle('active', p.id === `panel-${which}`);
  });

  hideAlert();
}
// Expose for inline onclick handlers in HTML
window.switchTab = switchTab;

// ---------------------------------------------------------------------------
// Alert helpers
// ---------------------------------------------------------------------------
function showAlert(message, type = 'error') {
  alertBanner.textContent = message;
  alertBanner.className = `alert alert-${type} visible`;
}
function hideAlert() {
  alertBanner.classList.remove('visible');
}

// ---------------------------------------------------------------------------
// Field error helpers
// ---------------------------------------------------------------------------
function setFieldError(input, errorId, message) {
  const errEl = document.getElementById(errorId);
  if (message) {
    input.classList.add('error');
    errEl.textContent = message;
    errEl.classList.add('visible');
  } else {
    input.classList.remove('error');
    errEl.classList.remove('visible');
  }
}
function clearFieldErrors(...pairs) {
  // pairs: [input, errorId]
  pairs.forEach(([input, id]) => setFieldError(input, id, ''));
}

// ---------------------------------------------------------------------------
// Loading state helpers
// ---------------------------------------------------------------------------
function setLoading(btn, loading, originalText) {
  if (loading) {
    btn.disabled = true;
    btn.innerHTML = `<span class="spinner" aria-hidden="true"></span>${originalText}...`;
  } else {
    btn.disabled = false;
    btn.textContent = originalText;
  }
}

// ---------------------------------------------------------------------------
// API call wrapper
// ---------------------------------------------------------------------------
async function apiPost(endpoint, body) {
  const res = await fetch(`${API_BASE}/api/auth/${endpoint}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json' },
    body: JSON.stringify(body),
  });
  const data = await res.json();
  return { ok: res.ok, status: res.status, data };
}

// ---------------------------------------------------------------------------
// Save JWT and redirect to lobby
// ---------------------------------------------------------------------------
function onAuthSuccess(token, displayName) {
  localStorage.setItem('gvn_token', token);
  localStorage.setItem('gvn_display_name', displayName);
  window.location.replace('index.html');
}

// ---------------------------------------------------------------------------
// Client-side validation helpers
// ---------------------------------------------------------------------------
const RE_USERNAME = /^[a-zA-Z0-9_]{3,20}$/;

function validateLoginForm() {
  let valid = true;
  clearFieldErrors(
    [loginUsername, 'err-login-username'],
    [loginPassword, 'err-login-password']
  );
  if (!loginUsername.value.trim()) {
    setFieldError(loginUsername, 'err-login-username', t('login.err_username'));
    valid = false;
  }
  if (!loginPassword.value) {
    setFieldError(loginPassword, 'err-login-password', t('login.err_password'));
    valid = false;
  }
  return valid;
}

function validateRegisterForm() {
  let valid = true;
  clearFieldErrors(
    [regUsername, 'err-reg-username'],
    [regDisplay,  'err-reg-display'],
    [regPassword, 'err-reg-password'],
    [regConfirm,  'err-reg-confirm']
  );

  if (!RE_USERNAME.test(regUsername.value)) {
    setFieldError(regUsername, 'err-reg-username',
      t('login.err_reg_username'));
    valid = false;
  }
  if (regDisplay.value.trim().length < 2 || regDisplay.value.trim().length > 24) {
    setFieldError(regDisplay, 'err-reg-display', t('login.err_display'));
    valid = false;
  }
  if (regPassword.value.length < 6) {
    setFieldError(regPassword, 'err-reg-password', t('login.err_pass_short'));
    valid = false;
  }
  if (regPassword.value !== regConfirm.value) {
    setFieldError(regConfirm, 'err-reg-confirm', t('login.err_confirm'));
    valid = false;
  }
  return valid;
}

// ---------------------------------------------------------------------------
// Form submit handlers
// ---------------------------------------------------------------------------
formLogin.addEventListener('submit', async (e) => {
  e.preventDefault();
  hideAlert();

  if (!validateLoginForm()) return;

  setLoading(btnLogin, true, t('login.btn_login'));
  try {
    const { ok, data } = await apiPost('login', {
      username: loginUsername.value.trim(),
      password: loginPassword.value,
    });

    if (ok) {
      onAuthSuccess(data.token, data.displayName);
    } else {
      showAlert(data.error || t('login.err_login_fail'));
    }
  } catch {
    showAlert(t('login.err_network'));
  } finally {
    setLoading(btnLogin, false, t('login.btn_login'));
  }
});

formRegister.addEventListener('submit', async (e) => {
  e.preventDefault();
  hideAlert();

  if (!validateRegisterForm()) return;

  setLoading(btnRegister, true, t('login.btn_register'));
  try {
    const { ok, data } = await apiPost('register', {
      username:    regUsername.value.trim(),
      displayName: regDisplay.value.trim(),
      password:    regPassword.value,
    });

    if (ok) {
      onAuthSuccess(data.token, data.displayName);
    } else {
      showAlert(data.error || t('login.err_register_fail'));
    }
  } catch {
    showAlert(t('login.err_network'));
  } finally {
    setLoading(btnRegister, false, t('login.btn_register'));
  }
});

btnGuest.addEventListener('click', async () => {
  hideAlert();
  setLoading(btnGuest, true, t('login.btn_guest'));
  try {
    const { ok, data } = await apiPost('guest', {});
    if (ok) {
      onAuthSuccess(data.token, data.displayName);
    } else {
      showAlert(data.error || t('login.err_guest_fail'));
    }
  } catch {
    showAlert(t('login.err_network'));
  } finally {
    setLoading(btnGuest, false, t('login.btn_guest'));
  }
});

// ---------------------------------------------------------------------------
// Real-time field validation (on blur)
// ---------------------------------------------------------------------------
loginUsername.addEventListener('blur', () => {
  if (!loginUsername.value.trim()) {
    setFieldError(loginUsername, 'err-login-username', t('login.err_username'));
  } else {
    setFieldError(loginUsername, 'err-login-username', '');
  }
});

regUsername.addEventListener('blur', () => {
  if (!RE_USERNAME.test(regUsername.value)) {
    setFieldError(regUsername, 'err-reg-username',
      t('login.err_reg_username'));
  } else {
    setFieldError(regUsername, 'err-reg-username', '');
  }
});

regConfirm.addEventListener('input', () => {
  if (regPassword.value && regConfirm.value && regPassword.value !== regConfirm.value) {
    setFieldError(regConfirm, 'err-reg-confirm', t('login.err_confirm'));
  } else {
    setFieldError(regConfirm, 'err-reg-confirm', '');
  }
});
