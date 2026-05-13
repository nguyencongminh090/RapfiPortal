/*
 *  Rapfi, a Gomoku/Renju playing engine supporting piskvork protocol.
 *  Copyright (C) 2022  Rapfi developers
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "eval.h"

#include "../game/board.h"
#include "../search/searchthread.h"
#include "evaluator.h"

#include <algorithm>
#include <cmath>

namespace {

/// Makes threat mask according current pattern4 counts on board.
int makeThreatMask(const StateInfo &st, Color self)
{
    Color oppo = ~self;

    bool oppoFive      = st.p4Count[oppo][A_FIVE];
    bool selfFlexFour  = st.p4Count[self][B_FLEX4];
    bool oppoFlexFour  = st.p4Count[oppo][B_FLEX4];
    bool selfFourPlus  = st.p4Count[self][D_BLOCK4_PLUS] + st.p4Count[self][C_BLOCK4_FLEX3];
    bool selfFour      = st.p4Count[self][E_BLOCK4];
    bool selfThreePlus = st.p4Count[self][G_FLEX3_PLUS] + st.p4Count[self][F_FLEX3_2X];
    bool selfThree     = st.p4Count[self][H_FLEX3];
    bool oppoFourPlus  = st.p4Count[oppo][D_BLOCK4_PLUS] + st.p4Count[oppo][C_BLOCK4_FLEX3];
    bool oppoFour      = st.p4Count[oppo][E_BLOCK4];
    bool oppoThreePlus = st.p4Count[oppo][G_FLEX3_PLUS] + st.p4Count[oppo][F_FLEX3_2X];
    bool oppoThree     = st.p4Count[oppo][H_FLEX3];

    int mask = 0;
    mask |= 0b1 & -int(oppoFive);
    mask |= 0b10 & -int(selfFlexFour);
    mask |= 0b100 & -int(oppoFlexFour);
    mask |= 0b1000 & -int(selfFourPlus);
    mask |= 0b10000 & -int(selfFour);
    mask |= 0b100000 & -int(selfThreePlus);
    mask |= 0b1000000 & -int(selfThree);
    mask |= 0b10000000 & -int(oppoFourPlus);
    mask |= 0b100000000 & -int(oppoFour);
    mask |= 0b1000000000 & -int(oppoThreePlus);
    mask |= 0b10000000000 & -int(oppoThree);

    assert(0 <= mask && mask < THREAT_NB);
    return mask;
}

/// Evaluates threats.
/// Threat indicates dynamic first-move status which causes highly non-linear eval changes.
template <Rule R>
inline Value evaluateThreat(const StateInfo &st, Color self)
{
    return (Value)Config::EVALS_THREAT[Config::tableIndex(R, self)][makeThreatMask(st, self)];
}

/// Evaluates basic patterns on board.
inline Value evaluateBasic(const StateInfo &st, Color self)
{
    return self == BLACK ? st.valueBlack : -st.valueBlack;
}

/// Finds a margin for switching to classical evaluation if
/// it falls outside alpha-beta window with this margin.
inline int classicalEvalMargin(Value bound)
{
    float winLossRate = 2 * (Config::valueToWinRate(bound) - 0.5f);
    float x           = Config::EvaluatorMarginWinLossScale * winLossRate;
    float x2          = x * x;
    return (int)(Config::EvaluatorMarginScale
                 * ::expf(-::powf(x2, Config::EvaluatorMarginWinLossExponent)));
}

}  // namespace

namespace {

// =============================================================================
// PORTAL: WALL Board Strategic Correction
// =============================================================================

/// Flood-fill to find the size (in cells) of the connected region around `start`
/// bounded by WALL cells, portal cells, and board edges. Uses the scratch `visited` array.
/// Returns the number of empty+stone cells reachable (WALL cells themselves not counted).
static int regionSize(const Board &board,
                      Pos          start,
                      bool (&visited)[FULL_BOARD_CELL_COUNT])
{
    // BFS with a small inline stack (no heap allocation in search-hot code)
    Pos   stack[FULL_BOARD_CELL_COUNT];
    int   top  = 0;
    int   size = 0;
    int   bs   = board.size();

    stack[top++] = start;
    visited[int(start)] = true;
    while (top > 0) {
        Pos cur = stack[--top];
        size++;
        // Check 4 orthogonal neighbors
        static constexpr int DX[4] = {0, 0, 1, -1};
        static constexpr int DY[4] = {1, -1, 0, 0};
        for (int d = 0; d < 4; d++) {
            Pos nb = Pos(cur.x() + DX[d], cur.y() + DY[d]);
            if (!nb.isInBoard(bs, bs)) continue;
            if (visited[int(nb)]) continue;
            // WALL cells and portal cells are region boundaries
            if (board.cell(nb).piece == WALL) continue;
            visited[int(nb)] = true;
            stack[top++] = nb;
        }
    }
    return size;
}

/// Returns true if a region of the given size can possibly contain a 5-in-a-row.
/// A region is "dead" if it is < 5 cells in every straight-line dimension.
/// Approximation: a region with fewer than 5 cells total cannot contain a win.
inline bool regionCanWin(int regionSz)
{
    return regionSz >= 5;
}

/// [PORTAL: WALL CORRECTION A] Dead Pocket Penalty
/// Flood-fill all regions. Regions with < 5 cells are dead pockets.
/// Count stones of each color trapped inside and return (selfDead, oppoDead).
static std::pair<int,int> countDeadPocketStones(const Board &board, Color self)
{
    static thread_local bool visited[FULL_BOARD_CELL_COUNT];
    std::fill_n(visited, board.cellCount(), false);

    int bs = board.size();
    int selfDead = 0, oppoDead = 0;

    for (int i = 0; i < board.cellCount(); i++) {
        Pos pos(i);
        if (visited[i]) continue;
        if (board.cell(pos).piece == WALL) { visited[i] = true; continue; }

        // BFS to collect region cells and count stones
        Pos   stack[FULL_BOARD_CELL_COUNT];
        int   top   = 0;
        int   sz    = 0;
        int   selfN = 0, oppoN = 0;

        stack[top++]  = pos;
        visited[i]    = true;
        while (top > 0) {
            Pos cur = stack[--top];
            sz++;
            Color piece = board.cell(cur).piece;
            if (piece == self)        selfN++;
            else if (piece == ~self)  oppoN++;

            static constexpr int DX[4] = {0, 0, 1, -1};
            static constexpr int DY[4] = {1, -1, 0, 0};
            for (int d = 0; d < 4; d++) {
                Pos nb = Pos(cur.x() + DX[d], cur.y() + DY[d]);
                if (!nb.isInBoard(bs, bs)) continue;
                if (visited[int(nb)]) continue;
                if (board.cell(nb).piece == WALL) continue;
                visited[int(nb)] = true;
                stack[top++] = nb;
            }
        }

        if (!regionCanWin(sz)) {
            // All stones in this dead pocket are useless
            selfDead += selfN;
            oppoDead += oppoN;
        }
    }
    return {selfDead, oppoDead};
}

/// [PORTAL: WALL CORRECTION B] Corridor Squeeze Bonus
/// Scan each row/column/diagonal for segments bounded by WALL/edge with exactly 5 playable cells.
/// If `side` has a B4 (open four) pattern in such a segment, it is unstoppable.
/// Returns the count of such unstoppable B4s for `side`.
static int countCorridorFours(const Board &board, Color side)
{
    int count = 0;
    int bs    = board.size();

    // Directions: H, V, diagonal \, diagonal /
    static constexpr int DX[4] = {1, 0, 1,  1};
    static constexpr int DY[4] = {0, 1, 1, -1};

    for (int dir = 0; dir < 4; dir++) {
        // Enumerate all line starts (cells where the line leaves the board if going backward)
        for (int r = 0; r < bs; r++) {
            for (int c = 0; c < bs; c++) {
                // Only start at line origins to avoid double-counting
                int pr = r - DX[dir], pc = c - DY[dir];
                if (Pos(pr, pc).isInBoard(bs, bs)
                    && board.cell(Pos(pr, pc)).piece != WALL)
                    continue;  // not a line start

                // Walk forward, collecting segments between WALLs/edges
                int segLen = 0;
                bool hasBFour = false;
                int x = r, y = c;
                while (Pos(x, y).isInBoard(bs, bs)) {
                    Pos cur(x, y);
                    if (board.cell(cur).piece == WALL) {
                        // Segment ended at WALL — check if exactly 5
                        if (segLen == 5 && hasBFour)
                            count++;
                        segLen  = 0;
                        hasBFour = false;
                    } else {
                        segLen++;
                        // B_FLEX4 or higher = open four that wins in a 5-cell corridor
                        if (board.cell(cur).pattern4[side] >= B_FLEX4)
                            hasBFour = true;
                    }
                    x += DX[dir];
                    y += DY[dir];
                }
                // Segment ended at board edge
                if (segLen == 5 && hasBFour)
                    count++;
            }
        }
    }
    return count;
}

/// [PORTAL: WALL CORRECTION C] Isolated Threat Discount
/// Count threats (H_FLEX3 and above) for `side` that are in regions smaller
/// than regionThreshold — they cannot grow into cross-region threats.
static int countIsolatedThreats(const Board &board, Color side, int regionThreshold = 10)
{
    static thread_local bool visited[FULL_BOARD_CELL_COUNT];
    std::fill_n(visited, board.cellCount(), false);

    int bs = board.size();
    int isolated = 0;

    for (int i = 0; i < board.cellCount(); i++) {
        Pos pos(i);
        if (visited[i]) continue;
        if (board.cell(pos).piece == WALL) { visited[i] = true; continue; }

        Pos   stack[FULL_BOARD_CELL_COUNT];
        int   top       = 0;
        int   sz        = 0;
        int   threatCnt = 0;

        stack[top++] = pos;
        visited[i]   = true;
        while (top > 0) {
            Pos cur = stack[--top];
            sz++;
            if (board.cell(cur).pattern4[side] >= H_FLEX3)
                threatCnt++;

            static constexpr int DX[4] = {0, 0, 1, -1};
            static constexpr int DY[4] = {1, -1, 0, 0};
            for (int d = 0; d < 4; d++) {
                Pos nb = Pos(cur.x() + DX[d], cur.y() + DY[d]);
                if (!nb.isInBoard(bs, bs)) continue;
                if (visited[int(nb)]) continue;
                if (board.cell(nb).piece == WALL) continue;
                visited[int(nb)] = true;
                stack[top++] = nb;
            }
        }

        if (sz < regionThreshold)
            isolated += threatCnt;
    }
    return isolated;
}

/// [PORTAL: WALL STRATEGIC CORRECTION]
/// Compute a correction value to add to classical eval when WALL cells exist.
/// Positive = good for `self`, negative = bad for `self`.
/// Clamped to ±800 to avoid dominating the NNUE signal.
template <Rule R>
Value computeWallStrategicCorrection(const Board &board, Color self)
{
    int correction = 0;

    // [A] Dead pocket penalty — stones trapped in sub-5 dead regions are useless
    auto [selfDead, oppoDead] = countDeadPocketStones(board, self);
    correction += (oppoDead - selfDead) * Config::WALL_DEAD_POCKET_PENALTY;

    // [B] Corridor squeeze bonus — B4 in 5-cell corridor is unstoppable
    int selfCorridors = countCorridorFours(board, self);
    int oppoCorridors = countCorridorFours(board, ~self);
    correction += (selfCorridors - oppoCorridors) * Config::WALL_CORRIDOR_FOUR_BONUS;

    // [C] Isolated threat discount — B3/B4 behind small walled regions cannot grow
    int selfIso = countIsolatedThreats(board, self);
    int oppoIso = countIsolatedThreats(board, ~self);
    correction += (oppoIso - selfIso) * Config::WALL_ISOLATED_THREAT_PENALTY;

    return Value(std::clamp(correction, -800, 800));
}

}  // namespace (WALL correction)

namespace Evaluation {

/// Calculates the final evaluation of a board.
/// @note Board must have at least one stone placed (`ply() > 0`).
template <Rule R>
Value evaluate(const Board &board, Value alpha, Value beta)
{
    assert(board.ply() > 0);
    Color self = board.sideToMove();

    const StateInfo &st0 = board.stateInfo();
    const StateInfo &st1 = board.stateInfo(1);

    Value basicEval  = (evaluateBasic(st0, self) + evaluateBasic(st1, self)) / 2;
    Value threatEval = evaluateThreat<R>(st0, self);
    Value eval       = std::clamp(basicEval + threatEval, VALUE_EVAL_MIN, VALUE_EVAL_MAX);

    // [PORTAL: WALL CORRECTION] Apply WALL-specific strategic bonus/penalty when
    // static WALL cells exist on the board. This compensates for the NNUE being
    // trained on free-board data and not understanding WALL geometry.
    if (board.wallCount() > 0) {
        Value wallCorr = computeWallStrategicCorrection<R>(board, self);
        eval = std::clamp(eval + wallCorr, VALUE_EVAL_MIN, VALUE_EVAL_MAX);
    }

    if (board.evaluator()) {
        // Use evaluator eval if classical eval are in alpha-beta window margin
        int margin = classicalEvalMargin(eval);
        if (eval >= alpha - margin && eval <= beta + margin)
            return computeEvaluatorValue(board).value();
    }

    return eval;
}

template Value evaluate<FREESTYLE>(const Board &, Value, Value);
template Value evaluate<STANDARD>(const Board &, Value, Value);
template Value evaluate<RENJU>(const Board &, Value, Value);

/// A safe but slower version of evaluate().
/// Similar to evaluate(), but can be called for all board states.
Value evaluate(const Board &board, Rule rule)
{
    if (board.ply() > 0) {
        switch (rule) {
        default:
        case Rule::FREESTYLE: return evaluate<Rule::FREESTYLE>(board);
        case Rule::STANDARD: return evaluate<Rule::STANDARD>(board);
        case Rule::RENJU: return evaluate<Rule::RENJU>(board);
        }
    }
    else {
        Color            self = board.sideToMove();
        const StateInfo &st   = board.stateInfo();

        Value basicEval  = evaluateBasic(st, self);
        Value threatEval = VALUE_ZERO;
        switch (rule) {
        default:
        case Rule::FREESTYLE: threatEval = evaluateThreat<Rule::FREESTYLE>(st, self); break;
        case Rule::STANDARD: threatEval = evaluateThreat<Rule::STANDARD>(st, self); break;
        case Rule::RENJU: threatEval = evaluateThreat<Rule::RENJU>(st, self); break;
        }

        Value eval = basicEval + threatEval;

        return std::clamp(eval, VALUE_EVAL_MIN, VALUE_EVAL_MAX);
    }
}

ValueType computeEvaluatorValue(const Board &board)
{
    Color     self = board.sideToMove();
    ValueType v    = board.evaluator()->evaluateValue(board);

    // Adjust draw rate according to draw ratio and draw black win rate
    if (Config::EvaluatorDrawRatio < 1.0) {
        float newDrawRate = Config::EvaluatorDrawRatio * v.draw();
        float drawWinRate = Config::EvaluatorDrawBlackWinRate;
        drawWinRate       = self == BLACK ? drawWinRate : 1.0f - drawWinRate;
        v                 = v.valueOfDrawWinRate(drawWinRate, newDrawRate);
    }

    return v;
}

/// Trace all evaluation info from a board state with rule.
EvalInfo::EvalInfo(const Board &board, Rule rule)
    : plyBack {0}
    , self(board.sideToMove())
    , threatMask(makeThreatMask(board.stateInfo(), self))
{
    Board &b = const_cast<Board &>(board);
    Pos    undoHistory[2];
    int    undoCount = 0;

    for (size_t backIndex = 0; backIndex < arraySize(plyBack); backIndex++) {
        auto &info = plyBack[backIndex];
        if (backIndex > 0 && b.ply() > 0) {
            undoHistory[undoCount++] = b.getLastMove();
            b.undo(rule);
        }

        FOR_EVERY_EMPTY_POS(&b, pos)
        {
            const Cell &c = b.cell(pos);
            info.pcodeCount[BLACK][c.pcode<BLACK>()]++;
            info.pcodeCount[WHITE][c.pcode<WHITE>()]++;
        }
    }

    // Recover board state
    while (undoCount > 0) {
        b.move(rule, undoHistory[--undoCount]);
    }
}

}  // namespace Evaluation
