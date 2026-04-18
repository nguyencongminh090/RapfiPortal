'use strict';

// =============================================================================
// GomokuVN — Server Configuration
// All constants are defined here. Never use magic numbers elsewhere.
// =============================================================================

// --- Room limits ---
const MAX_ROOMS             = 20;
const MAX_USERS_PER_ROOM    = 10;     // 2 players + 8 guests
const IDLE_TIMEOUT_MS       = 600_000; // 10 minutes

// --- Disconnect grace ---
const DISCONNECT_GRACE_MS   = 60_000; // 60 seconds

// --- Game / Board ---
const DEFAULT_BOARD_SIZE    = 17;
const VALID_BOARD_SIZES     = [17, 19, 20];
const DEFAULT_TIMER_MODE    = 'per_move';
const DEFAULT_TIMER_SECONDS = 60;
const TIME_REQUEST_BONUS    = 10;  // Seconds added per "Xin Time" request
const TIME_REQUEST_MAX      = 2;   // Max "Xin Time" uses per player per game

// --- WALL mechanic ---
const WALL_COUNT            = 3;
const WALL_EDGE_MIN_DIST    = 3;   // Min distance from any board edge
const WALL_CENTER_ZONE      = 1;   // Center ±1 cells is forbidden (3x3 zone)
const WALL_MIN_CHEBYSHEV    = 4;   // Chebyshev dist between any 2 walls must be > 3 (so >= 4)
const WALL_RETRY_LIMIT      = 100;

// --- PORTAL mechanic ---
const PORTAL_PAIR_COUNT     = 2;
const PORTAL_MIN_CHEBYSHEV  = 5;   // Min Chebyshev dist between any 2 portal cells
const PORTAL_EDGE_MIN_DIST  = 3;   // Min distance from any board edge
const PORTAL_RETRY_LIMIT    = 100;

// --- Chat rate limiting ---
const CHAT_RATE_LIMIT       = 5;    // Max messages per window
const CHAT_RATE_WINDOW_MS   = 3000; // Window duration

// --- Authentication ---
const JWT_SECRET  = process.env.JWT_SECRET || 'gomokuvn-dev-secret-change-in-production';
const JWT_EXPIRY  = '7d';
const JWT_GUEST_EXPIRY = '24h';
const BCRYPT_ROUNDS = 12;

// --- Guest name generation ---
// All words 3-5 letters so combined name is 4-8 letters total.
const GUEST_NAME_ADJECTIVES = [
  'Red', 'Blue', 'Bold', 'Dark', 'Fast', 'Free', 'Grim', 'Iron',
  'Jade', 'Kind', 'Lone', 'Neon', 'Pale', 'Pure', 'Sage', 'Slim',
  'Soft', 'Teal', 'True', 'Wild',
];
const GUEST_NAME_NOUNS = [
  'Bear', 'Bird', 'Buck', 'Bull', 'Colt', 'Crow', 'Deer', 'Dove',
  'Duck', 'Elk',  'Fish', 'Flea', 'Fly',  'Fox',  'Gnu',  'Hawk',
  'Ibis', 'Kite', 'Lamb', 'Lark', 'Lion', 'Lynx', 'Mink', 'Mole',
  'Moth', 'Mule', 'Newt', 'Puma', 'Rook', 'Slug', 'Swan', 'Toad',
  'Vole', 'Wolf', 'Wren', 'Yak',
];

// --- HTTP ---
const HTTP_PORT = process.env.PORT || 3000;

// --- Security ---
const MAX_EVENTS_PER_SECOND = 50; // Socket flood protection

module.exports = {
  MAX_ROOMS,
  MAX_USERS_PER_ROOM,
  IDLE_TIMEOUT_MS,
  DISCONNECT_GRACE_MS,
  DEFAULT_BOARD_SIZE,
  VALID_BOARD_SIZES,
  DEFAULT_TIMER_MODE,
  DEFAULT_TIMER_SECONDS,
  TIME_REQUEST_BONUS,
  TIME_REQUEST_MAX,
  WALL_COUNT,
  WALL_EDGE_MIN_DIST,
  WALL_CENTER_ZONE,
  WALL_MIN_CHEBYSHEV,
  WALL_RETRY_LIMIT,
  PORTAL_PAIR_COUNT,
  PORTAL_MIN_CHEBYSHEV,
  PORTAL_EDGE_MIN_DIST,
  PORTAL_RETRY_LIMIT,
  CHAT_RATE_LIMIT,
  CHAT_RATE_WINDOW_MS,
  JWT_SECRET,
  JWT_EXPIRY,
  JWT_GUEST_EXPIRY,
  BCRYPT_ROUNDS,
  GUEST_NAME_ADJECTIVES,
  GUEST_NAME_NOUNS,
  HTTP_PORT,
  MAX_EVENTS_PER_SECOND,
};
