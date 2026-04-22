'use strict';

/**
 * RoomManager.js — Room CRUD, lifecycle, idle cleanup.
 *
 * Single source of truth for all room state (stored in RAM via Map objects).
 * Rooms are never persisted to SQLite — only completed games are.
 *
 * Room structure:
 *   { roomId, host, users: Map, joinOrder: [], settings, state, gameState,
 *     scoreTable: {}, lastActivity, createdAt }
 *
 * Manual test checklist:
 *   [ ] createRoom enforces MAX_ROOMS
 *   [ ] joinRoom enforces MAX_USERS_PER_ROOM
 *   [ ] leaveRoom auto-destroys empty rooms
 *   [ ] leaveRoom transfers host to next user in join-order
 *   [ ] listRooms returns correct summary for each room
 *   [ ] idleCleanup destroys rooms with no activity for IDLE_TIMEOUT_MS
 */

const { v4: uuidv4 } = require('uuid');
const config = require('../config');
const logger = require('../utils/logger');
const EventEmitter = require('events');

// =============================================================================
// RoomManager — singleton
// =============================================================================

class RoomManager extends EventEmitter {
  constructor() {
    super();
    /** @type {Map<string, object>} roomId → room object */
    this.rooms = new Map();

    /** @type {Map<string, string>} userId → roomId (fast lookup) */
    this.userRoomMap = new Map();

    // Start idle room cleanup interval
    this._cleanupInterval = setInterval(
      () => this._idleCleanup(),
      60_000 // Check every 60 seconds
    );

    logger.info('[RoomManager] Initialized');
  }

  // ---------------------------------------------------------------------------
  // Create
  // ---------------------------------------------------------------------------

  /**
   * Create a new room. The creator becomes the Host.
   *
   * @param {object} userInfo  — { userId, displayName, isGuest }
   * @param {object} settings  — optional partial settings override
   * @returns {{ room: object } | { error: string }}
   */
  createRoom(userInfo, settings = {}) {
    // Enforce MAX_ROOMS cap
    if (this.rooms.size >= config.MAX_ROOMS) {
      return { error: 'Số phòng đã đạt giới hạn. Vui lòng thử lại sau.' };
    }

    // User can only be in one room at a time
    if (this.userRoomMap.has(userInfo.userId)) {
      return { error: 'Bạn đang ở trong một phòng khác.' };
    }

    const roomId = this._generateRoomId();
    const roomName = settings.roomName ? settings.roomName.slice(0, 30) : `Phòng của ${userInfo.displayName}`;
    const validatedSettings = this._validateSettings(settings);

    const room = {
      roomId,
      roomName,
      host: userInfo.userId,
      users: new Map(),
      joinOrder: [],                    // For host transfer queue
      settings: validatedSettings,
      state: 'idle',                    // idle | playing | interrupted
      gameState: null,                  // Set in Phase 4 when game starts
      scoreTable: {},                   // Per-room cumulative scores
      lastActivity: Date.now(),
      createdAt: new Date().toISOString(),
    };

    // Add creator as first user (slot: null = spectator for now)
    const userEntry = {
      userId: userInfo.userId,
      displayName: userInfo.displayName,
      isGuest: userInfo.isGuest,
      slot: null,     // null = guest/spectator, 1 or 2 = player slot
      ready: false,
    };

    room.users.set(userInfo.userId, userEntry);
    room.joinOrder.push(userInfo.userId);

    this.rooms.set(roomId, room);
    this.userRoomMap.set(userInfo.userId, roomId);

    logger.info(`[RoomManager] Room ${roomId} created by ${userInfo.displayName}`);
    return { room };
  }

  // ---------------------------------------------------------------------------
  // Join
  // ---------------------------------------------------------------------------

  /**
   * Join an existing room.
   *
   * @param {object} userInfo  — { userId, displayName, isGuest }
   * @param {string} roomId
   * @returns {{ room: object } | { error: string }}
   */
  joinRoom(userInfo, roomId) {
    const room = this.rooms.get(roomId);
    if (!room) {
      return { error: 'Phòng không tồn tại.' };
    }

    // Already in this room? (reconnect scenario)
    if (room.users.has(userInfo.userId)) {
      room.lastActivity = Date.now();
      return { room };
    }

    // Already in another room?
    if (this.userRoomMap.has(userInfo.userId)) {
      return { error: 'Bạn đang ở trong một phòng khác.' };
    }

    // Enforce per-room user cap
    if (room.users.size >= config.MAX_USERS_PER_ROOM) {
      return { error: 'Phòng đã đầy.' };
    }

    const userEntry = {
      userId: userInfo.userId,
      displayName: userInfo.displayName,
      isGuest: userInfo.isGuest,
      slot: null,
      ready: false,
    };

    room.users.set(userInfo.userId, userEntry);
    room.joinOrder.push(userInfo.userId);
    room.lastActivity = Date.now();

    this.userRoomMap.set(userInfo.userId, roomId);

    logger.info(`[RoomManager] ${userInfo.displayName} joined room ${roomId}`);
    return { room };
  }

