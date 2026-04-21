'use strict';

/**
 * socket-client.js — Socket.io client wrapper with JWT auth.
 *
 * Provides:
 *   - Automatic JWT injection from localStorage
 *   - Reconnection with token
 *   - Auth failure → redirect to login
 *   - Connection status tracking
 *
 * Usage:
 *   const client = new SocketClient();
 *   client.on('lobby:update', (data) => { ... });
 *   client.emit('room:create', { settings: { ... } });
 */

// eslint-disable-next-line no-unused-vars
class SocketClient {
  constructor() {
    this.socket = null;
    this._listeners = [];
    this._statusEl = null;

    this._connect();
  }

  // ---------------------------------------------------------------------------
  // Connection
  // ---------------------------------------------------------------------------

  _connect() {
    const token = localStorage.getItem('gvn_token');
    if (!token) {
      window.location.replace('login.html');
      return;
    }

    // Connect with JWT in auth handshake
    this.socket = io({
      auth: { token },
      reconnection: true,
      reconnectionAttempts: Infinity,
      reconnectionDelay: 1000,
      reconnectionDelayMax: 5000,
    });

    // ── Connection lifecycle ──────────────────────────────────────────
    this.socket.on('connect', () => {
      this._setStatus('connected');
    });

    this.socket.on('disconnect', (reason) => {
      this._setStatus('disconnected', reason);
    });

    this.socket.on('reconnect_attempt', (attempt) => {
      this._setStatus('reconnecting', attempt);
    });

    this.socket.on('reconnect', () => {
      this._setStatus('connected');
    });

    // Auth failure → back to login
    this.socket.on('connect_error', (err) => {
      if (err.message === 'AUTH_REQUIRED' || err.message === 'AUTH_INVALID') {
        localStorage.removeItem('gvn_token');
        localStorage.removeItem('gvn_display_name');
        window.location.replace('login.html');
      }
    });
  }

  // ---------------------------------------------------------------------------
  // Public API
  // ---------------------------------------------------------------------------

  /** Register an event listener. Returns this for chaining. */
  on(event, callback) {
    if (!this.socket) return this;
    this.socket.on(event, callback);
    this._listeners.push({ event, callback });
    return this;
  }

  /** Emit an event to the server. */
  emit(event, data) {
    if (!this.socket) return;
    this.socket.emit(event, data);
  }

  /** Remove all registered listeners and disconnect. */
  destroy() {
    if (!this.socket) return;
    for (const { event, callback } of this._listeners) {
      this.socket.off(event, callback);
    }
    this._listeners = [];
    this.socket.disconnect();
    this.socket = null;
  }

  /** Get current user info from stored JWT (client-side decode). */
  getUserInfo() {
    const token = localStorage.getItem('gvn_token');
    if (!token) return null;
    try {
      let payload = token.split('.')[1];
      payload = payload.replace(/-/g, '+').replace(/_/g, '/');
      const decodedPayload = decodeURIComponent(
        atob(payload).split('').map(function(c) {
          return '%' + ('00' + c.charCodeAt(0).toString(16)).slice(-2);
        }).join('')
      );
      return JSON.parse(decodedPayload);
    } catch {
      return null;
    }
  }

  /** Log out: clear storage, redirect to login. */
  logout() {
    localStorage.removeItem('gvn_token');
    localStorage.removeItem('gvn_display_name');
    this.destroy();
    window.location.replace('login.html');
  }

  // ---------------------------------------------------------------------------
  // Status banner
  // ---------------------------------------------------------------------------

  /** Bind a status banner DOM element. */
  bindStatusBanner(el) {
    this._statusEl = el;
  }

  _setStatus(status, detail) {
    if (!this._statusEl) return;

    switch (status) {
      case 'connected':
        this._statusEl.classList.remove('visible');
        break;
      case 'disconnected':
        this._statusEl.textContent = 'Mất kết nối. Đang thử kết nối lại...';
        this._statusEl.classList.add('visible');
        break;
      case 'reconnecting':
        this._statusEl.textContent = `Kết nối lại... (lần ${detail})`;
        this._statusEl.classList.add('visible');
        break;
    }
  }
}
