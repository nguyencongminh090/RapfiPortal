/*
 *  Portal Gomoku UI — Game Controller Implementation
 */

#include "GameController.hpp"
#include "../model/GameRecord.hpp"

#include <iostream>

namespace controller {

// =============================================================================
// Constructor / Destructor
// =============================================================================

GameController::GameController() {
    // Wire engine signals → our handlers
    connections_.push_back(
        engine_.signalEngineMove.connect(
            sigc::mem_fun(*this, &GameController::onEngineMove)));
    connections_.push_back(
        engine_.signalMessage.connect(
            sigc::mem_fun(*this, &GameController::onEngineMessage)));
    connections_.push_back(
        engine_.signalStateChanged.connect(
            sigc::mem_fun(*this, &GameController::onEngineStateChange)));
    connections_.push_back(
        engine_.signalAboutInfo.connect(
            sigc::mem_fun(*this, &GameController::onEngineName)));
}

GameController::~GameController() {
    for (auto& conn : connections_) {
        conn.disconnect();
    }
    if (engine_.state() != engine::EngineState::Disconnected) {
        engine_.disconnect();
    }
}

// =============================================================================
// Game Flow
// =============================================================================

void GameController::newGame(int boardSize) {
    // Stop engine if thinking
    if (engine_.state() == engine::EngineState::Thinking) {
        engine_.stopThinking();
    }

    model::PortalTopology savedTopo = board_.topology();
    int prevBoardSize = board_.size();

    board_.reset(boardSize);

    if (boardSize == prevBoardSize) {
        board_.setTopology(savedTopo);
    }
    signalBoardChanged.emit();

    // If engine connected, start a new game in the engine too
    if (engine_.state() != engine::EngineState::Disconnected) {
        engine::EngineConfig cfg;
        cfg.boardSize = boardSize;
        cfg.timeoutTurn = turnTimeMs_;
        cfg.timeoutMatch = matchTimeMs_;
        cfg.maxMemoryBytes = maxMemory_;

        engine_.startGame(cfg);
        syncBoardToEngine();
        topologyDirty_ = false;

        engine_.applyConfig(cfg);

        if (isEngineTurn()) {
            requestEngineMoveFromScratch();
        }
    }
}

void GameController::setGameMode(GameMode mode, HumanSide side) {
    mode_ = mode;
    humanSide_ = side;
    
    // If we switch to a mode where engine should move, trigger it
    if (isEngineTurn() && engine_.state() == engine::EngineState::Idle) {
        requestEngineMoveFromScratch();
    }
}

void GameController::onCellClicked(int x, int y) {
    // Ignore clicks when engine is thinking
    if (engine_.state() == engine::EngineState::Thinking ||
        engine_.state() == engine::EngineState::Stopping) {
        return;
    }

    // Can't place on non-empty cells
    if (!board_.isEmpty(x, y)) {
        return;
    }

    switch (mode_) {
    case GameMode::FreePlay:
        // Both sides human — just place the stone
        if (board_.placeStone(x, y, board_.sideToMove())) {
            signalBoardChanged.emit();
        }
        break;

    case GameMode::HumanVsEngine:
        // Only allow move if it's the human's turn
        if (!isEngineTurn()) {
            if (board_.placeStone(x, y, board_.sideToMove())) {
                signalBoardChanged.emit();

                // Now tell the engine about the move and ask it to respond
                if (engine_.state() == engine::EngineState::Idle) {
                    requestEngineMoveAfterHuman(x, y);
                }
            }
        }
        break;
    }
}

void GameController::undoMove() {
    if (engine_.state() == engine::EngineState::Thinking ||
        engine_.state() == engine::EngineState::Stopping) {
        return;
    }

    if (board_.ply() == 0) return;

    // BUG-010 FIX: helper that always sends TAKEBACK regardless of pass.
    // Protocol doc: TAKEBACK coordinates are ignored by the engine — it just decrements ply.
    // Sending TAKEBACK -1,-1 for pass moves keeps engine ply in sync with board ply.
    auto sendTakeBack = [this](const model::Move& m) {
        if (engine_.state() != engine::EngineState::Idle) return;
        // For pass moves use sentinel coords; engine ignores them anyway.
        int x = m.isPass() ? -1 : m.coord.x;
        int y = m.isPass() ? -1 : m.coord.y;
        engine_.takeBack(x, y);
    };

    if (mode_ == GameMode::HumanVsEngine && board_.ply() >= 2) {
        auto move2 = board_.undoLast();
        auto move1 = board_.undoLast();
        sendTakeBack(move2);
        sendTakeBack(move1);
    } else {
        auto move = board_.undoLast();
        sendTakeBack(move);
    }

    signalBoardChanged.emit();
}

void GameController::redoMove() {
    if (engine_.state() == engine::EngineState::Thinking ||
        engine_.state() == engine::EngineState::Stopping) {
        return;
    }

    if (!board_.canRedo()) return;

    if (mode_ == GameMode::HumanVsEngine) {
        // Try to redo 2 moves (human then engine) so it remains human's turn
        if (board_.redoMove()) {
            if (board_.canRedo()) {
                board_.redoMove();
            }
        }
        
        // Sync position to engine silently
        if (engine_.state() == engine::EngineState::Idle) {
            auto record = model::GameRecord::fromBoard(board_);
            auto entries = record.toBoardEntries(board_.sideToMove());
            engine_.loadPositionSilent(entries);
        }
    } else {
        // FreePlay mode redo 1 move
        board_.redoMove();
        if (engine_.state() == engine::EngineState::Idle) {
            auto record = model::GameRecord::fromBoard(board_);
            auto entries = record.toBoardEntries(board_.sideToMove());
            engine_.loadPositionSilent(entries);
        }
    }

    signalBoardChanged.emit();
}

void GameController::passMove() {
    board_.pass(board_.sideToMove());
    signalBoardChanged.emit();
}

void GameController::loadGameFromMoves(int boardSize,
                                       const std::vector<std::pair<int,int>>& moves)
{
    // Stop engine if currently thinking.
    if (engine_.state() == engine::EngineState::Thinking ||
        engine_.state() == engine::EngineState::Stopping) {
        engine_.stopThinking();
    }

    // Atomically reset board to the new size (clears history + redoStack + topology).
    board_.reset(boardSize);

    // Replay all moves directly on the board model.
    for (auto [x, y] : moves) {
        if (x == -1 && y == -1) {
            board_.pass(board_.sideToMove());
        } else {
            board_.placeStone(x, y, board_.sideToMove());
        }
    }

    // Sync the final position to the engine silently (no think).
    if (engine_.state() == engine::EngineState::Idle) {
        auto record  = model::GameRecord::fromBoard(board_);
        auto entries = record.toBoardEntries(board_.sideToMove());
        engine_.loadPositionSilent(entries);
    }

    // Notify UI once.
    signalBoardChanged.emit();
}

// =============================================================================
// Engine Lifecycle
// =============================================================================

void GameController::connectEngine(const std::string& enginePath) {
    engine::EngineConfig cfg;
    cfg.executablePath = enginePath;
    cfg.boardSize      = board_.size();
    cfg.timeoutTurn    = turnTimeMs_;
    cfg.timeoutMatch   = matchTimeMs_;
    cfg.maxMemoryBytes = maxMemory_;

    if (!engine_.connect(cfg)) return;

    engine_.startGame(cfg);

    // Sync topology (walls + portals) to the engine.
    syncBoardToEngine();

    // BUG-003 FIX: If a game is already in progress, sync the move history too.
    // Without this the engine starts from an empty board while the UI has pieces.
    if (board_.ply() > 0) {
        auto record  = model::GameRecord::fromBoard(board_);
        auto entries = record.toBoardEntries(board_.sideToMove());
        engine_.loadPositionSilent(entries);  // YXBOARD — no auto-think
    }
}

void GameController::disconnectEngine() {
    engine_.disconnect();
}

void GameController::startThinking() {
    if (engine_.state() != engine::EngineState::Idle) return;

    // Sync topology only when it has changed (BUG-005 fix — avoids N+1 applyAndReinit).
    if (topologyDirty_) {
        syncBoardToEngine();
        topologyDirty_ = false;
    }

    // Step 1: Load the current board position SILENTLY (YXBOARD).
    //   → engine processes the position but does NOT start thinking.
    //   → state stays Idle after this call.
    auto record  = model::GameRecord::fromBoard(board_);
    auto entries = record.toBoardEntries(board_.sideToMove());
    engine_.loadPositionSilent(entries);            // YXBOARD — state stays Idle ✓

    // Step 2: NOW start thinking with N-best output (YXNBEST n).
    //   → this requires Idle state (OK — we are still Idle after step 1).
    //   → sets state to Thinking after sending the command.
    engine_.requestNBest(nbest_);                   // YXNBEST — state → Thinking ✓
}

void GameController::stopThinking() {
    if (engine_.state() == engine::EngineState::Thinking) {
        engine_.stopThinking();
    }
}

void GameController::syncBoardToEngine() {
    if (engine_.state() == engine::EngineState::Disconnected) return;

    // Send topology
    engine_.clearPortals();
    for (const auto& wall : board_.topology().walls()) {
        engine_.addWall(wall.x, wall.y);
    }
    for (const auto& portal : board_.topology().portals()) {
        engine_.addPortal(portal.a.x, portal.a.y, portal.b.x, portal.b.y);
    }
}

// =============================================================================
// Engine Configuration
// =============================================================================

void GameController::setTurnTime(int ms)        { turnTimeMs_ = ms; }
void GameController::setMatchTime(int ms)       { matchTimeMs_ = ms; }
void GameController::setMaxMemory(int64_t bytes) { maxMemory_ = bytes; }
void GameController::setNBest(int n)            { nbest_ = n; }

// =============================================================================
// Polling
// =============================================================================

bool GameController::pollEngine() {
    if (engine_.state() == engine::EngineState::Disconnected) return false;
    engine_.checkStoppingTimeout();
    return engine_.pollOutput() > 0;
}

// =============================================================================
// Private Helpers
// =============================================================================

bool GameController::isEngineTurn() const {
    if (mode_ != GameMode::HumanVsEngine) return false;

    model::Color current = board_.sideToMove();
    if (humanSide_ == HumanSide::Black) {
        return current == model::Color::White;  // Engine plays white
    } else {
        return current == model::Color::Black;  // Engine plays black
    }
}

void GameController::requestEngineMoveAfterHuman(int humanX, int humanY) {
    if (engine_.state() != engine::EngineState::Idle) return;
    engine_.makeMove(humanX, humanY);  // sends "TURN x,y" → sets state Thinking
}

void GameController::requestEngineMoveFromScratch() {
    if (engine_.state() != engine::EngineState::Idle) return;
    if (board_.ply() == 0) {
        engine_.requestEngineMove();  // sends "BEGIN" → state Thinking
    } else {
        auto record  = model::GameRecord::fromBoard(board_);
        auto entries = record.toBoardEntries(board_.sideToMove());
        engine_.loadPosition(entries);   // BOARD → state Thinking
    }
}

// =============================================================================
// Engine Signal Handlers
// =============================================================================

void GameController::onEngineMove(int x, int y) {
    // Engine responded with a move — place it on our board
    model::Color engineColor = board_.sideToMove();
    if (board_.placeStone(x, y, engineColor)) {
        signalBoardChanged.emit();
    }
}

void GameController::onEngineMessage(const std::string& msg) {
    signalEngineMessage.emit(msg);

    // TODO: Parse analysis info from MESSAGE lines when we build AnalysisController
}

void GameController::onEngineStateChange(engine::EngineState state) {
    signalEngineStateChanged.emit(state);
}

void GameController::onEngineName(const std::string& name) {
    signalEngineName.emit(name);
}

}  // namespace controller
