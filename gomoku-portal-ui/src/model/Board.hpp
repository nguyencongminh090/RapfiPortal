/*
 *  Portal Gomoku UI — Board Model
 *  Lightweight client-side board mirror.
 *  Tracks cell state, move history, and portal topology for UI rendering.
 *  Does NOT duplicate engine's pattern detection — only tracks what's visible.
 */

#pragma once

#include "Cell.hpp"
#include "Move.hpp"
#include "PortalTopology.hpp"
#include "../util/Coord.hpp"

#include <optional>
#include <stdexcept>
#include <vector>

namespace model {

/// Client-side board state for rendering and game recording.
///
/// Responsibilities:
///   - Track cell contents (stones, walls, portals)
///   - Maintain move history with undo support
///   - Store portal topology for rendering
///   - Provide data for serialization (GameRecord)
///
/// NOT responsible for:
///   - Pattern detection (engine handles this)
///   - Move legality beyond basic checks (empty cell, in-bounds)
///   - Search or evaluation
class Board {
public:
    /// Construct an empty board of the given size.
    explicit Board(int size = 15);

    // =========================================================================
    // Mutators
    // =========================================================================

    /// Reset the board to empty with a new size. Clears history and topology.
    void reset(int size);

    /// Reset the board to empty, keeping the same size and topology.
    void resetKeepTopology();

    /// Place a stone at (x, y) for the given color.
    /// Returns false if the cell is not empty or out of bounds.
    bool placeStone(int x, int y, Color color);

    /// Place a stone using a Coord.
    bool placeStone(util::Coord pos, Color color);

    /// Record a pass move (no stone placed, turn passes).
    void pass(Color color);

    /// Undo the last move. Returns the undone Move.
    /// Throws if history is empty.
    Move undoLast();

    /// Redo the previously undone move (if no new moves were played).
    bool redoMove();

    /// Check if redo is possible.
    [[nodiscard]] bool canRedo() const { return !redoStack_.empty(); }

    /// Set the portal topology (walls + portal pairs).
    /// Marks cells as Wall/PortalA/PortalB on the board grid.
    void setTopology(const PortalTopology& topo);

    /// Clear all topology (walls + portals). Removes them from the grid.
    void clearTopology();

    // =========================================================================
    // Queries
    // =========================================================================

    /// Board size (N for NxN).
    [[nodiscard]] int size() const { return size_; }

    /// Current ply count (number of moves played).
    [[nodiscard]] int ply() const { return static_cast<int>(history_.size()); }

    /// Get the cell state at (x, y).
    [[nodiscard]] Cell cellAt(int x, int y) const;

    /// Get the cell state at a Coord.
    [[nodiscard]] Cell cellAt(util::Coord pos) const;

    /// Check if (x, y) is within the board.
    [[nodiscard]] bool inBounds(int x, int y) const;

    /// Check if (x, y) is empty.
    [[nodiscard]] bool isEmpty(int x, int y) const;

    /// Check if (x, y) is a wall.
    [[nodiscard]] bool isWall(int x, int y) const;

    /// Check if (x, y) is a portal endpoint.
    [[nodiscard]] bool isPortal(int x, int y) const;

    /// Get the portal partner of the cell at (x, y).
    /// Returns nullopt if not a portal.
    [[nodiscard]] std::optional<util::Coord> portalPartner(int x, int y) const;

    /// Get the last move played. Throws if history is empty.
    [[nodiscard]] const Move& lastMove() const;

    /// Full move history (oldest first).
    [[nodiscard]] const std::vector<Move>& history() const { return history_; }

    /// Current portal topology.
    [[nodiscard]] const PortalTopology& topology() const { return topology_; }

    /// Whose turn is it? (Based on ply count: even=Black, odd=White).
    [[nodiscard]] Color sideToMove() const;

    /// Count stones of a given color on the board.
    [[nodiscard]] int stoneCount(Color color) const;

    /// Count all stones on the board.
    [[nodiscard]] int totalStones() const;

    /// Count empty cells.
    [[nodiscard]] int emptyCells() const;

    /// Check if the board is completely full.
    [[nodiscard]] bool isBoardFull() const { return emptyCells() == 0; }

    /// Check if a move is legal (in bounds and empty).
    [[nodiscard]] bool isLegalMove(util::Coord pos) const { return isEmpty(pos.x, pos.y); }

private:
    int                    size_;
    std::vector<Cell>      cells_;     ///< size_*size_, row-major
    std::vector<Move>      history_;
    std::vector<Move>      redoStack_; ///< Cleared on new move
    PortalTopology         topology_;

    /// Convert (x, y) to 1D index.
    [[nodiscard]] int idx(int x, int y) const { return y * size_ + x; }

    /// Apply topology to the cell grid (mark walls/portals).
    void applyTopologyToGrid();

    /// Remove topology markings from the cell grid (walls/portals → Empty).
    void clearTopologyFromGrid();

    /// Push a pass move to history WITHOUT clearing redoStack_.
    /// Used only by redoMove() to preserve remaining redo entries.
    /// For new user actions, use the public pass() which does clear redoStack_.
    void passInternal(Color color, int ply);
};

}  // namespace model
