/*
 *  Portal Gomoku UI — Game Record
 *  Serializable game state: metadata + move list + portal topology.
 *  Used for save/load and engine BOARD command generation.
 */

#pragma once

#include "Cell.hpp"
#include "Move.hpp"
#include "PortalTopology.hpp"
#include "../util/Coord.hpp"

#include <string>
#include <tuple>
#include <vector>

namespace model {

/// A complete game record: everything needed to reconstruct a game position.
struct GameRecord {
    // --- Metadata ---
    int         boardSize = 15;
    int         rule      = 0;          ///< 0=Freestyle, 1=Standard, 4=Renju, etc.
    std::string event;                  ///< Event name (optional)
    std::string black;                  ///< Black player name
    std::string white;                  ///< White player name
    std::string date;                   ///< Date string (YYYY-MM-DD)
    std::string result;                 ///< Result string: "B+", "W+", "Draw", ""

    // --- Board State ---
    PortalTopology          topology;   ///< Walls + portal pairs
    std::vector<Move>       moves;      ///< Move history (in order)

    // =========================================================================
    // Builders
    // =========================================================================

    /// Build a BOARD protocol entry list from the current state.
    /// Returns {x, y, color} tuples where color: 1=BLACK, 2=WHITE, 3=WALL.
    /// selfColor determines which color maps to 1(SELF) vs 2(OPPO).
    [[nodiscard]] std::vector<std::tuple<int,int,int>>
    toBoardEntries(Color selfColor = Color::Black) const {
        std::vector<std::tuple<int,int,int>> entries;

        // Add walls (color=3)
        for (auto& w : topology.walls())
            entries.emplace_back(w.x, w.y, 3);

        // Add moves (color=1 for self, 2 for opponent)
        for (auto& m : moves) {
            if (m.isPass()) continue;
            int color = (m.color == selfColor) ? 1 : 2;
            entries.emplace_back(m.coord.x, m.coord.y, color);
        }

        return entries;
    }

    /// Create a GameRecord from a Board's current state.
    [[nodiscard]] static GameRecord fromBoard(const class Board& board);
};

}  // namespace model
