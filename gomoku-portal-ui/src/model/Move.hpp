/*
 *  Portal Gomoku UI — Move Value Object
 *  Represents a single move in the game history.
 */

#pragma once

#include "Cell.hpp"
#include "../util/Coord.hpp"

#include <string>

namespace model {

/// A single move in the game: who placed what, where, and when (ply number).
struct Move {
    util::Coord coord;          ///< Board position (x, y)
    Color       color;          ///< Who played this move
    int         ply = 0;        ///< 0-based ply number (0 = first move of the game)

    /// Check if this is a pass move (no stone placed).
    [[nodiscard]] bool isPass() const {
        return coord.x < 0 || coord.y < 0;
    }

    /// Create a pass move for the given color at the given ply.
    [[nodiscard]] static Move pass(Color c, int ply) {
        return {util::Coord::none(), c, ply};
    }

    /// Format as string: "B(7,7)@0" or "W(PASS)@3"
    [[nodiscard]] std::string toString() const {
        std::string s = (color == Color::Black) ? "B(" : "W(";
        if (isPass()) {
            s += "PASS";
        } else {
            s += std::to_string(coord.x) + "," + std::to_string(coord.y);
        }
        s += ")@" + std::to_string(ply);
        return s;
    }

    bool operator==(const Move& o) const {
        return coord == o.coord && color == o.color && ply == o.ply;
    }
};

}  // namespace model
