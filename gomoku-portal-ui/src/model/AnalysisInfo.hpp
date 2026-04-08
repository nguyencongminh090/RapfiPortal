/*
 *  Portal Gomoku UI — Analysis Info
 *  Value objects for engine search information and N-best results.
 */

#pragma once

#include "../util/Coord.hpp"

#include <string>
#include <vector>

namespace model {

/// A single move in an N-best analysis result.
struct AnalysisMove {
    util::Coord coord;          ///< Move position
    int         rank  = 0;      ///< 1-based rank in N-best list
    int         score = 0;      ///< Evaluation score (centipawns-like)
    int         depth = 0;      ///< Search depth
    std::vector<util::Coord> pv; ///< Principal variation (sequence of moves)
    std::string pvText;         ///< Raw PV string from engine MESSAGE
};

/// Aggregate search information from engine MESSAGE output.
struct AnalysisInfo {
    int         depth    = 0;       ///< Current search depth
    int         score    = 0;       ///< Best score
    int         nodes    = 0;       ///< Nodes searched
    int         nps      = 0;       ///< Nodes per second
    int         timeMs   = 0;       ///< Search time in milliseconds
    std::string bestMove;           ///< Best move string
    std::vector<AnalysisMove> nBest; ///< N-best move list (empty if single-PV)
};

}  // namespace model
