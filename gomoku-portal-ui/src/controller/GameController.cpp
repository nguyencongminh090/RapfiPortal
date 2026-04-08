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

    board_.reset(boardSize);
    signalBoardChanged.emit();

    // If engine connected, start a new game in the engine too
    if (engine_.state() != engine::EngineState::Disconnected) {
        engine::EngineConfig cfg;
        cfg.boardSize = boardSize;
        cfg.timeoutTurn = turnTimeMs_;
        cfg.timeoutMatch = matchTimeMs_;
        cfg.maxMemoryBytes = maxMemory_;
        engine_.startGame(cfg);
    }
}

void GameController::setGameMode(GameMode mode, HumanSide side) {
    mode_ = mode;
    humanSide_ = side;
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
                    requestEngineMove();
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

    if (mode_ == GameMode::HumanVsEngine && board_.ply() >= 2) {
        // Undo both engine's move and human's move
        auto move2 = board_.undoLast();
        auto move1 = board_.undoLast();

        // Tell engine to take back both
        if (engine_.state() == engine::EngineState::Idle) {
            if (!move2.isPass()) engine_.takeBack(move2.coord.x, move2.coord.y);
            if (!move1.isPass()) engine_.takeBack(move1.coord.x, move1.coord.y);
        }
    } else {
        // FreePlay or only 1 move: undo one
        auto move = board_.undoLast();
        if (engine_.state() == engine::EngineState::Idle && !move.isPass()) {
            engine_.takeBack(move.coord.x, move.coord.y);
        }
    }

    signalBoardChanged.emit();
}

void GameController::passMove() {
    board_.pass(board_.sideToMove());
    signalBoardChanged.emit();
}

// =============================================================================
// Engine Lifecycle
// =============================================================================

void GameController::connectEngine(const std::string& enginePath) {
    engine::EngineConfig cfg;
    cfg.executablePath = enginePath;
    cfg.boardSize = board_.size();
    cfg.timeoutTurn = turnTimeMs_;
    cfg.timeoutMatch = matchTimeMs_;
    cfg.maxMemoryBytes = maxMemory_;

    if (engine_.connect(cfg)) {
        // Start a game in the engine with the current board state
        engine_.startGame(cfg);
        syncBoardToEngine();
    }
}

void GameController::disconnectEngine() {
    engine_.disconnect();
}

void GameController::startThinking() {
    if (engine_.state() == engine::EngineState::Idle) {
        syncBoardToEngine();
        engine_.requestNBest(nbest_);
        // Use BOARD to load position and start thinking
        auto record = model::GameRecord::fromBoard(board_);
        auto entries = record.toBoardEntries(
            board_.sideToMove() == model::Color::Black
                ? model::Color::Black : model::Color::White);
        engine_.loadPosition(entries);
    }
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

void GameController::requestEngineMove() {
    if (engine_.state() != engine::EngineState::Idle) return;

    // Load position via BOARD and let engine think
    auto record = model::GameRecord::fromBoard(board_);
    auto entries = record.toBoardEntries(
        board_.sideToMove() == model::Color::Black
            ? model::Color::Black : model::Color::White);
    engine_.loadPosition(entries);
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
