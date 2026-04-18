'use strict';

/**
 * TimerManager.js — Server-side game timer.
 *
 * Supports two modes:
 *   - per_move: each player gets timerSeconds per move (resets on move)
 *   - per_game: each player gets timerSeconds total for the entire game
 *
 * Timer ticks every 1 second via setInterval.
 * On timeout, calls the provided onTimeout callback.
 *
 * Manual test checklist:
 *   [ ] Timer starts and ticks every second
 *   [ ] per_move: resets active player's timer on switchTurn
 *   [ ] per_game: decrements active player, no reset
 *   [ ] Timeout triggers callback with correct playerId
 *   [ ] stop() clears interval and prevents further ticks
 *   [ ] Multiple start/stop cycles work without leaks
 */

const logger = require('../utils/logger');

class TimerManager {
  /**
   * @param {object} opts
   * @param {string} opts.roomId — for logging
   * @param {string} opts.mode — 'per_move' | 'per_game'
   * @param {number} opts.seconds — initial seconds per player
   * @param {string} opts.blackPlayerId
   * @param {string} opts.whitePlayerId
   * @param {function(io, roomId)} opts.onTick — called every second with current times
   * @param {function(string)} opts.onTimeout — called with the playerId who timed out
   */
  constructor(opts) {
    this.roomId         = opts.roomId;
    this.mode           = opts.mode;
    this.initialSeconds = opts.seconds;

    this.blackPlayerId  = opts.blackPlayerId;
    this.whitePlayerId  = opts.whitePlayerId;

    this.onTick    = opts.onTick    || (() => {});
    this.onTimeout = opts.onTimeout || (() => {});

    // Timer state: remaining seconds for each player
    this.black = opts.seconds;
    this.white = opts.seconds;

    // Who is currently ticking down
    this.activeColor = 'black'; // 'black' starts first

    this._interval = null;
  }

  /** Start the timer. */
  start() {
    if (this._interval) return; // Already running

    this._interval = setInterval(() => {
      this._tick();
    }, 1000);

    logger.info(`[Timer] Started for room ${this.roomId} (${this.mode}, ${this.initialSeconds}s)`);
  }

  /** Stop the timer and clear the interval. */
  stop() {
    if (this._interval) {
      clearInterval(this._interval);
      this._interval = null;
    }
  }

  /**
   * Switch turn and handle timer logic.
   * Called after a valid move.
   *
   * @param {'black'|'white'} newActiveColor — the color whose turn it now is
   */
  switchTurn(newActiveColor) {
    // In per_move mode, reset the player who just moved
    if (this.mode === 'per_move') {
      // The player who just moved is the opposite of newActiveColor
      if (newActiveColor === 'white') {
        this.black = this.initialSeconds; // Black just moved, reset
      } else {
        this.white = this.initialSeconds; // White just moved, reset
      }
    }

    this.activeColor = newActiveColor;
  }

  /** Get current timer values. */
  getTimers() {
    return { black: this.black, white: this.white };
  }

  /**
   * Add bonus time to a player.
   * Used for "Xin Time" feature.
   *
   * @param {'black'|'white'} color — which player to add time to
   * @param {number} seconds — seconds to add
   */
  addTime(color, seconds) {
    if (color === 'black') {
      this.black += seconds;
    } else {
      this.white += seconds;
    }
  }

  /** Internal tick — called every second. */
  _tick() {
    if (this.activeColor === 'black') {
      this.black--;
      if (this.black <= 0) {
        this.black = 0;
        this.stop();
        this.onTick(this.getTimers());
        this.onTimeout(this.blackPlayerId);
        return;
      }
    } else {
      this.white--;
      if (this.white <= 0) {
        this.white = 0;
        this.stop();
        this.onTick(this.getTimers());
        this.onTimeout(this.whitePlayerId);
        return;
      }
    }

    this.onTick(this.getTimers());
  }

  /** Destroy: stop timer and null all references. */
  destroy() {
    this.stop();
    this.onTick = null;
    this.onTimeout = null;
  }
}

module.exports = TimerManager;