  // ---------------------------------------------------------------------------
  // Leave
  // ---------------------------------------------------------------------------

  /**
   * Remove a user from their room. Handles:
   *   - Host transfer (queue-based)
   *   - Room destruction when last user leaves
   *
   * @param {string} userId
   * @returns {{ room: object|null, destroyed: boolean, hostTransferred: boolean }}
   */
  leaveRoom(userId) {
    const roomId = this.userRoomMap.get(userId);
    if (!roomId) {
      return { room: null, destroyed: false, hostTransferred: false };
    }

    const room = this.rooms.get(roomId);
    if (!room) {
      this.userRoomMap.delete(userId);
      return { room: null, destroyed: false, hostTransferred: false };
    }

    // Remove user
    room.users.delete(userId);
    room.joinOrder = room.joinOrder.filter(id => id !== userId);
    this.userRoomMap.delete(userId);
    room.lastActivity = Date.now();

    // If room is now empty → destroy
    if (room.users.size === 0) {
      this._destroyRoom(roomId);
      logger.info(`[RoomManager] Room ${roomId} destroyed (empty)`);
      return { room, destroyed: true, hostTransferred: false };
    }

    // Host transfer if needed
    let hostTransferred = false;
    if (room.host === userId) {
      // Promote next user in join-order queue
      const newHost = room.joinOrder[0];
      room.host = newHost;
      hostTransferred = true;

      const newHostUser = room.users.get(newHost);
      logger.info(`[RoomManager] Host transferred to ${newHostUser.displayName} in room ${roomId}`);
    }

    return { room, destroyed: false, hostTransferred };
  }

  // ---------------------------------------------------------------------------
  // Sit (occupy a player slot)
  // ---------------------------------------------------------------------------

  /**
   * User occupies a player slot (1 or 2). Only valid when room is NOT playing.
   *
   * @param {string} userId
   * @param {number} slot — 1 or 2
   * @returns {{ room: object } | { error: string }}
   */
  sitDown(userId, slot) {
    const room = this._getUserRoom(userId);
    if (!room) return { error: 'Bạn chưa vào phòng nào.' };

    if (room.state === 'playing') {
      return { error: 'Không thể ngồi vào khi đang chơi.' };
    }

    if (slot !== 1 && slot !== 2) {
      return { error: 'Vị trí không hợp lệ.' };
    }

    const user = room.users.get(userId);

    // Already in this slot?
    if (user.slot === slot) return { room };

    // Check if slot is occupied by another user
    for (const [, u] of room.users) {
      if (u.slot === slot && u.userId !== userId) {
        return { error: 'Vị trí này đã có người.' };
      }
    }

    user.slot = slot;
    user.ready = false;
    room.lastActivity = Date.now();

    logger.info(`[RoomManager] ${user.displayName} sat in slot ${slot} in room ${room.roomId}`);
    return { room };
  }

  // ---------------------------------------------------------------------------
  // Stand (vacate player slot, become guest)
  // ---------------------------------------------------------------------------

  /**
   * User vacates their player slot. Only valid when NOT playing.
   *
   * @param {string} userId
   * @returns {{ room: object } | { error: string }}
   */
  standUp(userId) {
    const room = this._getUserRoom(userId);
    if (!room) return { error: 'Bạn chưa vào phòng nào.' };

    if (room.state === 'playing') {
      return { error: 'Không thể đứng dậy khi đang chơi.' };
    }

    const user = room.users.get(userId);
    if (user.slot === null) {
      return { error: 'Bạn chưa ngồi vào chỗ nào.' };
    }

    user.slot = null;
    user.ready = false;
    room.lastActivity = Date.now();

    logger.info(`[RoomManager] ${user.displayName} stood up in room ${room.roomId}`);
    return { room };
  }

  // ---------------------------------------------------------------------------
  // Update Settings (Host only)
  // ---------------------------------------------------------------------------

  /**
   * Update room settings. Only the Host can do this, and only when NOT playing.
   *
   * @param {string} userId
   * @param {object} newSettings — partial settings object
   * @returns {{ room: object } | { error: string }}
   */
  updateSettings(userId, newSettings) {
    const room = this._getUserRoom(userId);
    if (!room) return { error: 'Bạn chưa vào phòng nào.' };

    if (room.host !== userId) {
      return { error: 'Chỉ chủ phòng mới có thể thay đổi cài đặt.' };
    }

    if (room.state === 'playing') {
      return { error: 'Không thể thay đổi cài đặt khi đang chơi.' };
    }

    // Merge new settings with validation
    const merged = this._validateSettings({ ...room.settings, ...newSettings });
    room.settings = merged;
    room.lastActivity = Date.now();

    // Reset ready status for all seated players when settings change
    for (const [, u] of room.users) {
      if (u.slot !== null) u.ready = false;
    }

    logger.info(`[RoomManager] Settings updated in room ${room.roomId}`);
    return { room };
  }

