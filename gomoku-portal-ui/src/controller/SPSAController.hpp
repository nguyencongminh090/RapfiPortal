/*
 *  Portal Gomoku UI — SPSA Tuning Controller
 *  Implements Simultaneous Perturbation Stochastic Approximation
 *  for automated engine parameter optimization via self-play.
 *
 *  Algorithm reference: Spall, J.C. (1998)
 *  "Implementation of the Simultaneous Perturbation Algorithm
 *   for Stochastic Optimization"
 *
 *  Key design:
 *    - Each iteration plays N games between θ+ and θ- perturbed engines.
 *    - Every M iterations, a validation match runs: current θ vs baseline.
 *    - State is persisted to JSON after every iteration for stop/continue.
 *    - Parameters are sent to engines via "INFO <name> <value>" protocol.
 */

#pragma once

#include "../engine/EngineController.hpp"
#include "../model/Board.hpp"
#include "../model/OBFManager.hpp"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>
#include <random>
#include <filesystem>

namespace controller {

// ============================================================================
// Data Structures
// ============================================================================

/// A single tunable parameter with its metadata and current value.
struct SPSAParam {
    std::string name;       ///< Engine INFO key (e.g. "WALL_DEAD_POCKET_PENALTY")
    double      value;      ///< Current optimized value
    double      min;        ///< Lower bound
    double      max;        ///< Upper bound
    double      initial;    ///< Starting value (for reference)
};

/// SPSA gain sequence configuration.
struct SPSAConfig {
    std::string enginePath;         ///< Path to the tuning engine binary
    std::string baselinePath;       ///< Path to the baseline engine binary (for validation)
    std::string obfPath;            ///< Path to opening book file
    std::string statePath;          ///< Path to save/load state JSON

    // Gain sequence constants (Spall's recommended defaults)
    double a     = 1.0;            ///< Learning rate base
    double c     = 2.0;            ///< Perturbation magnitude base
    double alpha = 0.602;          ///< a_k decay exponent
    double gamma = 0.101;          ///< c_k decay exponent
    double A     = 10.0;           ///< Stabilization constant

    // Match settings
    int gamesPerIteration = 20;    ///< Total games per SPSA iteration
    int turnTimeMs        = 5000;  ///< Per-move time limit
    int matchTimeMs       = 0;     ///< Match time (0=unlimited)
    int threads           = 1;     ///< Threads per engine
    int64_t maxMemory     = 350LL * 1024 * 1024; ///< Memory per engine
    int boardSize         = 15;    ///< Board size for games
    int rule              = 0;     ///< Game rule

    // Validation settings
    int validationInterval = 5;    ///< Run validation every N SPSA iterations
    int validationGames    = 20;   ///< Number of games per validation match
};

/// Snapshot of one completed SPSA iteration for the log.
struct SPSAIterationResult {
    int    iteration;
    double scoreTheta_plus;    ///< Win count for θ+
    double scoreTheta_minus;   ///< Win count for θ-
    double draws;
};

/// Snapshot of one validation match result.
struct SPSAValidationResult {
    int    afterIteration;     ///< Which SPSA iteration preceded this validation
    int    scoreTuned;         ///< Wins for tuned engine (current θ)
    int    scoreBaseline;      ///< Wins for baseline engine (original params)
    int    draws;
    double winrate;            ///< Tuned engine winrate [0..100]
};

enum class SPSAState {
    Idle,           ///< Not running
    Playing,        ///< SPSA perturbation match in progress
    Validating,     ///< Validation match vs baseline in progress
    Finished        ///< Tuning manually stopped or completed
};

// ============================================================================
// Controller
// ============================================================================

class SPSAController {
public:
    SPSAController();
    ~SPSAController();

    // -----------------------------------------------------------------------
    // Lifecycle
    // -----------------------------------------------------------------------

    /// Start (or resume) SPSA tuning with the given config.
    /// If a state file exists at config.statePath, it will be loaded.
    void start(const SPSAConfig& config);

    /// Gracefully stop after the current iteration finishes.
    void stop();

    /// Must be called periodically from GTK timer.
    void pollEngines();

    // -----------------------------------------------------------------------
    // State queries
    // -----------------------------------------------------------------------

