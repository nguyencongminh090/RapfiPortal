/*
 *  Portal Gomoku Engine — Influence Map Implementation
 *
 *  This file implements the Portal-Aware Influence Map.
 *
 *  Algorithm overview:
 *    1. init(): Enumerate ALL valid 5-cell virtual lines using portalStep().
 *       - Start from each playable cell, walk 4 steps in each of 4 directions.
 *       - Reject lines with < 5 cells, duplicate cells (portal loops), or walls.
 *       - Build reverse lookup: cell → list of line indices.
 *       - Compute initial weights from current board state.
 *
 *    2. onMove(): For each line containing the placed cell:
 *       - Compute old weight, update stone count, compute new weight.
 *       - Apply delta to blackPot_/whitePot_ for all 5 cells in the line.
 *       - Update runningTotal_.
 *
 *    3. onUndo(): Exact reverse of onMove().
 */

#include "influence.h"

#include "../game/board.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace Evaluation {

// =====================================================================
// Construction / Destruction
// =====================================================================

InfluenceMap::InfluenceMap(int boardSize)
    : boardSize_(boardSize)
    , numLines_(0)
    , initialized_(false)
    , runningTotal_(0)
{
    std::memset(blackPot_, 0, sizeof(blackPot_));
    std::memset(whitePot_, 0, sizeof(whitePot_));
}

InfluenceMap::~InfluenceMap() = default;

InfluenceMap::InfluenceMap(const InfluenceMap &other)
    : boardSize_(other.boardSize_)
    , numLines_(other.numLines_)
    , initialized_(other.initialized_)
    , lines_(other.lines_)
    , runningTotal_(other.runningTotal_)
{
    std::memcpy(blackPot_, other.blackPot_, sizeof(blackPot_));
    std::memcpy(whitePot_, other.whitePot_, sizeof(whitePot_));

    // Copy reverse lookup vectors
    for (int i = 0; i < FULL_BOARD_CELL_COUNT; i++)
        cellLineIndices_[i] = other.cellLineIndices_[i];
}

// =====================================================================
// Line Weight Computation
// =====================================================================

std::pair<int, int> InfluenceMap::lineWeights(int nBlack, int nWhite)
{
    // Dead line: both colors present → no potential for either side
    // Pure Black line (nWhite == 0): Black has potential
    // Pure White line (nBlack == 0): White has potential
    // Empty line (both 0): both have potential (equal, cancels in net)
    int bp = (nWhite == 0) ? WEIGHTS[std::min(nBlack, WEIGHT_CAP)] : 0;
    int wp = (nBlack == 0) ? WEIGHTS[std::min(nWhite, WEIGHT_CAP)] : 0;
    return {bp, wp};
}

// =====================================================================
// Initialization — Virtual Line Enumeration
// =====================================================================

void InfluenceMap::init(const Board &board)
{
    // Reset state
    lines_.clear();
    numLines_ = 0;
    runningTotal_ = 0;
    std::memset(blackPot_, 0, sizeof(blackPot_));
    std::memset(whitePot_, 0, sizeof(whitePot_));
    for (int i = 0; i < FULL_BOARD_CELL_COUNT; i++)
        cellLineIndices_[i].clear();

    // Reserve estimated capacity (4 dirs × boardSize² ≈ 900 for 15×15)
    lines_.reserve(boardSize_ * boardSize_ * 4);

    // Enumerate all valid 5-cell virtual lines.
    //
    // For each playable cell c and each direction d:
    //   Walk 4 steps in direction +d using portalStep().
    //   If all 5 cells are valid and unique, register the line.
    //
    // A cell is "playable" if it's not a non-portal WALL:
    //   - EMPTY, BLACK, WHITE are all playable
    //   - Portal cells (piece=WALL but isPortalCell) are NOT playable
    //     (portals are zero-width; they can't hold stones)
    //   - Non-portal WALLs are not playable

    for (int y = 0; y < boardSize_; y++) {
        for (int x = 0; x < boardSize_; x++) {
            Pos startPos(x, y);

            // Skip non-playable cells (any kind of WALL)
            if (board.get(startPos) == WALL)
                continue;

            for (int dir = 0; dir < 4; dir++) {
                Pos cells[LINE_LEN];
                cells[0] = startPos;
                bool valid = true;

                // Walk LINE_LEN - 1 more steps
                Pos cur = startPos;
                for (int step = 1; step < LINE_LEN; step++) {
                    Pos next = board.portalStep(cur, dir, +1);

                    // Check bounds
                    if (int(next) < 0 || int(next) >= FULL_BOARD_CELL_COUNT) {
                        valid = false;
                        break;
                    }

                    // Check non-portal WALL (terminates the line)
                    if (board.get(next) == WALL && !board.isPortalCell(next)) {
                        valid = false;
                        break;
                    }

                    // Portal cell itself should not appear as a line cell
                    // (portalStep already skips portals, but be defensive)
                    if (board.isPortalCell(next)) {
                        valid = false;
                        break;
                    }

                    // Check for self-intersection (portal loop detection)
                    for (int j = 0; j < step; j++) {
                        if (cells[j] == next) {
                            valid = false;
                            break;
                        }
                    }
                    if (!valid)
                        break;

                    cells[step] = next;
                    cur = next;
                }

                if (!valid)
                    continue;

                // All 5 cells are valid and unique — register this line
                VirtualLine line;
                line.nBlack = 0;
                line.nWhite = 0;
                for (int i = 0; i < LINE_LEN; i++) {
                    line.cells[i] = cells[i];
                    Color piece = board.get(cells[i]);
                    if (piece == BLACK)
                        line.nBlack++;
                    else if (piece == WHITE)
                        line.nWhite++;
                }

                uint16_t lineIdx = static_cast<uint16_t>(lines_.size());
                lines_.push_back(line);

                // Register in reverse lookup for each cell
                for (int i = 0; i < LINE_LEN; i++) {
                    cellLineIndices_[int(cells[i])].push_back(lineIdx);
                }
            }
        }
    }

    numLines_ = static_cast<int>(lines_.size());

    // Compute initial influence from current board state
    for (int li = 0; li < numLines_; li++) {
        const VirtualLine &line = lines_[li];
        auto [bp, wp] = lineWeights(line.nBlack, line.nWhite);

        // Add weight to every cell in the line
        for (int i = 0; i < LINE_LEN; i++) {
            int cellIdx = int(line.cells[i]);
            blackPot_[cellIdx] += bp;
            whitePot_[cellIdx] += wp;
        }

        // Update running total: delta = (bp - wp) applied to 5 cells
        runningTotal_ += (bp - wp) * LINE_LEN;
    }

    initialized_ = true;
}

