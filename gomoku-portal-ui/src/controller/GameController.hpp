/*
 *  Portal Gomoku UI — Game Controller
 *  Central Façade connecting engine, board model, and UI signals.
 *
 *  Responsibilities:
 *    - Owns Board model and EngineController
 *    - Routes user clicks → board mutations → engine commands
 *    - Routes engine responses → board updates → UI refresh
 *    - Manages game mode (FreePlay, HumanVsEngine)
 *    - Handles new game, undo, engine lifecycle
 */

#pragma once

#include "../model/Board.hpp"
#include "../model/AnalysisInfo.hpp"
#include "../engine/EngineController.hpp"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>

namespace controller {

/// Game mode determines who plays each side.
enum class GameMode {
    FreePlay,         ///< Both sides human (click-to-place)
    HumanVsEngine,    ///< Human plays one side, engine plays the other
};

/// Which side the human plays in HumanVsEngine mode.
enum class HumanSide {
    Black,
    White,
};

/// Central controller connecting all components.
class GameController {
public:
    GameController();
    ~GameController();

    // =========================================================================
    // Board Access (read-only for rendering)
    // =========================================================================

    [[nodiscard]] model::Board& board() { return board_; }
    [[nodiscard]] const model::Board& board() const { return board_; }
    [[nodiscard]] engine::EngineController& engine() { return engine_; }

    // =========================================================================
    // Game Flow
    // =========================================================================

    /// Start a new game with the given board size.
    void newGame(int boardSize = 15);

    /// Set the game mode.
    void setGameMode(GameMode mode, HumanSide side = HumanSide::Black);

    /// Handle a user click on a board cell.
    /// In FreePlay: places a stone for the current side.
    /// In HumanVsEngine: places if it's the human's turn, then asks engine.
    void onCellClicked(int x, int y);

    /// Called when the user wants to undo.
    void undoMove();

    /// Called when the user wants to redo.
    void redoMove();

    /// Called when the user passes their turn.
    void passMove();

    /// Load a full game from a plain move list.
    /// Atomically replaces board state, clears redo stack,
    /// syncs the engine (YXBOARD), and emits signalBoardChanged.
    /// @param boardSize  Size of the board for this game.
    /// @param moves      Ordered list of (x, y) pairs; (-1,-1) = pass.
    void loadGameFromMoves(int boardSize, const std::vector<std::pair<int,int>>& moves);

    // =========================================================================
    // Engine Lifecycle
    // =========================================================================

    /// Connect to an engine binary at the given path.
    void connectEngine(const std::string& enginePath);

    /// Disconnect the engine.
    void disconnectEngine();

    /// Ask the engine to think about the current position.
    void startThinking();

    /// Stop the engine's current search.
    void stopThinking();

    /// Send the current board position to the engine via BOARD command.
    void syncBoardToEngine();

    // =========================================================================
    // Engine Configuration
    // =========================================================================

    /// Set the engine's thinking time in milliseconds.
    void setTurnTime(int ms);

    /// Set the engine's match time in milliseconds.
    void setMatchTime(int ms);

    /// Set the maximum memory in bytes.
    void setMaxMemory(int64_t bytes);

    /// Set the number of candidate moves (NBEST).
    void setNBest(int n);
    
    /// Set the game rule.
    void setRule(int rule);

    /// Set thread count.
    void setThreadNum(int n);

    /// Enable/disable pondering.
    void setPondering(bool enable);

    /// Set maximum search depth.
    void setMaxDepth(int depth);

    /// Set maximum nodes.
    void setMaxNodes(unsigned long long n);

    /// Maintenance: clear hash.
    void clearHash();

    /// Maintenance: reload config.
    void reloadConfig();

    /// Maintenance: show hash usage.
    void showHashUsage();

    /// Mark topology as dirty (needs re-sync to engine before next think).
    /// Called by SetupController after topology is modified.
    void markTopologyDirty() { topologyDirty_ = true; }

    // =========================================================================
    // Polling (call from GTK main loop timer)
    // =========================================================================

    /// Drain engine output and dispatch signals. Returns true if there was output.
    bool pollEngine();

    // =========================================================================
    // Signals (connect from MainWindow)
    // =========================================================================

    /// Board state changed — canvas should refresh.
    sigc::signal<void()> signalBoardChanged;

    /// Engine state changed (connected, thinking, idle, etc.).
    sigc::signal<void(engine::EngineState)> signalEngineStateChanged;

    /// Engine sent a MESSAGE line (for log panel).
    sigc::signal<void(const std::string&)> signalEngineMessage;

    /// Engine sent analysis info line (for analysis panel).
    sigc::signal<void(const model::AnalysisInfo&)> signalAnalysisInfo;

    /// Raw communication line (isSend, text)
    sigc::signal<void(bool, const std::string&)> signalRawComm;

    /// Engine identified itself.
    sigc::signal<void(const std::string&)> signalEngineName;

private:
    model::Board board_{15};
    engine::EngineController engine_;
    GameMode mode_ = GameMode::FreePlay;
    HumanSide humanSide_ = HumanSide::Black;
    int     turnTimeMs_    = 5000;
    int     matchTimeMs_   = 0;
    int64_t maxMemory_     = 350 * 1024 * 1024;  // 350MB default
    int     nbest_         = 1;
    int     rule_          = 0;
    int     threadNum_     = 0;
    bool    pondering_     = false;
    int     maxDepth_      = 99;
    unsigned long long maxNodes_ = 0;
    bool    topologyDirty_ = false;  // BUG-005: true when topology changed since last sync
    // NOTE: nbest_ is sent every startThinking() call via requestNBest() —
    // no dirty flag needed because YXNBEST starts the thinking session itself.

    // Signal connections for cleanup
    std::vector<sigc::connection> connections_;

    /// Check if the engine should move now (in HvE mode).
    bool isEngineTurn() const;

    /// Ask engine to play the current position after a human move.
    void requestEngineMoveAfterHuman(int humanX, int humanY);

    /// Ask engine to play the current position from scratch (BEGIN or BOARD).
    void requestEngineMoveFromScratch();

    // Engine signal handlers
    void onEngineMove(int x, int y);
    void onEngineMessage(const std::string& msg);
    void onEngineStateChange(engine::EngineState state);
    void onEngineName(const std::string& name);
};

}  // namespace controller
