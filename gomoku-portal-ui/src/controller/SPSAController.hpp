/*
 *  Portal Gomoku UI — SPSA Tuning Controller (Concurrent)
 *  Implements Simultaneous Perturbation Stochastic Approximation
 *  for automated engine parameter optimization via self-play.
 *
 *  Architecture:
 *    - N concurrent GameSlots, each owning 2 engines + 1 board.
 *    - Work-queue scheduling: slots grab games from a shared queue.
 *    - Optional Spectate mode: pipe one slot's board to the main UI.
 *    - Validation matches (θ vs baseline) reuse the same slot pool.
 *    - State persisted to JSON after every iteration for stop/continue.
 *
 *  Algorithm reference: Spall, J.C. (1998)
 */

#pragma once

#include "../engine/EngineController.hpp"
#include "../model/Board.hpp"
#include "../model/OBFManager.hpp"

#include <sigc++/sigc++.h>
#include <string>
#include <vector>
#include <queue>
#include <random>
#include <mutex>
#include <atomic>
#include <memory>
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
    double      a = 0.0;    ///< Parameter-specific learning rate base
    double      c = 0.0;    ///< Parameter-specific perturbation base
};

/// SPSA gain sequence configuration.
struct SPSAConfig {
    std::string enginePath;         ///< Path to the tuning engine binary
    std::string baselinePath;       ///< Path to the baseline engine binary (for validation)
    std::string obfPath;            ///< Path to opening book file
    std::string statePath;          ///< Path to save/load state JSON
    std::string paramsConfigPath;   ///< Path to CSV with initial parameter definitions

    // Gain sequence constants (Spall's recommended defaults)
    double a     = 1.0;            ///< Learning rate base
    double c     = 2.0;            ///< Perturbation magnitude base
    double alpha = 0.602;          ///< a_k decay exponent
    double gamma = 0.101;          ///< c_k decay exponent
    double A     = 10.0;           ///< Stabilization constant

    // Match settings
    int gamesPerIteration = 20;    ///< Total games per SPSA iteration
    int turnTimeMs        = 5000;  ///< Per-move time limit (info timeout_turn)
    int matchTimeMs       = 0;     ///< Per-game total time (info timeout_match)
    int threads           = 1;     ///< Threads per engine
    int64_t maxMemory     = 350LL * 1024 * 1024; ///< Memory per engine
    int boardSize         = 15;    ///< Board size for games
    int rule              = 0;     ///< Game rule

    // Concurrency
    int concurrency       = 1;     ///< Number of concurrent game slots

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
// GameSlot — self-contained game runner
// ============================================================================

/// A single concurrent game slot owning 2 engines and 1 board.
struct GameSlot {
    int                        id = 0;
    engine::EngineController   enginePlus;   ///< θ+ engine (or tuned in validation)
    engine::EngineController   engineMinus;  ///< θ- engine (or baseline in validation)
    model::Board               board{15};
    bool                       isPlusTurn = false;
    int                        gameIndex = -1;   ///< Which game# from the queue
    bool                       busy = false;
    int                        openingIdx = 0;
    int                        gameInOpening = 0;
    std::chrono::time_point<std::chrono::steady_clock> turnStartTime;
    int                        plusTimeLeftMs = 0;
    int                        minusTimeLeftMs = 0;
    float                      lastEvalWR = 50.0f;

    // Signal connections for this slot
    std::vector<sigc::connection> connections;
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
    void start(const SPSAConfig& config);

    /// Gracefully stop after the current games finish.
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

    /// Progress: how many games completed / total in current iteration
    [[nodiscard]] int gamesCompleted() const { return gamesCompleted_; }
    [[nodiscard]] int gamesTotalCurrent() const { return gamesTotalCurrent_; }
    [[nodiscard]] int activeSlotsCount() const;
    [[nodiscard]] double currentScorePlus() const { return matchScorePlus_; }
    [[nodiscard]] double currentScoreMinus() const { return matchScoreMinus_; }
    [[nodiscard]] double currentDraws() const { return matchDraws_; }

    // -----------------------------------------------------------------------
    // Spectate
    // -----------------------------------------------------------------------

    /// Enable spectating: pipe one slot's board to signalBoardUpdated.
    void setSpectating(bool enabled) { spectating_ = enabled; }
    [[nodiscard]] bool isSpectating() const { return spectating_; }

