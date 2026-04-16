/*
 *  Portal Gomoku Engine — Influence Map
 *
 *  Provides a portal-aware positional evaluation layer that supplements
 *  the classical pattern evaluator. Computes per-cell "strategic value"
 *  based on how many valid 5-cell virtual lines pass through each cell,
 *  weighted exponentially by friendly/enemy stone counts.
 *
 *  Key properties:
 *    - Portal lines are naturally included via portalStep() enumeration
 *    - Walls terminate lines (same as board boundaries)
 *    - Self-intersecting portal lines are detected and rejected
 *    - Incremental update is O(1) per move via precomputed reverse lookup
 *    - Weight is capped at n=2 stones to avoid conflicting with tactical eval
 */

#pragma once

#include "../core/pos.h"
#include "../core/types.h"

#include <cstdint>
#include <vector>

class Board;

namespace Evaluation {

/// PORTAL: Influence Map for positional evaluation.
///
/// ┌───────────────────────────────────────────────────────────────────┐
/// │  VirtualLine { cells[5], nBlack, nWhite }                        │
/// │    — One valid 5-cell line on the virtual board topology          │
/// │                                                                   │
/// │  lines_[]           — All valid 5-cell virtual lines              │
/// │  cellLineIndices_[] — Reverse lookup: cell → list of line indices │
/// │  blackPot_[]        — Per-cell Black potential (sum of weights)   │
/// │  whitePot_[]        — Per-cell White potential (sum of weights)   │
/// │  runningTotal_      — Running sum of (blackPot - whitePot)        │
/// └───────────────────────────────────────────────────────────────────┘
///
/// The influence at a cell from Black's perspective:
///   net(cell) = blackPot_[cell] - whitePot_[cell]
///   importance(cell) = blackPot_[cell] + whitePot_[cell]
///
/// getScore(BLACK) = scaleFactor * runningTotal_
/// getScore(WHITE) = -getScore(BLACK)
class InfluenceMap
{
public:
    explicit InfluenceMap(int boardSize);
    ~InfluenceMap();

    /// Copy constructor for Board cloning (multi-threaded search).
    InfluenceMap(const InfluenceMap &other);
    InfluenceMap &operator=(const InfluenceMap &) = delete;

    // -----------------------------------------------------------------
    // Lifecycle

    /// Initialize line tables and influence values from current board state.
    /// Must be called after portals/walls are set up (i.e., after newGame()).
    /// Uses board.portalStep() for virtual line enumeration.
    void init(const Board &board);

    /// Incremental update when a stone is placed.
    /// @param pos  Position where the stone was placed.
    /// @param color BLACK or WHITE.
    void onMove(Pos pos, Color color);

    /// Incremental reverse update when a stone is removed (undo).
    /// @param pos  Position where the stone was removed.
    /// @param color BLACK or WHITE (the color that was removed).
    void onUndo(Pos pos, Color color);

    // -----------------------------------------------------------------
    // Queries

    /// Net influence at a cell (from Black's perspective).
    /// Positive = favors Black, Negative = favors White.
    [[nodiscard]] int32_t netInfluence(Pos pos) const
    {
        int idx = int(pos);
        if (idx < 0 || idx >= FULL_BOARD_CELL_COUNT) return 0;
        return blackPot_[idx] - whitePot_[idx];
    }

    /// Strategic importance of a cell (high = contested / valuable).
    /// Always non-negative. Useful for move ordering.
    [[nodiscard]] int32_t importance(Pos pos) const
    {
        int idx = int(pos);
        if (idx < 0 || idx >= FULL_BOARD_CELL_COUNT) return 0;
        return blackPot_[idx] + whitePot_[idx];
    }

    /// Black potential at a cell.
    [[nodiscard]] int32_t blackPotential(Pos pos) const
    {
        int idx = int(pos);
        if (idx < 0 || idx >= FULL_BOARD_CELL_COUNT) return 0;
        return blackPot_[idx];
    }

    /// White potential at a cell.
    [[nodiscard]] int32_t whitePotential(Pos pos) const
    {
        int idx = int(pos);
        if (idx < 0 || idx >= FULL_BOARD_CELL_COUNT) return 0;
        return whitePot_[idx];
    }

    /// Global positional score for the given side.
    /// Returns a Value in the engine's evaluation range.
    [[nodiscard]] Value getScore(Color self) const;

    /// Number of valid virtual lines discovered during init().
    [[nodiscard]] int lineCount() const { return numLines_; }

    /// Check if the map has been initialized.
    [[nodiscard]] bool isInitialized() const { return initialized_; }

private:
    // -----------------------------------------------------------------
    // Internal types

    /// A valid 5-cell virtual line on the board.
    /// Packed to 12 bytes for cache efficiency.
    ///
    /// ┌──────────────────────────────────────────┐
    /// │  cells[5]  : 5 × Pos (10 bytes)          │
    /// │  nBlack    : uint8_t — Black stone count  │
    /// │  nWhite    : uint8_t — White stone count  │
    /// └──────────────────────────────────────────┘
    struct VirtualLine
    {
        Pos     cells[5];
        uint8_t nBlack;
        uint8_t nWhite;
    };
    static_assert(sizeof(VirtualLine) == 12, "VirtualLine must be 12 bytes");

    // -----------------------------------------------------------------
    // Constants

    /// Length of a winning line in Gomoku.
    static constexpr int LINE_LEN = 5;

    /// Weight cap: only count up to this many stones for influence.
    /// Beyond this, classical pattern eval handles tactics.
    static constexpr int WEIGHT_CAP = 2;

    /// Exponential growth factor for line weights.
    static constexpr int GROWTH_FACTOR = 4;

    /// Weight table: WEIGHTS[n] = min(GROWTH_FACTOR^n, GROWTH_FACTOR^WEIGHT_CAP)
    ///   n=0: 1  (open territory)
    ///   n=1: 4  (one stone)
    ///   n=2: 16 (two stones — near-threat)
    ///   n=3: 16 (capped)
    ///   n=4: 16 (capped)
    static constexpr int WEIGHTS[LINE_LEN] = {1, 4, 16, 16, 16};

    /// Scale factor for converting raw influence sum to engine Value.
    /// Tunable. Start conservative so influence is a "tiebreaker."
    static constexpr float INFLUENCE_SCALE = 0.3f;

    // -----------------------------------------------------------------
    // Data

    int  boardSize_;
    int  numLines_;
    bool initialized_;

    /// All valid virtual lines discovered during init().
    std::vector<VirtualLine> lines_;

    /// Reverse lookup: for each cell, the list of line indices containing it.
    std::vector<uint16_t> cellLineIndices_[FULL_BOARD_CELL_COUNT];

    /// Per-cell accumulated potential from Black-only lines.
    int32_t blackPot_[FULL_BOARD_CELL_COUNT];

    /// Per-cell accumulated potential from White-only lines.
    int32_t whitePot_[FULL_BOARD_CELL_COUNT];

    /// Running total of sum(blackPot[c] - whitePot[c]) across ALL cells.
    /// Updated incrementally in onMove/onUndo.
    int32_t runningTotal_;

    // -----------------------------------------------------------------
    // Helpers

    /// Compute the net weight of a line from Black's perspective.
    ///   blackPot = (nWhite == 0) ? WEIGHTS[min(nBlack, CAP)] : 0
    ///   whitePot = (nBlack == 0) ? WEIGHTS[min(nWhite, CAP)] : 0
    ///   return {blackPot, whitePot}
    static std::pair<int, int> lineWeights(int nBlack, int nWhite);
};

}  // namespace Evaluation