// =====================================================================
// Incremental Update — O(1) per move
// =====================================================================

void InfluenceMap::onMove(Pos pos, Color color)
{
    if (!initialized_)
        return;

    int cellIdx = int(pos);
    if (cellIdx < 0 || cellIdx >= FULL_BOARD_CELL_COUNT)
        return;

    const auto &lineRefs = cellLineIndices_[cellIdx];

    for (uint16_t li : lineRefs) {
        VirtualLine &line = lines_[li];

        // Compute old weights
        auto [oldBP, oldWP] = lineWeights(line.nBlack, line.nWhite);

        // Update stone count
        if (color == BLACK)
            line.nBlack++;
        else
            line.nWhite++;

        // Compute new weights
        auto [newBP, newWP] = lineWeights(line.nBlack, line.nWhite);

        int deltaBP = newBP - oldBP;
        int deltaWP = newWP - oldWP;

        if (deltaBP == 0 && deltaWP == 0)
            continue;

        // Apply delta to all 5 cells in the line
        for (int i = 0; i < LINE_LEN; i++) {
            int ci = int(line.cells[i]);
            blackPot_[ci] += deltaBP;
            whitePot_[ci] += deltaWP;
        }

        // Update running total
        runningTotal_ += (deltaBP - deltaWP) * LINE_LEN;
    }
}

void InfluenceMap::onUndo(Pos pos, Color color)
{
    if (!initialized_)
        return;

    int cellIdx = int(pos);
    if (cellIdx < 0 || cellIdx >= FULL_BOARD_CELL_COUNT)
        return;

    const auto &lineRefs = cellLineIndices_[cellIdx];

    for (uint16_t li : lineRefs) {
        VirtualLine &line = lines_[li];

        // Compute old weights (before undo)
        auto [oldBP, oldWP] = lineWeights(line.nBlack, line.nWhite);

        // Reverse stone count
        if (color == BLACK)
            line.nBlack--;
        else
            line.nWhite--;

        // Compute new weights (after undo)
        auto [newBP, newWP] = lineWeights(line.nBlack, line.nWhite);

        int deltaBP = newBP - oldBP;
        int deltaWP = newWP - oldWP;

        if (deltaBP == 0 && deltaWP == 0)
            continue;

        for (int i = 0; i < LINE_LEN; i++) {
            int ci = int(line.cells[i]);
            blackPot_[ci] += deltaBP;
            whitePot_[ci] += deltaWP;
        }

        runningTotal_ += (deltaBP - deltaWP) * LINE_LEN;
    }
}

// =====================================================================
// Score Query
// =====================================================================

Value InfluenceMap::getScore(Color self) const
{
    if (!initialized_)
        return VALUE_ZERO;

    // runningTotal_ is the sum of (blackPot - whitePot) across ALL cells.
    // Positive = Black advantage, Negative = White advantage.
    float raw = INFLUENCE_SCALE * static_cast<float>(runningTotal_);

    // Clamp to eval range
    int score = static_cast<int>(raw);
    score = std::clamp(score, int(VALUE_EVAL_MIN), int(VALUE_EVAL_MAX));

    return (self == BLACK) ? Value(score) : Value(-score);
}

}  // namespace Evaluation