    // -----------------------------------------------------------------------
    // Signals
    // -----------------------------------------------------------------------

    sigc::signal<void()>                     signalStateChanged;
    sigc::signal<void(const std::string&)>   signalLogMessage;
    sigc::signal<void()>                     signalIterationComplete;
    sigc::signal<void()>                     signalParamsUpdated;
    sigc::signal<void()>                     signalValidationComplete;
    sigc::signal<void()>                     signalProgressUpdated;

    // Game observation signals (only emitted when spectating)
    sigc::signal<void(const model::Board&)>  signalBoardUpdated;
    sigc::signal<void(int, int, int)>        signalMoveMade; // x, y, color
    sigc::signal<void(const std::string&, const std::string&)> signalMatchInfo;

private:
    SPSAState   state_ = SPSAState::Idle;
    SPSAConfig  config_;
    int         iteration_ = 0;
    bool        stopRequested_ = false;
    bool        spectating_ = false;
    int         spectateSlot_ = 0;   ///< Which slot we are spectating

    // Parameters being tuned
    std::vector<SPSAParam> params_;

    // Current perturbation vector (±1 per parameter)
    std::vector<int> delta_;

    // Iteration history
    std::vector<SPSAIterationResult> history_;

    // Validation history
    std::vector<SPSAValidationResult> valHistory_;

    // -----------------------------------------------------------------------
    // Concurrent game slots
    // -----------------------------------------------------------------------

    static constexpr int MAX_SLOTS = 256;
    std::vector<std::unique_ptr<GameSlot>> slots_;

    // Work queue: indices of games still to be played
    std::queue<int> pendingGames_;

    // Queue for staggered engine start to avoid OS thread exhaustion
    std::queue<int> pendingSlotStarts_;
    bool onSlotStartTimeout();

    // Aggregated match results (across all slots)
    int matchScorePlus_  = 0;
    int matchScoreMinus_ = 0;
    int matchDraws_      = 0;
    int gamesCompleted_  = 0;
    int gamesTotalCurrent_ = 0;

    // Opening assignment: track which opening goes with which game index
    // Opening index = (gameIndex / 2) % openings_.size()
    // Game-in-opening = gameIndex % 2

    // Opening book
    std::vector<model::GameRecord> openings_;

    // RNG for perturbation
    std::mt19937 rng_;

    // -----------------------------------------------------------------------
    // Gain sequences
    // -----------------------------------------------------------------------

    // SPSA Mathematics
    double a_k(double base_a) const;
    double c_k(double base_c) const;
    void initDefaultParams();
    void loadParamsConfig(const std::string& path);
    void generatePerturbation();
    void applyPerturbedParams(GameSlot& slot);
    void applyCurrentParams(engine::EngineController& engine);

    // -----------------------------------------------------------------------
    // Slot management
    // -----------------------------------------------------------------------

    /// Create and wire up N game slots.
    void initSlots(int count);

    /// Destroy all slots and their engines.
    void destroySlots();

    /// Connect engines in a slot and send START.
    bool connectSlotEngines(GameSlot& slot, const std::string& plusPath,
                            const std::string& minusPath);

    /// Dispatch the next pending game to a specific slot.
    void dispatchToSlot(int slotId);

    /// Handle a move from a slot's engine.
    void onSlotMove(int slotId, bool fromPlus, int x, int y);
    void onSlotError(int slotId, bool fromPlus, const std::string& err);
    void onSlotMessage(int slotId, bool fromPlus, const std::string& msg);

    /// End a game on a specific slot. result: 1=plus wins, 2=minus wins, 0=draw
    void endSlotGame(int slotId, int result);

    /// Send START + position to a slot's engine.
    void sendStartToSlotEngine(engine::EngineController& engine,
                               const model::GameRecord& rec);

    // -----------------------------------------------------------------------
    // Iteration management
    // -----------------------------------------------------------------------

    void startIterationMatches();
    void onAllGamesFinished();
    void finalizeIteration();

    // -----------------------------------------------------------------------
    // Validation (current θ vs baseline)
    // -----------------------------------------------------------------------

    [[nodiscard]] bool shouldValidate() const;
    void startValidationMatch();
    void onAllValidationGamesFinished();
    void finalizeValidation();

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
