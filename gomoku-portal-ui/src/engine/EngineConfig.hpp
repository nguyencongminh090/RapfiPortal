/*
 *  Portal Gomoku UI — Engine Configuration
 *  Value object holding all engine-related settings.
 */

#pragma once

#include <filesystem>
#include <string>

namespace engine {

/// All configurable engine parameters.
/// Passed to EngineController::connect() and applyConfig().
struct EngineConfig {
    // --- Connection ---
    std::filesystem::path executablePath;   ///< Path to pbrain-MINT-P binary
    std::filesystem::path workingDir;       ///< Working directory for the engine (for config.toml)

    // --- Game Rules ---
    int  boardSize   = 15;                  ///< Board size (5..22)
    int  rule        = 0;                   ///< 0=Freestyle, 1=Standard, 2/4=Renju, 5=Swap1, 6=Swap2

    // --- Time Control ---
    int  timeoutTurn  = 5000;               ///< Per-move time limit in ms
    int  timeoutMatch = 180000;             ///< Match total time limit in ms

    // --- Resources ---
    size_t maxMemoryBytes = 209715200;      ///< Engine memory limit in BYTES (default 200 MB)
    int    threadNum      = 0;              ///< 0 = auto (use all hardware threads)

    // --- Search ---
    int  strength  = 100;                   ///< Skill level 0..100
    int  maxDepth  = 99;                    ///< Max search depth
    bool pondering = false;                 ///< Enable pondering (think on opponent's time)

};

}  // namespace engine
