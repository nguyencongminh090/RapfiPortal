/*
 *  Portal Gomoku UI — Engine Controller
 *  High-level async engine interaction API with state machine enforcement.
 *  All public methods are called from the GTK main thread.
 *  Engine responses are delivered via sigc++ signals (also on main thread).
 */

#pragma once

#include "EngineConfig.hpp"
#include "EngineProcess.hpp"
#include "EngineProtocol.hpp"
#include "EngineState.hpp"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>
#include <tuple>

namespace engine {

/// High-level engine controller with state machine and signal-based output dispatch.
///
/// Lifecycle:
///   1. connect(config)     → spawns engine, sends YXSHOWINFO + config
///   2. startGame(...)      → sends START + portals + rule
///   3. makeMove() / requestEngineMove() / takeBack()
///   4. disconnect()        → sends END, kills process
///
/// State machine:
///   Disconnected → connect() → Idle
///   Idle → makeMove()/requestEngineMove() → Thinking
///   Thinking → engine replies move → Idle
///   Thinking → stopThinking() → Stopping → engine replies → Idle
///   Any → disconnect() → Disconnected
class EngineController {
public:
    EngineController();
    ~EngineController();

    // =========================================================================
    // Lifecycle
    // =========================================================================

    /// Spawn engine process and initialize GUI mode.
    /// Returns true if engine launched successfully.
    bool connect(const EngineConfig& config);

    /// Send END and terminate engine process.
    void disconnect();

    /// Current engine state.
    [[nodiscard]] EngineState state() const { return state_; }

    // =========================================================================
    // Game Commands (valid in IDLE state only)
    // =========================================================================

    /// Initialize a new game: sends START + CLEARPORTALS + portals + walls + rule + config.
    void startGame(const EngineConfig& config,
                   const std::vector<std::pair<int,int>>& walls = {},
                   const std::vector<std::tuple<int,int,int,int>>& portals = {});

    /// Restart current game (keeps topology). Sends RESTART.
    void restartGame();

    /// Send TURN x,y — human plays at (x,y), engine will think and reply.
    void makeMove(int x, int y);

    /// Send BEGIN — engine plays first move.
    void requestEngineMove();

    /// Send TAKEBACK x,y — undo one ply.
    void takeBack(int x, int y);

    /// Send BOARD block to load a position. Engine starts thinking.
    void loadPosition(const std::vector<std::tuple<int,int,int>>& entries);

    /// Send YXBOARD block to load a position silently (no thinking).
    void loadPositionSilent(const std::vector<std::tuple<int,int,int>>& entries);

    // =========================================================================
    // Portal Setup (valid in IDLE state only)
    // =========================================================================

    /// Send INFO WALL x,y
    void addWall(int x, int y);

    /// Send INFO YXPORTAL ax,ay bx,by
    void addPortal(int ax, int ay, int bx, int by);

    /// Send INFO CLEARPORTALS
    void clearPortals();

    // =========================================================================
    // Configuration
    // =========================================================================

    /// Send all INFO parameters from config.
    void applyConfig(const EngineConfig& config);

    // =========================================================================
    // Analysis
    // =========================================================================

    /// Send YXNBEST n
    void requestNBest(int n);

    /// Send STOP to interrupt thinking (valid in THINKING state).
    void stopThinking();

    /// Send TRACEBOARD
    void traceBoard();

    /// Send TRACESEARCH
    void traceSearch();

    // =========================================================================
    // Hash & Database
    // =========================================================================

    void hashClear();
    void hashDump(const std::string& path);
    void hashLoad(const std::string& path);
    void setDatabase(const std::string& path);
    void saveDatabase();
    void queryDatabaseAll();
    void queryDatabaseOne();

    // =========================================================================
    // Signals (emitted on GTK main thread via drainOutput)
    // =========================================================================

    /// Engine played a move at (x, y).
    sigc::signal<void(int, int)>             signalEngineMove;

    /// Engine played a double move (Swap2/Balance).
    sigc::signal<void(int, int, int, int)>   signalEngineMove2;

    /// Engine responded with SWAP.
    sigc::signal<void()>                     signalSwap;

    /// Engine acknowledged a command with OK.
    sigc::signal<void()>                     signalOk;

    /// MESSAGE line from engine.
    sigc::signal<void(const std::string&)>   signalMessage;

    /// ERROR line from engine.
    sigc::signal<void(const std::string&)>   signalError;

    /// ABOUT info response.
    sigc::signal<void(const std::string&)>   signalAboutInfo;

    /// Forbidden points list (Renju).
    sigc::signal<void(const std::vector<std::pair<int,int>>&)>  signalForbid;

    /// Database query result line.
    sigc::signal<void(const std::string&)>   signalDatabaseLine;

    /// Database query finished.
    sigc::signal<void()>                     signalDatabaseDone;

    /// Engine state changed.
    sigc::signal<void(EngineState)>          signalStateChanged;

    // =========================================================================
    // Main thread polling (call from Glib::signal_idle or Glib::signal_timeout)
    // =========================================================================

    /// Process pending engine output. Returns number of lines processed.
    /// Must be called periodically from the GTK main thread.
    int pollOutput();

private:
    EngineProcess process_;
    EngineState   state_ = EngineState::Disconnected;
    EngineConfig  currentConfig_;

    /// Switch to a new state and emit signalStateChanged.
    void setState(EngineState newState);

    /// Guard: throw runtime_error if not in expected state.
    void requireState(EngineState expected, const char* action) const;

    /// Guard: throw runtime_error if not idle.
    void requireIdle(const char* action) const;

    /// Send a raw command string to the engine.
    void send(const std::string& cmd);

    /// Called for each parsed line (from main thread via pollOutput).
    void onLineReceived(const std::string& line);
};

}  // namespace engine