  // ---------------------------------------------------------------------------
  // Toggle Ready
  // ---------------------------------------------------------------------------

  /**
   * Toggle a player's ready status. Only seated players can ready up.
   *
   * @param {string} userId
   * @returns {{ room: object, allReady: boolean } | { error: string }}
   */
  toggleReady(userId) {
    const room = this._getUserRoom(userId);
    if (!room) return { error: 'Bạn chưa vào phòng nào.' };

    if (room.state === 'playing') {
      return { error: 'Ván đang diễn ra.' };
    }

    const user = room.users.get(userId);
    if (user.slot === null) {
      return { error: 'Bạn cần ngồi vào chỗ trước khi sẵn sàng.' };
    }

    user.ready = !user.ready;
    room.lastActivity = Date.now();

    // Check if both slots are filled and both ready
    const allReady = this._areAllPlayersReady(room);

    return { room, allReady };
  }

  // ---------------------------------------------------------------------------
  // Kick User (Host only)
  // ---------------------------------------------------------------------------

  /**
   * Host kicks a user from the room.
   *
   * @param {string} hostId — userId of the host performing the kick
   * @param {string} targetId — userId of the user to kick
   * @returns {{ room: object, kicked: boolean } | { error: string }}
   */
  kickUser(hostId, targetId) {
    const room = this._getUserRoom(hostId);
    if (!room) return { error: 'Bạn chưa vào phòng nào.' };

    if (room.host !== hostId) {
      return { error: 'Chỉ chủ phòng mới có thể mời người ra.' };
    }

    if (hostId === targetId) {
      return { error: 'Bạn không thể mời chính mình ra.' };
    }

    if (!room.users.has(targetId)) {
      return { error: 'Người dùng không có trong phòng.' };
    }

    if (room.state === 'playing') {
      return { error: 'Không thể mời người ra khi đang chơi.' };
    }

    // Remove the user
    room.users.delete(targetId);
    room.joinOrder = room.joinOrder.filter(id => id !== targetId);
    this.userRoomMap.delete(targetId);
    room.lastActivity = Date.now();

    return { room, kicked: true };
  }

  // ---------------------------------------------------------------------------
  // Role helpers
  // ---------------------------------------------------------------------------

  /**
   * Determine a user's role in a room.
   * Role is computed from state, not stored — avoids sync issues.
   *
   * @param {object} room
   * @param {string} userId
   * @returns {'host'|'player'|'guest'|null}
   */
  getUserRole(room, userId) {
    if (!room.users.has(userId)) return null;
    if (room.host === userId) return 'host';
    const user = room.users.get(userId);
    if (user.slot !== null) return 'player';
    return 'guest';
  }

  /** Check if both player slots are filled and both players are ready. */
  _areAllPlayersReady(room) {
    let slot1 = null, slot2 = null;
    for (const [, u] of room.users) {
      if (u.slot === 1) slot1 = u;
      if (u.slot === 2) slot2 = u;
    }
    return slot1 && slot2 && slot1.ready && slot2.ready;
  }

  /** Helper: get the room a user is in, or null. */
  _getUserRoom(userId) {
    const roomId = this.userRoomMap.get(userId);
    return roomId ? this.rooms.get(roomId) : null;
  }

  // ---------------------------------------------------------------------------
  // List
  // ---------------------------------------------------------------------------

  /**
   * Returns a summary array of all rooms for the lobby display.
   * Each entry includes: roomId, hostName, playerCount, userCount, state, rules.
   *
   * @returns {Array<object>}
   */
  listRooms() {
    const list = [];
    for (const [, room] of this.rooms) {
      const hostUser = room.users.get(room.host);
      const playerCount = this._countPlayers(room);

      list.push({
        roomId: room.roomId,
        roomName: room.roomName,
        hostName: hostUser ? hostUser.displayName : '—',
        playerCount,                     // 0, 1, or 2 seated players
        userCount: room.users.size,      // Total people in room
        state: room.state,
        boardSize: room.settings.boardSize,
        ruleWall: room.settings.ruleWall,
        rulePortal: room.settings.rulePortal,
        timerMode: room.settings.timerMode,
        timerSeconds: room.settings.timerSeconds,
      });
    }
    return list;
  }

