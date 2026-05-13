/*
 *  Portal Gomoku UI — SPSA Tuning Controller Implementation
 *
 *  SPSA (Simultaneous Perturbation Stochastic Approximation) tuner
 *  for optimizing engine search/eval parameters via self-play.
 *
 *  Persistence: After each iteration, the full state (parameters,
 *  iteration count, history) is written to a JSON file. On restart,
 *  the state is loaded and tuning resumes from the saved iteration.
 */

#include "SPSAController.hpp"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <glibmm/main.h>

namespace controller {

// ============================================================================
// Construction / Destruction
// ============================================================================

SPSAController::SPSAController()
    : rng_(std::random_device{}())
{
    // Engine θ+ signals (reused for both SPSA and validation)
    connections_.push_back(
        enginePlus_.signalEngineMove.connect([this](int x, int y) {
            if (state_ == SPSAState::Validating)
                handleValidationMove(true, x, y);
            else
                handleEngineMove(true, x, y);
        })
    );
    connections_.push_back(
        enginePlus_.signalError.connect([this](const std::string& err) {
            log("θ+ ERROR: " + err);
            if (state_ == SPSAState::Validating)
                endValidationGame(2);
            else
                endCurrentGame(2);
        })
    );

    // Engine θ- signals (reused for both SPSA and validation)
    connections_.push_back(
        engineMinus_.signalEngineMove.connect([this](int x, int y) {
            if (state_ == SPSAState::Validating)
                handleValidationMove(false, x, y);
            else
                handleEngineMove(false, x, y);
        })
    );
    connections_.push_back(
        engineMinus_.signalError.connect([this](const std::string& err) {
            log("θ- ERROR: " + err);
            if (state_ == SPSAState::Validating)
                endValidationGame(1);
            else
                endCurrentGame(1);
        })
    );
}

SPSAController::~SPSAController() {
    stop();
    for (auto& c : connections_)
        c.disconnect();
}

// ============================================================================
// Lifecycle
// ============================================================================

void SPSAController::start(const SPSAConfig& config) {
    config_ = config;
    stopRequested_ = false;

    // Load openings
    openings_ = model::OBFManager::readOpenings(config_.obfPath);
    if (openings_.empty()) {
        log("No openings found. Using empty board.");
        model::GameRecord rec;
        rec.boardSize = config_.boardSize;
        openings_.push_back(rec);
    }

    // Try to resume from saved state
    if (!config_.statePath.empty() && loadState(config_.statePath)) {
        log("Resumed SPSA from iteration " + std::to_string(iteration_)
            + " with " + std::to_string(params_.size()) + " parameters.");
    } else {
        // Fresh start
        iteration_ = 0;
        history_.clear();
        initDefaultParams();
        log("Starting fresh SPSA with " + std::to_string(params_.size()) + " parameters.");
    }

    state_ = SPSAState::Playing;
    signalStateChanged.emit();
    signalParamsUpdated.emit();

    // Begin first iteration
    generatePerturbation();
    startIterationMatches();
}

void SPSAController::stop() {
    if (state_ == SPSAState::Idle) return;

    stopRequested_ = true;
    enginePlus_.disconnect();
    engineMinus_.disconnect();
    state_ = SPSAState::Finished;
    signalStateChanged.emit();
    log("SPSA tuning stopped.");
}

void SPSAController::pollEngines() {
    enginePlus_.pollOutput();
    engineMinus_.pollOutput();
}

// ============================================================================
// Gain Sequences
// ============================================================================

double SPSAController::a_k() const {
    return config_.a / std::pow(config_.A + iteration_ + 1.0, config_.alpha);
}

double SPSAController::c_k() const {
    return config_.c / std::pow(iteration_ + 1.0, config_.gamma);
}

// ============================================================================
// Parameters
// ============================================================================

void SPSAController::initDefaultParams() {
    params_.clear();

    // PORTAL: The 6 WALL/Portal classical eval & search constants
    // These match the TUNE() ranges registered in portal_src/config.cpp
    params_.push_back({"WALL_DEAD_POCKET_PENALTY",      -60.0,  -200.0,   0.0,  -60.0});
    params_.push_back({"WALL_CORRIDOR_FOUR_BONUS",      450.0,   200.0, 800.0,  450.0});
    params_.push_back({"WALL_ISOLATED_THREAT_PENALTY",  -30.0,  -100.0,   0.0,  -30.0});
    params_.push_back({"WALL_ADJACENCY_MOVE_BONUS",      80.0,    50.0, 200.0,   80.0});
    params_.push_back({"PORTAL_ADJACENCY_MOVE_BONUS",    60.0,    40.0, 150.0,   60.0});
    params_.push_back({"WALL_FIRST_MOVE_BONUS",         250.0,   100.0, 400.0,  250.0});
}

void SPSAController::generatePerturbation() {
    delta_.resize(params_.size());
    std::uniform_int_distribution<int> dist(0, 1);
    for (size_t i = 0; i < params_.size(); ++i)
        delta_[i] = dist(rng_) * 2 - 1; // -1 or +1
}

void SPSAController::applyPerturbedParams() {
    double ck = c_k();

    // Send θ+ params to enginePlus_
    for (size_t i = 0; i < params_.size(); ++i) {
        double perturbed = params_[i].value + ck * delta_[i];
        perturbed = std::clamp(perturbed, params_[i].min, params_[i].max);
        int intVal = static_cast<int>(std::round(perturbed));

        // Send via Gomocup INFO protocol
        std::string cmd = "INFO " + params_[i].name + " " + std::to_string(intVal);
        enginePlus_.send_raw_if_idle(cmd);
    }

    // Send θ- params to engineMinus_
    for (size_t i = 0; i < params_.size(); ++i) {
        double perturbed = params_[i].value - ck * delta_[i];
        perturbed = std::clamp(perturbed, params_[i].min, params_[i].max);
        int intVal = static_cast<int>(std::round(perturbed));

        std::string cmd = "INFO " + params_[i].name + " " + std::to_string(intVal);
        engineMinus_.send_raw_if_idle(cmd);
    }
}

// ============================================================================
// Match Execution
// ============================================================================

void SPSAController::startIterationMatches() {
    log("=== SPSA Iteration " + std::to_string(iteration_ + 1) + " ===");
    log("a_k=" + std::to_string(a_k()) + " c_k=" + std::to_string(c_k()));

    matchScorePlus_ = 0;
    matchScoreMinus_ = 0;
    matchDraws_ = 0;
    matchGameIndex_ = 0;
    currentOpeningIdx_ = 0;
    currentGameInOpening_ = 0;
    matchTotalGames_ = config_.gamesPerIteration;

    // Connect engines
    engine::EngineConfig engCfg;
    engCfg.executablePath = config_.enginePath;
    engCfg.timeoutTurn = config_.turnTimeMs;
    engCfg.timeoutMatch = config_.matchTimeMs;
    engCfg.maxMemoryBytes = config_.maxMemory;
    engCfg.threadNum = config_.threads;
    engCfg.boardSize = config_.boardSize;
    engCfg.rule = config_.rule;

    engCfg.executablePath = config_.enginePath;
    if (!enginePlus_.connect(engCfg)) {
        log("Failed to start engine θ+!");
        stop();
        return;
    }
    if (!engineMinus_.connect(engCfg)) {
        log("Failed to start engine θ-!");
        stop();
        return;
    }

    // Apply perturbed parameters
    applyPerturbedParams();

    // Start the first game
    setupNextGame();
}

void SPSAController::setupNextGame() {
    if (matchGameIndex_ >= matchTotalGames_) {
        finalizeIteration();
        return;
    }

    // Pick opening (cycle through available openings)
    const auto& opening = openings_[currentOpeningIdx_ % openings_.size()];
    matchBoard_.reset(opening.boardSize > 0 ? opening.boardSize : config_.boardSize);
    matchBoard_.setTopology(opening.topology);

    for (const auto& m : opening.moves) {
        if (!m.isPass())
            matchBoard_.placeStone(m.coord, m.color);
        else
            matchBoard_.pass(m.color);
    }

    log("Game " + std::to_string(matchGameIndex_ + 1) + "/" + std::to_string(matchTotalGames_));

    sendStartToEngine(enginePlus_, opening);
    sendStartToEngine(engineMinus_, opening);

    // Alternate who plays Black: even games → θ+ is Black, odd → θ- is Black
    bool plusIsBlack = (currentGameInOpening_ % 2 == 0);
    bool openingTurnIsBlack = (opening.moves.size() % 2 == 0);
    isPlusTurn_ = (openingTurnIsBlack == plusIsBlack);

    signalBoardUpdated.emit(matchBoard_);
    std::string p1 = plusIsBlack ? "θ+ (Tuning)" : "θ- (Baseline)";
    std::string p2 = plusIsBlack ? "θ- (Baseline)" : "θ+ (Tuning)";
    signalMatchInfo.emit(p1, p2);

    auto entries = opening.toBoardEntries();

    if (isPlusTurn_) {
        engineMinus_.loadPositionSilent(entries);
        if (entries.empty()) {
            enginePlus_.loadPositionSilent(entries);
            enginePlus_.requestEngineMove();
        } else {
            enginePlus_.loadPosition(entries);
        }
    } else {
        enginePlus_.loadPositionSilent(entries);
        if (entries.empty()) {
            engineMinus_.loadPositionSilent(entries);
            engineMinus_.requestEngineMove();
        } else {
            engineMinus_.loadPosition(entries);
        }
    }
}

void SPSAController::sendStartToEngine(
    engine::EngineController& engine, const model::GameRecord& rec)
{
    engine::EngineConfig cfg;
    cfg.boardSize = rec.boardSize > 0 ? rec.boardSize : config_.boardSize;
    cfg.rule = rec.rule;
    cfg.timeoutTurn = config_.turnTimeMs;
    cfg.timeoutMatch = config_.matchTimeMs;
    cfg.maxMemoryBytes = config_.maxMemory;
    cfg.threadNum = config_.threads;

    std::vector<std::pair<int,int>> walls;
    for (const auto& w : rec.topology.walls())
        walls.push_back({w.x, w.y});

    std::vector<std::tuple<int,int,int,int>> portals;
    for (const auto& p : rec.topology.portals())
        portals.push_back({p.a.x, p.a.y, p.b.x, p.b.y});

    engine.startGame(cfg, walls, portals);
}

// Win check — same logic as TournamentController
static bool checkWin(const model::Board& board, util::Coord pos, model::Color c) {
    if (pos == util::Coord::none() || board.cellAt(pos) != model::colorToCell(c))
        return false;
    int dx[] = { 1, 0, 1, 1 };
    int dy[] = { 0, 1, 1, -1 };
    model::Cell targetCell = model::colorToCell(c);
    for (int d = 0; d < 4; ++d) {
        int count = 1;
        util::Coord curr = pos;
        for (int i = 0; i < 5; ++i) {
            curr = util::Coord(curr.x + dx[d], curr.y + dy[d]);
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
        curr = pos;
        for (int i = 0; i < 5; ++i) {
            curr = util::Coord(curr.x - dx[d], curr.y - dy[d]);
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

void SPSAController::handleEngineMove(bool fromPlus, int x, int y) {
    if (state_ != SPSAState::Playing) return;
    if (fromPlus != isPlusTurn_) return; // out-of-turn, ignore

    util::Coord mv(x, y);
    if (mv == util::Coord::none()) {
        matchBoard_.pass(matchBoard_.sideToMove());
    } else {
        if (!matchBoard_.isLegalMove(mv)) {
            log("ILLEGAL move from θ" + std::string(fromPlus ? "+" : "-")
                + " at " + std::to_string(x) + "," + std::to_string(y));
            endCurrentGame(fromPlus ? 2 : 1);
            return;
        }
        matchBoard_.placeStone(mv, matchBoard_.sideToMove());
    }
    signalMoveMade.emit(x, y, static_cast<int>(matchBoard_.sideToMove() == model::Color::Black ? 1 : 2));
    signalBoardUpdated.emit(matchBoard_);

    // Check win
    auto curColor = matchBoard_.sideToMove() == model::Color::Black
                    ? model::Color::White : model::Color::Black;
    if (checkWin(matchBoard_, mv, curColor)) {
        endCurrentGame(fromPlus ? 1 : 2);
        return;
    }
    if (matchBoard_.isBoardFull()) {
        endCurrentGame(0);
        return;
    }

    // Switch turns
    isPlusTurn_ = !isPlusTurn_;
    if (isPlusTurn_)
        enginePlus_.makeMove(x, y);
    else
        engineMinus_.makeMove(x, y);
}

void SPSAController::endCurrentGame(int result) {
    if (state_ != SPSAState::Playing) return;

    if (result == 1) matchScorePlus_++;
    else if (result == 2) matchScoreMinus_++;
    else matchDraws_++;

    std::string r = result == 1 ? "θ+ won" : (result == 2 ? "θ- won" : "Draw");
    log("  Game " + std::to_string(matchGameIndex_ + 1) + " result: " + r
        + " (Score: +" + std::to_string(matchScorePlus_)
        + " -" + std::to_string(matchScoreMinus_)
        + " =" + std::to_string(matchDraws_) + ")");

    matchGameIndex_++;
    currentGameInOpening_++;
    if (currentGameInOpening_ >= 2) { // 2 games per opening (swap sides)
        currentGameInOpening_ = 0;
        currentOpeningIdx_++;
    }

    // Defer to avoid recursion in signal handlers
    Glib::signal_idle().connect_once([this]() {
        if (state_ != SPSAState::Playing) return;

        // Reconnect engines cleanly
        enginePlus_.disconnect();
        engineMinus_.disconnect();

        engine::EngineConfig engCfg;
        engCfg.executablePath = config_.enginePath;
        engCfg.timeoutTurn = config_.turnTimeMs;
        engCfg.timeoutMatch = config_.matchTimeMs;
        engCfg.maxMemoryBytes = config_.maxMemory;
        engCfg.threadNum = config_.threads;

        enginePlus_.connect(engCfg);
        engineMinus_.connect(engCfg);

        // Re-apply perturbed params (they are reset after disconnect)
        applyPerturbedParams();

        setupNextGame();
    });
}

// ============================================================================
// Gradient Update
// ============================================================================

void SPSAController::finalizeIteration() {
    // Compute the game result as a score differential:
    // W+ = score of θ+ (wins + 0.5*draws)
    // W- = score of θ- (wins + 0.5*draws)
    double Wplus  = matchScorePlus_ + 0.5 * matchDraws_;
    double Wminus = matchScoreMinus_ + 0.5 * matchDraws_;
    double totalGames = matchScorePlus_ + matchScoreMinus_ + matchDraws_;

    // Normalize to [0, 1] scale
    double scoreDiff = (totalGames > 0) ? (Wplus - Wminus) / totalGames : 0.0;

    double ak = a_k();
    double ck = c_k();

    log("Iteration " + std::to_string(iteration_ + 1)
        + " complete: θ+=" + std::to_string(matchScorePlus_)
        + " θ-=" + std::to_string(matchScoreMinus_)
        + " D=" + std::to_string(matchDraws_)
        + " diff=" + std::to_string(scoreDiff));

    // Update each parameter: θ_{k+1} = θ_k - a_k * g_hat_k
    // where g_hat_k[i] = (scoreDiff) / (2 * c_k * delta[i])
    // Note: we want to maximize winrate, so we use + instead of -
    // (or equivalently negate the gradient direction)
    for (size_t i = 0; i < params_.size(); ++i) {
        // Gradient estimate for parameter i
        double g_hat = scoreDiff / (2.0 * ck * delta_[i]);

        // Update: move in the direction that increases θ+ score
        double newVal = params_[i].value + ak * g_hat;
        newVal = std::clamp(newVal, params_[i].min, params_[i].max);

        log("  " + params_[i].name + ": "
            + std::to_string(static_cast<int>(std::round(params_[i].value)))
            + " -> " + std::to_string(static_cast<int>(std::round(newVal))));

        params_[i].value = newVal;
    }

    // Record history
    history_.push_back({iteration_ + 1,
                        static_cast<double>(matchScorePlus_),
                        static_cast<double>(matchScoreMinus_),
                        static_cast<double>(matchDraws_)});

    iteration_++;
    signalIterationComplete.emit();
    signalParamsUpdated.emit();

    // Persist state
    saveState();

    // Check stop
    if (stopRequested_) {
        state_ = SPSAState::Finished;
        signalStateChanged.emit();
        log("SPSA stopped after " + std::to_string(iteration_) + " iterations.");
        return;
    }

    // Disconnect engines before next step
    enginePlus_.disconnect();
    engineMinus_.disconnect();

    // Check if validation should run before next SPSA iteration
    if (shouldValidate()) {
        Glib::signal_idle().connect_once([this]() {
            if (stopRequested_) return;
            startValidationMatch();
        });
        return;
    }

    // Next SPSA iteration
    generatePerturbation();
    Glib::signal_idle().connect_once([this]() {
        if (stopRequested_ || state_ != SPSAState::Playing) return;
        startIterationMatches();
    });
}

// ============================================================================
// Validation (current θ vs baseline)
// ============================================================================

bool SPSAController::shouldValidate() const {
    if (config_.baselinePath.empty()) return false;
    if (config_.validationInterval <= 0) return false;
    return (iteration_ % config_.validationInterval == 0);
}

void SPSAController::applyCurrentParams(engine::EngineController& engine) {
    for (const auto& p : params_) {
        int intVal = static_cast<int>(std::round(p.value));
        engine.send_raw_if_idle("INFO " + p.name + " " + std::to_string(intVal));
    }
}

void SPSAController::startValidationMatch() {
    state_ = SPSAState::Validating;
    signalStateChanged.emit();

    log("=== VALIDATION after iteration " + std::to_string(iteration_)
        + " (θ vs baseline) ===");

    matchScorePlus_ = 0;   // tuned engine score
    matchScoreMinus_ = 0;  // baseline score
    matchDraws_ = 0;
    matchGameIndex_ = 0;
    currentOpeningIdx_ = 0;
    currentGameInOpening_ = 0;
    matchTotalGames_ = config_.validationGames;

    // Connect tuned engine (θ+)
    engine::EngineConfig tunedCfg;
    tunedCfg.executablePath = config_.enginePath;
    tunedCfg.timeoutTurn = config_.turnTimeMs;
    tunedCfg.timeoutMatch = config_.matchTimeMs;
    tunedCfg.maxMemoryBytes = config_.maxMemory;
    tunedCfg.threadNum = config_.threads;

    if (!enginePlus_.connect(tunedCfg)) {
        log("Failed to start tuned engine for validation!");
        // Fall through to next SPSA iteration
        state_ = SPSAState::Playing;
        signalStateChanged.emit();
        generatePerturbation();
        startIterationMatches();
        return;
    }

    // Connect baseline engine (θ-)
    engine::EngineConfig baseCfg = tunedCfg;
    baseCfg.executablePath = config_.baselinePath;
    if (!engineMinus_.connect(baseCfg)) {
        log("Failed to start baseline engine for validation!");
        enginePlus_.disconnect();
        state_ = SPSAState::Playing;
        signalStateChanged.emit();
        generatePerturbation();
        startIterationMatches();
        return;
    }

    // Apply current best θ to tuned engine (no perturbation)
    applyCurrentParams(enginePlus_);
    // Baseline gets NO param overrides — uses its compiled defaults

    setupNextValidationGame();
}

void SPSAController::setupNextValidationGame() {
    if (matchGameIndex_ >= matchTotalGames_) {
        finalizeValidation();
        return;
    }

    const auto& opening = openings_[currentOpeningIdx_ % openings_.size()];
    matchBoard_.reset(opening.boardSize > 0 ? opening.boardSize : config_.boardSize);
    matchBoard_.setTopology(opening.topology);

    for (const auto& m : opening.moves) {
        if (!m.isPass()) matchBoard_.placeStone(m.coord, m.color);
        else matchBoard_.pass(m.color);
    }

    log("  Val game " + std::to_string(matchGameIndex_ + 1)
        + "/" + std::to_string(matchTotalGames_));

    sendStartToEngine(enginePlus_, opening);
    sendStartToEngine(engineMinus_, opening);

    // Alternate sides: even = tuned is Black, odd = baseline is Black
    bool tunedIsBlack = (currentGameInOpening_ % 2 == 0);
    bool openingTurnIsBlack = (opening.moves.size() % 2 == 0);
    isPlusTurn_ = (openingTurnIsBlack == tunedIsBlack);

    signalBoardUpdated.emit(matchBoard_);
    std::string p1 = tunedIsBlack ? "Tuned θ" : "Baseline";
    std::string p2 = tunedIsBlack ? "Baseline" : "Tuned θ";
    signalMatchInfo.emit(p1, p2);

    auto entries = opening.toBoardEntries();

    if (isPlusTurn_) {
        engineMinus_.loadPositionSilent(entries);
        if (entries.empty()) {
            enginePlus_.loadPositionSilent(entries);
            enginePlus_.requestEngineMove();
        } else {
            enginePlus_.loadPosition(entries);
        }
    } else {
        enginePlus_.loadPositionSilent(entries);
        if (entries.empty()) {
            engineMinus_.loadPositionSilent(entries);
            engineMinus_.requestEngineMove();
        } else {
            engineMinus_.loadPosition(entries);
        }
    }
}

void SPSAController::handleValidationMove(bool fromTuned, int x, int y) {
    if (state_ != SPSAState::Validating) return;
    if (fromTuned != isPlusTurn_) return;

    util::Coord mv(x, y);
    if (mv == util::Coord::none()) {
        matchBoard_.pass(matchBoard_.sideToMove());
    } else {
        if (!matchBoard_.isLegalMove(mv)) {
            endValidationGame(fromTuned ? 2 : 1);
            return;
        }
        matchBoard_.placeStone(mv, matchBoard_.sideToMove());
    }
    
    signalMoveMade.emit(x, y, static_cast<int>(matchBoard_.sideToMove() == model::Color::Black ? 1 : 2));
    signalBoardUpdated.emit(matchBoard_);

    auto curColor = matchBoard_.sideToMove() == model::Color::Black
                    ? model::Color::White : model::Color::Black;
    if (checkWin(matchBoard_, mv, curColor)) {
        endValidationGame(fromTuned ? 1 : 2);
        return;
    }
    if (matchBoard_.isBoardFull()) {
        endValidationGame(0);
        return;
    }

    isPlusTurn_ = !isPlusTurn_;
    if (isPlusTurn_)
        enginePlus_.makeMove(x, y);
    else
        engineMinus_.makeMove(x, y);
}

void SPSAController::endValidationGame(int result) {
    if (state_ != SPSAState::Validating) return;

    if (result == 1) matchScorePlus_++;
    else if (result == 2) matchScoreMinus_++;
    else matchDraws_++;

    matchGameIndex_++;
    currentGameInOpening_++;
    if (currentGameInOpening_ >= 2) {
        currentGameInOpening_ = 0;
        currentOpeningIdx_++;
    }

    Glib::signal_idle().connect_once([this]() {
        if (state_ != SPSAState::Validating) return;

        enginePlus_.disconnect();
        engineMinus_.disconnect();

        engine::EngineConfig tunedCfg;
        tunedCfg.executablePath = config_.enginePath;
        tunedCfg.timeoutTurn = config_.turnTimeMs;
        tunedCfg.timeoutMatch = config_.matchTimeMs;
        tunedCfg.maxMemoryBytes = config_.maxMemory;
        tunedCfg.threadNum = config_.threads;

        enginePlus_.connect(tunedCfg);

        engine::EngineConfig baseCfg = tunedCfg;
        baseCfg.executablePath = config_.baselinePath;
        engineMinus_.connect(baseCfg);

        applyCurrentParams(enginePlus_);
        setupNextValidationGame();
    });
}

void SPSAController::finalizeValidation() {
    double total = matchScorePlus_ + matchScoreMinus_ + matchDraws_;
    double winrate = total > 0
        ? (matchScorePlus_ + 0.5 * matchDraws_) / total * 100.0 : 50.0;

    SPSAValidationResult vr;
    vr.afterIteration = iteration_;
    vr.scoreTuned = matchScorePlus_;
    vr.scoreBaseline = matchScoreMinus_;
    vr.draws = matchDraws_;
    vr.winrate = winrate;
    valHistory_.push_back(vr);

    log("VALIDATION result: Tuned=" + std::to_string(matchScorePlus_)
        + " Baseline=" + std::to_string(matchScoreMinus_)
        + " Draws=" + std::to_string(matchDraws_)
        + " Winrate=" + std::to_string(winrate) + "%");

    signalValidationComplete.emit();
    saveState();

    // Resume SPSA
    enginePlus_.disconnect();
    engineMinus_.disconnect();

    state_ = SPSAState::Playing;
    signalStateChanged.emit();

    if (stopRequested_) {
        state_ = SPSAState::Finished;
        signalStateChanged.emit();
        return;
    }

    generatePerturbation();
    Glib::signal_idle().connect_once([this]() {
        if (stopRequested_ || state_ != SPSAState::Playing) return;
        startIterationMatches();
    });
}

// ============================================================================
// Persistence — Simple JSON serialization (hand-written to avoid dependency)
// ============================================================================

void SPSAController::saveState() {
    if (config_.statePath.empty()) return;

    std::filesystem::create_directories(
        std::filesystem::path(config_.statePath).parent_path());

    std::ofstream out(config_.statePath);
    if (!out.is_open()) {
        log("Warning: Cannot write SPSA state to " + config_.statePath);
        return;
    }

    out << std::fixed << std::setprecision(6);
    out << "{\n";
    out << "  \"iteration\": " << iteration_ << ",\n";

    // Parameters
    out << "  \"params\": [\n";
    for (size_t i = 0; i < params_.size(); ++i) {
        out << "    { \"name\": \"" << params_[i].name << "\""
            << ", \"value\": " << params_[i].value
            << ", \"min\": " << params_[i].min
            << ", \"max\": " << params_[i].max
            << ", \"initial\": " << params_[i].initial
            << " }";
        if (i + 1 < params_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    // History
    out << "  \"history\": [\n";
    for (size_t i = 0; i < history_.size(); ++i) {
        out << "    { \"iter\": " << history_[i].iteration
            << ", \"plus\": " << history_[i].scoreTheta_plus
            << ", \"minus\": " << history_[i].scoreTheta_minus
            << ", \"draws\": " << history_[i].draws
            << " }";
        if (i + 1 < history_.size()) out << ",";
        out << "\n";
    }
    out << "  ],\n";

    // Validation history
    out << "  \"validation\": [\n";
    for (size_t i = 0; i < valHistory_.size(); ++i) {
        out << "    { \"after\": " << valHistory_[i].afterIteration
            << ", \"tuned\": " << valHistory_[i].scoreTuned
            << ", \"baseline\": " << valHistory_[i].scoreBaseline
            << ", \"draws\": " << valHistory_[i].draws
            << ", \"winrate\": " << valHistory_[i].winrate
            << " }";
        if (i + 1 < valHistory_.size()) out << ",";
        out << "\n";
    }
    out << "  ]\n";
    out << "}\n";

    log("State saved to " + config_.statePath + " (iter=" + std::to_string(iteration_) + ")");
}

bool SPSAController::loadState(const std::string& path) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    // Simple JSON parser for our specific format
    std::string content((std::istreambuf_iterator<char>(in)),
                         std::istreambuf_iterator<char>());

    // Parse iteration
    auto iterPos = content.find("\"iteration\":");
    if (iterPos == std::string::npos) return false;
    iteration_ = std::stoi(content.substr(iterPos + 13));

    // Parse params
    params_.clear();
    auto paramsStart = content.find("\"params\":");
    if (paramsStart == std::string::npos) return false;

    size_t searchFrom = paramsStart;
    while (true) {
        auto namePos = content.find("\"name\":", searchFrom);
        if (namePos == std::string::npos) break;

        // Check we haven't passed the params array end
        auto nextBracket = content.find(']', paramsStart + 9);
        if (namePos > nextBracket) break;

        SPSAParam param;

        // Parse name
        auto nameStart = content.find('"', namePos + 7) + 1;
        auto nameEnd = content.find('"', nameStart);
        param.name = content.substr(nameStart, nameEnd - nameStart);

        // Parse value
        auto valPos = content.find("\"value\":", nameEnd);
        param.value = std::stod(content.substr(valPos + 8));

        // Parse min
        auto minPos = content.find("\"min\":", valPos);
        param.min = std::stod(content.substr(minPos + 6));

        // Parse max
        auto maxPos = content.find("\"max\":", minPos);
        param.max = std::stod(content.substr(maxPos + 6));

        // Parse initial
        auto initPos = content.find("\"initial\":", maxPos);
        param.initial = std::stod(content.substr(initPos + 10));

        params_.push_back(param);
        searchFrom = initPos + 10;
    }

    // Parse history
    history_.clear();
    auto histStart = content.find("\"history\":");
    if (histStart != std::string::npos) {
        size_t hSearch = histStart;
        while (true) {
            auto iterPos2 = content.find("\"iter\":", hSearch);
            if (iterPos2 == std::string::npos) break;

            auto histEnd = content.find(']', histStart + 10);
            if (iterPos2 > histEnd) break;

            SPSAIterationResult res;
            res.iteration = std::stoi(content.substr(iterPos2 + 7));

            auto plusPos = content.find("\"plus\":", iterPos2);
            res.scoreTheta_plus = std::stod(content.substr(plusPos + 7));

            auto minusPos = content.find("\"minus\":", plusPos);
            res.scoreTheta_minus = std::stod(content.substr(minusPos + 8));

            auto drawsPos = content.find("\"draws\":", minusPos);
            res.draws = std::stod(content.substr(drawsPos + 8));

            history_.push_back(res);
            hSearch = drawsPos + 8;
        }
    }

    // Parse validation history
    valHistory_.clear();
    auto valStart = content.find("\"validation\":");
    if (valStart != std::string::npos) {
        size_t vSearch = valStart;
        while (true) {
            auto afterPos = content.find("\"after\":", vSearch);
            if (afterPos == std::string::npos) break;

            auto valEnd = content.find(']', valStart + 13);
            if (afterPos > valEnd) break;

            SPSAValidationResult vr;
            vr.afterIteration = std::stoi(content.substr(afterPos + 8));

            auto tunedPos = content.find("\"tuned\":", afterPos);
            vr.scoreTuned = std::stoi(content.substr(tunedPos + 8));

            auto basePos = content.find("\"baseline\":", tunedPos);
            vr.scoreBaseline = std::stoi(content.substr(basePos + 11));

            auto drawsPos = content.find("\"draws\":", basePos);
            vr.draws = std::stoi(content.substr(drawsPos + 8));

            auto wrPos = content.find("\"winrate\":", drawsPos);
            vr.winrate = std::stod(content.substr(wrPos + 10));

            valHistory_.push_back(vr);
            vSearch = wrPos + 10;
        }
    }

    return !params_.empty();
}

// ============================================================================
// Helpers
// ============================================================================

void SPSAController::log(const std::string& msg) {
    std::cout << "[SPSA] " << msg << std::endl;
    signalLogMessage.emit(msg);
}

} // namespace controller