    [[nodiscard]] SPSAState state() const { return state_; }
    [[nodiscard]] int currentIteration() const { return iteration_; }
    [[nodiscard]] const std::vector<SPSAParam>& params() const { return params_; }
    [[nodiscard]] const std::vector<SPSAIterationResult>& history() const { return history_; }
    [[nodiscard]] const std::vector<SPSAValidationResult>& validationHistory() const { return valHistory_; }

    // -----------------------------------------------------------------------
    // Signals
    // -----------------------------------------------------------------------

    sigc::signal<void()>                     signalStateChanged;
    sigc::signal<void(const std::string&)>   signalLogMessage;
    sigc::signal<void()>                     signalIterationComplete;
    sigc::signal<void()>                     signalParamsUpdated;
    sigc::signal<void()>                     signalValidationComplete;

    // Game observation signals
    sigc::signal<void(const model::Board&)>  signalBoardUpdated;
    sigc::signal<void(int, int, int)>        signalMoveMade; // x, y, color
    sigc::signal<void(const std::string&, const std::string&)> signalMatchInfo;

private:
    SPSAState   state_ = SPSAState::Idle;
    SPSAConfig  config_;
    int         iteration_ = 0;
    bool        stopRequested_ = false;

    // Parameters being tuned
    std::vector<SPSAParam> params_;

    // Current perturbation vector (±1 per parameter)
    std::vector<int> delta_;

    // Iteration history
    std::vector<SPSAIterationResult> history_;

    // Validation history
    std::vector<SPSAValidationResult> valHistory_;

    // Engines for SPSA: θ+ (plus) and θ- (minus)
    // Also reused for validation: tuned (plus) vs baseline (minus)
    engine::EngineController enginePlus_;
    engine::EngineController engineMinus_;

    // Opening book
    std::vector<model::GameRecord> openings_;

    // Match state within one iteration / validation
    model::Board matchBoard_{15};
    int matchScorePlus_  = 0;   // θ+ score (SPSA) or tuned score (validation)
    int matchScoreMinus_ = 0;   // θ- score (SPSA) or baseline score (validation)
    int matchDraws_      = 0;
    int matchGameIndex_  = 0;
    int matchTotalGames_ = 0;
    int currentOpeningIdx_ = 0;
    int currentGameInOpening_ = 0;
    bool isPlusTurn_ = false;

    // RNG for perturbation
    std::mt19937 rng_;

    // Signal connections
    std::vector<sigc::connection> connections_;

    // -----------------------------------------------------------------------
    // Gain sequences
    // -----------------------------------------------------------------------

    [[nodiscard]] double a_k() const;
    [[nodiscard]] double c_k() const;

    // -----------------------------------------------------------------------
    // Algorithm steps
    // -----------------------------------------------------------------------

    void initDefaultParams();
    void generatePerturbation();
    void applyPerturbedParams();
    void startIterationMatches();
    void setupNextGame();
    void handleEngineMove(bool fromPlus, int x, int y);
    void endCurrentGame(int result);
    void finalizeIteration();
    void sendStartToEngine(engine::EngineController& engine,
                           const model::GameRecord& rec);

    // -----------------------------------------------------------------------
    // Validation (current θ vs baseline)
    // -----------------------------------------------------------------------

    /// Check if validation should run after this iteration.
    [[nodiscard]] bool shouldValidate() const;

    /// Start a validation match: current θ vs baseline engine.
    void startValidationMatch();

    /// Set up the next game in a validation match.
    void setupNextValidationGame();

    /// Handle a move during validation.
    void handleValidationMove(bool fromTuned, int x, int y);

    /// End a single validation game.
    void endValidationGame(int result);

    /// Finalize validation match and resume SPSA.
    void finalizeValidation();

    /// Apply current best θ params to the tuned engine (no perturbation).
    void applyCurrentParams(engine::EngineController& engine);

    // -----------------------------------------------------------------------
    // Persistence
    // -----------------------------------------------------------------------

    void saveState();
    bool loadState(const std::string& path);

    // -----------------------------------------------------------------------
    // Helpers
    // -----------------------------------------------------------------------

    void log(const std::string& msg);
};

} // namespace controller