  // ---------------------------------------------------------------------------
  // Getters
  // ---------------------------------------------------------------------------

  /** Get a room by ID. */
  getRoom(roomId) {
    return this.rooms.get(roomId) || null;
  }

  /** Get the room a user is currently in. */
  getRoomByUser(userId) {
    const roomId = this.userRoomMap.get(userId);
    return roomId ? this.rooms.get(roomId) : null;
  }

  /** Get the roomId a user is currently in. */
  getRoomIdByUser(userId) {
    return this.userRoomMap.get(userId) || null;
  }

  // ---------------------------------------------------------------------------
  // Serialization helpers (for socket emit)
  // ---------------------------------------------------------------------------

  /**
   * Serialize a room for the `room:joined` / `room:updated` payload.
   * Converts Map to arrays for JSON transport.
   */
  serializeRoom(room) {
    const users = [];
    for (const [, u] of room.users) {
      users.push({
        userId: u.userId,
        displayName: u.displayName,
        isGuest: u.isGuest,
        slot: u.slot,
        ready: u.ready,
        role: this.getUserRole(room, u.userId),
      });
    }

    const hostUser = room.users.get(room.host);
    return {
      roomId: room.roomId,
      roomName: room.roomName,
      hostId: room.host,
      hostName: hostUser ? hostUser.displayName : '—',
      users,
      state: room.state,
      settings: { ...room.settings },
      scoreTable: { ...room.scoreTable },
    };
  }

  // ---------------------------------------------------------------------------
  // Internal helpers
  // ---------------------------------------------------------------------------

  /** Count the number of seated players (slot !== null). */
  _countPlayers(room) {
    let count = 0;
    for (const [, u] of room.users) {
      if (u.slot === 1 || u.slot === 2) count++;
    }
    return count;
  }

  /** Generate a short, human-friendly room ID like "#A3F". */
  _generateRoomId() {
    // 3-char hex — 4096 possible values, enough for 20 concurrent rooms
    const chars = 'ABCDEFGHJKLMNPQRSTUVWXYZ23456789'; // No I,O,0,1 to avoid ambiguity
    let id;
    let attempts = 0;
    do {
      id = '#' + chars[Math.floor(Math.random() * chars.length)]
              + chars[Math.floor(Math.random() * chars.length)]
              + chars[Math.floor(Math.random() * chars.length)];
      attempts++;
    } while (this.rooms.has(id) && attempts < 100);
    return id;
  }

  /** Validate and merge room settings with defaults. */
  _validateSettings(settings) {
    const s = {};

    s.boardSize = config.VALID_BOARD_SIZES.includes(settings.boardSize)
      ? settings.boardSize
      : config.DEFAULT_BOARD_SIZE;

    s.ruleWall   = settings.ruleWall === true;
    s.rulePortal = settings.rulePortal === true;

    s.timerMode = (settings.timerMode === 'per_move' || settings.timerMode === 'per_game')
      ? settings.timerMode
      : config.DEFAULT_TIMER_MODE;

    s.timerSeconds = (typeof settings.timerSeconds === 'number'
                      && settings.timerSeconds >= 5
                      && settings.timerSeconds <= 3600)
      ? Math.floor(settings.timerSeconds)
      : config.DEFAULT_TIMER_SECONDS;

    return s;
  }

  /** Destroy a room: clear references. Timers/listeners cleared by callers. */
  _destroyRoom(roomId) {
    const room = this.rooms.get(roomId);
    if (!room) return;

    // Remove all user→room mappings
    for (const [userId] of room.users) {
      this.userRoomMap.delete(userId);
    }

    // Null out internal data structures
    room.users.clear();
    room.joinOrder = [];
    room.gameState = null;
    room.scoreTable = null;

    this.rooms.delete(roomId);
    this.emit('room_destroyed', roomId);
  }

  /** Periodic cleanup: destroy rooms with no activity for IDLE_TIMEOUT_MS. */
  _idleCleanup() {
    const now = Date.now();
    for (const [roomId, room] of this.rooms) {
      // Never destroy rooms with an active or interrupted game
      if (room.state === 'playing' || room.state === 'interrupted') continue;
      if (now - room.lastActivity >= config.IDLE_TIMEOUT_MS) {
        logger.info(`[RoomManager] Idle cleanup: destroying room ${roomId} (idle ${Math.round((now - room.lastActivity) / 1000)}s)`);
        this._destroyRoom(roomId);
      }
    }
  }

  /** Shutdown: clear the cleanup interval (for clean process exit). */
  shutdown() {
    if (this._cleanupInterval) {
      clearInterval(this._cleanupInterval);
      this._cleanupInterval = null;
    }
  }
}

// Export singleton instance — RoomManager is the single source of truth
module.exports = new RoomManager();
