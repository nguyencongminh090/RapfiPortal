/*
 *  Portal Gomoku UI — Cell Type
 *  Represents the state of a single board cell.
 */

#pragma once

#include <cstdint>

namespace model {

/// State of a single cell on the board.
enum class Cell : uint8_t {
    Empty   = 0,    ///< No piece, no obstacle
    Black   = 1,    ///< Black stone
    White   = 2,    ///< White stone
    Wall    = 3,    ///< Immovable WALL cell (blocks lines)
    PortalA = 4,    ///< Portal endpoint A (rendered distinctly from B)
    PortalB = 5     ///< Portal endpoint B
};

/// The two player colors (subset of Cell).
enum class Color : uint8_t {
    Black = 1,
    White = 2
};

/// Convert Color to its Cell equivalent.
[[nodiscard]] constexpr Cell colorToCell(Color c) {
    return static_cast<Cell>(static_cast<uint8_t>(c));
}

/// Flip color: Black ↔ White.
[[nodiscard]] constexpr Color opponent(Color c) {
    return (c == Color::Black) ? Color::White : Color::Black;
}

/// Check if a cell contains a stone (Black or White).
[[nodiscard]] constexpr bool isStone(Cell c) {
    return c == Cell::Black || c == Cell::White;
}

/// Check if a cell is a portal endpoint (A or B).
[[nodiscard]] constexpr bool isPortal(Cell c) {
    return c == Cell::PortalA || c == Cell::PortalB;
}

/// Check if a cell is an obstacle (Wall or Portal — cannot place stone).
[[nodiscard]] constexpr bool isObstacle(Cell c) {
    return c == Cell::Wall || isPortal(c);
}

/// Convert Cell to protocol color code: 1=Black(SELF), 2=White(OPPO), 3=Wall.
/// Returns 0 for Empty/Portal (not representable in protocol).
[[nodiscard]] constexpr int cellToProtocolColor(Cell c) {
    switch (c) {
    case Cell::Black: return 1;
    case Cell::White: return 2;
    case Cell::Wall:  return 3;
    default:          return 0;
    }
}

}  // namespace model
