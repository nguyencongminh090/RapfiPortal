'use strict';

/**
 * logger.js — Simple leveled logger utility.
 *
 * Replaces console.log everywhere in the codebase.
 * Levels: info, warn, error.
 * Format: [LEVEL] [HH:MM:SS] message
 */

function timestamp() {
  return new Date().toTimeString().slice(0, 8);
}

const logger = {
  info(...args) {
    console.info(`[INFO ] [${timestamp()}]`, ...args);
  },
  warn(...args) {
    console.warn(`[WARN ] [${timestamp()}]`, ...args);
  },
  error(...args) {
    console.error(`[ERROR] [${timestamp()}]`, ...args);
  },
};

module.exports = logger;
