/*
 *  Portal Gomoku UI — Tournament Controller
 *  Manages a match between two engines across multiple openings.
 */

#pragma once

#include "../model/Board.hpp"
#include "../engine/EngineController.hpp"
#include "../model/OBFManager.hpp"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>

namespace controller {

enum class TournamentState {
    Idle,
    Initializing,     // Connecting to engines
    Playing,          // Engines are playing
    Finished
};

struct TournamentConfig {
    std::string engineA_Path;
    std::string engineB_Path;
    std::string obfPath;
    int matchTimeMs = 0;
    int turnTimeMs = 5000;
    int64_t maxMemory = 350 * 1024 * 1024;
    int threads = 1;
    int gamesPerOpening = 2; // Engine A as Black, then Engine B as Black
};

class TournamentController {
public:
    TournamentController();
    ~TournamentController();

    void startTournament(const TournamentConfig& config);
    void stopTournament();

    [[nodiscard]] TournamentState state() const { return state_; }
    [[nodiscard]] int gamesPlayed() const { return currentMatchIndex_; }
    [[nodiscard]] int totalGames() const { return totalMatches_; }
    
    // Scores
    [[nodiscard]] int scoreEngineA() const { return scoreA_; }
    [[nodiscard]] int scoreEngineB() const { return scoreB_; }
    [[nodiscard]] int scoreDraws() const { return draws_; }

    [[nodiscard]] const model::Board& currentBoard() const { return currentBoard_; }

    /// Must be called continuously from GTK timer (e.g. from MainWindow)
    void pollEngines();

    /// Signals for UI
    sigc::signal<void()> signalStateChanged;
    sigc::signal<void(const std::string&)> signalLogMessage;
    sigc::signal<void(bool, const std::string&)> signalRawComm;
    sigc::signal<void()> signalScoreChanged;
    sigc::signal<void()> signalBoardUpdated;

private:
    TournamentState state_ = TournamentState::Idle;
    TournamentConfig config_;
    engine::EngineConfig currentEngConfig_;
    
    engine::EngineController engineA_;
    engine::EngineController engineB_;

    model::Board currentBoard_{15};
    std::vector<model::GameRecord> openings_;
    
    int scoreA_ = 0;
    int scoreB_ = 0;
    int draws_ = 0;

    int currentOpeningIndex_ = 0;
    int currentGameInOpening_ = 0; // 0=A is Black, 1=B is Black
    int currentMatchIndex_ = 0;
    int totalMatches_ = 0;
    
    bool isEngineATurn_ = false;

    // State machine helpers
    void log(const std::string& msg);
    void setupNextMatch();
    void sendStartToEngine(engine::EngineController& engine, const model::GameRecord& rec);
    void sendBoardToEngine(engine::EngineController& engine, const model::GameRecord& rec);
    void handleEngineMove(bool fromEngineA, int x, int y);
    void endCurrentGame(int resultWinner); // 1 = A won, 2 = B won, 0 = Draw

    // Save game record
    void saveCurrentGame(int result);

    std::vector<sigc::connection> connections_;
};

} // namespace controller
