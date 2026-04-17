#include "TournamentController.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>
#include <glibmm/main.h>

namespace controller {

TournamentController::TournamentController() {
    // Engine A signals
    connections_.push_back(
        engineA_.signalEngineMove.connect([this](int x, int y) {
            handleEngineMove(true, x, y);
        })
    );
    connections_.push_back(
        engineA_.signalError.connect([this](const std::string& err) {
            log("Engine A ERROR: " + err);
            endCurrentGame(2); // B wins by forfeit
        })
    );
    connections_.push_back(
        engineA_.signalRawComm.connect([this](bool isSend, const std::string& line) {
            signalRawComm.emit(isSend, "Engine A: " + line);
        })
    );
    // Engine B signals
    connections_.push_back(
        engineB_.signalEngineMove.connect([this](int x, int y) {
            handleEngineMove(false, x, y);
        })
    );
    connections_.push_back(
        engineB_.signalError.connect([this](const std::string& err) {
            log("Engine B ERROR: " + err);
            endCurrentGame(1); // A wins by forfeit
        })
    );
    connections_.push_back(
        engineB_.signalRawComm.connect([this](bool isSend, const std::string& line) {
            signalRawComm.emit(isSend, "Engine B: " + line);
        })
    );
}

TournamentController::~TournamentController() {
    stopTournament();
    for (auto& c : connections_) {
        c.disconnect();
    }
}

void TournamentController::log(const std::string& msg) {
    std::cout << "[Tournament] " << msg << std::endl;
    signalLogMessage.emit(msg);
}

void TournamentController::pollEngines() {
    engineA_.pollOutput();
    engineB_.pollOutput();
}

void TournamentController::stopTournament() {
    if (state_ == TournamentState::Idle || state_ == TournamentState::Finished) return;
    
    engineA_.disconnect();
    engineB_.disconnect();
    state_ = TournamentState::Finished;
    signalStateChanged.emit();
    log("Tournament stopped.");
}

void TournamentController::startTournament(const TournamentConfig& config) {
    config_ = config;
    openings_ = model::OBFManager::readOpenings(config_.obfPath);
    
    if (openings_.empty()) {
        log("No openings found in " + config_.obfPath + ". Defaulting to 1 empty opening.");
        model::GameRecord rec;
        rec.boardSize = 15;
        openings_.push_back(rec);
    }
    
    scoreA_ = 0; scoreB_ = 0; draws_ = 0;
    currentOpeningIndex_ = 0;
    currentGameInOpening_ = 0;
    currentMatchIndex_ = 0;
    totalMatches_ = (int)openings_.size() * config_.gamesPerOpening;
    
    state_ = TournamentState::Initializing;
    signalStateChanged.emit();
    signalScoreChanged.emit();
    
    log("Starting tournament: " + std::to_string(totalMatches_) + " total games.");
    
    currentEngConfig_.timeoutTurn = config.turnTimeMs;
    currentEngConfig_.timeoutMatch = config.matchTimeMs;
    currentEngConfig_.maxMemoryBytes = config.maxMemory;
    currentEngConfig_.threadNum = config.threads;
    
    currentEngConfig_.executablePath = config.engineA_Path;
    if (!engineA_.connect(currentEngConfig_)) {
        log("Failed to start Engine A: " + config.engineA_Path);
        stopTournament();
        return;
    }
    
    currentEngConfig_.executablePath = config.engineB_Path;
    if (!engineB_.connect(currentEngConfig_)) {
        log("Failed to start Engine B: " + config.engineB_Path);
        stopTournament();
        return;
    }
    
    state_ = TournamentState::Playing;
    signalStateChanged.emit();
    
    setupNextMatch();
}

void TournamentController::setupNextMatch() {
    if (currentMatchIndex_ >= totalMatches_) {
        stopTournament();
        log("Tournament Completed!");
        return;
    }
    
    const auto& opening = openings_[currentOpeningIndex_];
    currentBoard_.reset(opening.boardSize);
    currentBoard_.setTopology(opening.topology);
    
    for (const auto& m : opening.moves) {
        if (!m.isPass()) {
            currentBoard_.placeStone(m.coord, m.color);
        } else {
            currentBoard_.pass(m.color);
        }
    }
    
    log(std::string("--- Match ") + std::to_string(currentMatchIndex_ + 1) + " / " + std::to_string(totalMatches_) + " ---");
    
    bool engineAIsBlack = (currentGameInOpening_ % 2 == 0);
    
    sendStartToEngine(engineA_, opening);
    sendStartToEngine(engineB_, opening);
    
    signalBoardUpdated.emit();
    
    bool openingTurnIsBlack = (opening.moves.size() % 2 == 0);
    isEngineATurn_ = (openingTurnIsBlack == engineAIsBlack);
    
    auto entries = opening.toBoardEntries();
    
    if (isEngineATurn_) {
        log("Engine A thinks...");
        engineB_.loadPositionSilent(entries);
        if (entries.empty()) {
            engineA_.loadPositionSilent(entries);
            engineA_.requestEngineMove();
        } else {
            engineA_.loadPosition(entries);
        }
    } else {
        log("Engine B thinks...");
        engineA_.loadPositionSilent(entries);
        if (entries.empty()) {
            engineB_.loadPositionSilent(entries);
            engineB_.requestEngineMove();
        } else {
            engineB_.loadPosition(entries);
        }
    }
}

void TournamentController::sendStartToEngine(engine::EngineController& engine, const model::GameRecord& rec) {
    engine::EngineConfig cfg = currentEngConfig_;
    cfg.boardSize = rec.boardSize;
    cfg.rule = rec.rule;
    
    std::vector<std::pair<int,int>> walls;
    for (const auto& w : rec.topology.walls()) walls.push_back({w.x, w.y});
    
    std::vector<std::tuple<int,int,int,int>> portals;
    for (const auto& p : rec.topology.portals()) portals.push_back({p.a.x, p.a.y, p.b.x, p.b.y});
    
    engine.startGame(cfg, walls, portals);
}

void TournamentController::sendBoardToEngine(engine::EngineController& engine, const model::GameRecord& rec) {
    auto entries = rec.toBoardEntries();
    engine.loadPositionSilent(entries);
}

static bool checkWinLocally(const model::Board& board, util::Coord pos, model::Color c) {
    if (pos == util::Coord::none() || board.cellAt(pos) != model::colorToCell(c)) return false;
    int dx[] = { 1, 0, 1, 1 };
    int dy[] = { 0, 1, 1, -1 };
    model::Cell targetCell = model::colorToCell(c);
    for (int d = 0; d < 4; ++d) {
        int count = 1;
        // Check forward
        util::Coord curr = pos;
        int stepX = dx[d], stepY = dy[d];
        for (int i = 0; i < 5; ++i) {
            curr = util::Coord(curr.x + stepX, curr.y + stepY);
            if (!board.inBounds(curr.x, curr.y)) break;
            auto cell = board.cellAt(curr);
            if (cell == model::Cell::Wall) break;
            if (cell == model::Cell::PortalA || cell == model::Cell::PortalB) {
                auto partner = board.portalPartner(curr.x, curr.y);
                if (!partner) break;
                curr = *partner;
                continue;
            }
            if (cell == targetCell) count++; else break;
        }
        // Check backward
        curr = pos;
        stepX = -dx[d], stepY = -dy[d];
        for (int i = 0; i < 5; ++i) {
            curr = util::Coord(curr.x + stepX, curr.y + stepY);
            if (!board.inBounds(curr.x, curr.y)) break;
            auto cell = board.cellAt(curr);
            if (cell == model::Cell::Wall) break;
            if (cell == model::Cell::PortalA || cell == model::Cell::PortalB) {
                auto partner = board.portalPartner(curr.x, curr.y);
                if (!partner) break;
                curr = *partner;
                continue;
            }
            if (cell == targetCell) count++; else break;
        }
        if (count >= 5) return true;
    }
    return false;
}

void TournamentController::handleEngineMove(bool fromEngineA, int x, int y) {
    if (fromEngineA != isEngineATurn_) {
        // Technically an engine might send an unexpected move message or play early.
        // We could forfeit, but for safety in asynchronous loops, just warn.
        log("Warning: Out of turn move from Engine " + std::string(fromEngineA ? "A" : "B"));
        return;
    }
    
    util::Coord mv(x, y);
    if (mv == util::Coord::none()) {
        currentBoard_.pass(currentBoard_.sideToMove());
        log("Engine " + std::string(fromEngineA ? "A" : "B") + " passed.");
    } else {
        if (!currentBoard_.isLegalMove(mv)) {
            log("ILLEGAL MOVE by Engine " + std::string(fromEngineA ? "A" : "B") + " at " + std::to_string(x) + "," + std::to_string(y));
            endCurrentGame(fromEngineA ? 2 : 1);
            return;
        }
        currentBoard_.placeStone(mv, currentBoard_.sideToMove());
        log("Engine " + std::string(fromEngineA ? "A" : "B") + " played " + std::to_string(x) + "," + std::to_string(y));
    }
    
    signalBoardUpdated.emit();
    
    // Note: since placeStone already switches sideToMove(), we use the !sideToMove to check win
    auto curColor = currentBoard_.sideToMove() == model::Color::Black ? model::Color::White : model::Color::Black;
    
    if (checkWinLocally(currentBoard_, mv, curColor)) {
        log("Win detected for Engine " + std::string(fromEngineA ? "A" : "B"));
        endCurrentGame(fromEngineA ? 1 : 2);
        return;
    }
    if (currentBoard_.isBoardFull()) {
        log("Board full - Draw");
        endCurrentGame(0);
        return;
    }
    
    isEngineATurn_ = !isEngineATurn_;
    
    if (isEngineATurn_) {
        engineA_.makeMove(x, y); 
    } else {
        engineB_.makeMove(x, y);
    }
}

void TournamentController::endCurrentGame(int result) {
    if (state_ == TournamentState::Finished) return;

    if (result == 1) scoreA_++;
    else if (result == 2) scoreB_++;
    else draws_++;
    
    signalScoreChanged.emit();
    saveCurrentGame(result);
    
    currentMatchIndex_++;
    currentGameInOpening_++;
    if (currentGameInOpening_ >= config_.gamesPerOpening) {
        currentGameInOpening_ = 0;
        currentOpeningIndex_++;
    }
    
    // Defer setupNextMatch to clear signal handlers and engine thinking traces
    Glib::signal_idle().connect_once([this]() {
        if (state_ == TournamentState::Finished) return;
        
        // Disconnect and reconnect to guarantee a perfectly clean state for the next match,
        // purging any hung "Thinking" or "Stopping" internal states.
        engineA_.disconnect();
        engineB_.disconnect();
        
        currentEngConfig_.executablePath = config_.engineA_Path;
        engineA_.connect(currentEngConfig_);
        
        currentEngConfig_.executablePath = config_.engineB_Path;
        engineB_.connect(currentEngConfig_);
        
        setupNextMatch();
    });
}

void TournamentController::saveCurrentGame(int result) {
    auto rec = model::GameRecord::fromBoard(currentBoard_);
    rec.result = (result == 1) ? "A+" : (result == 2 ? "B+" : "Draw");
    
    std::filesystem::create_directories("data/tournaments");
    std::string path = "data/tournaments/match_" + std::to_string(currentMatchIndex_+1) + ".sgf";
    
    std::ofstream out(path);
    out << "Match: " << currentMatchIndex_ + 1 << "\n";
    out << "Result: " << rec.result << "\n";
    out << "Total moves: " << rec.moves.size() << "\n\n";
    
    // Save history in standard format
    // <x>,<y>
    for (const auto& m : rec.moves) {
        if (m.isPass()) out << "-1,-1\n";
        else out << m.coord.x << "," << m.coord.y << "\n";
    }
}

} // namespace controller
