/*
 *  Portal Gomoku UI — Coordinate Utilities
 *  Board coordinate ↔ protocol coordinate conversion.
 */

#pragma once

#include <utility>

namespace util {

/// Board coordinate representation.
/// (0,0) = top-left corner. x = column, y = row.
struct Coord {
    int x = -1;
    int y = -1;

    [[nodiscard]] bool isValid(int boardSize) const {
        return x >= 0 && x < boardSize && y >= 0 && y < boardSize;
    }

    bool operator==(const Coord& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Coord& o) const { return !(*this == o); }
    bool operator<(const Coord& o) const {
        return (y != o.y) ? y < o.y : x < o.x;
    }

    /// Convert board coord to 1D index (row-major).
    [[nodiscard]] int toIndex(int boardSize) const { return y * boardSize + x; }

    /// Create from 1D index (row-major).
    [[nodiscard]] static Coord fromIndex(int idx, int boardSize) {
        return {idx % boardSize, idx / boardSize};
    }

    /// Invalid sentinel coord.
    [[nodiscard]] static constexpr Coord none() { return {-1, -1}; }
};

/// Portal pair: two coordinates linked as a bidirectional teleporter.
struct PortalPair {
    Coord a;
    Coord b;
};

}  // namespace util
