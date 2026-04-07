/*
 *  Portal Gomoku Engine — based on Rapfi
 *  Original Rapfi Copyright (C) 2022  Rapfi developers
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  PORTAL MODIFICATIONS:
 *  - Added PortalPair struct for teleportation pairs
 *  - Added portalPartner[] lookup table for O(1) teleport
 *  - Added portalAffected[][] mask per direction
 *  - Modified getKeyAt() with dual-path (fast bitKey / slow virtualWalk)
 *  - Added buildPortalKey(), portalStep()
 *  - move() extended update zone for portal neighborhoods
 */

#pragma once

#include "../config.h"
#include "../core/hash.h"
#include "../core/platform.h"
#include "../core/pos.h"
#include "../core/types.h"
#include "pattern.h"

#include <array>
#include <cassert>
#include <cstring>


namespace Search {
class SearchThread;
}
namespace Evaluation {
class Evaluator;
}

#define FOR_EVERY_POSITION(board, pos)                                   \
    for (Pos pos = (board)->startPos(); pos <= (board)->endPos(); pos++) \
        if ((board)->get(pos) != WALL)

#define FOR_EVERY_EMPTY_POS(board, pos)                                  \
    for (Pos pos = (board)->startPos(); pos <= (board)->endPos(); pos++) \
        if ((board)->isEmpty(pos))

#define FOR_EVERY_CANDAREA_POS(board, pos, candArea) \
    for (int8_t _y = (candArea).y0,                  \
                y1 = (candArea).y1,                  \
                x0 = (candArea).x0,                  \
                x1 = (candArea).x1,                  \
                _x = x0;                             \
         _y <= y1;                                   \
         _y++, _x = x0)                              \
        for (Pos pos {_x, _y}; _x <= x1; _x++, pos++)

#define FOR_EVERY_CAND_POS(board, pos)                                \
    FOR_EVERY_CANDAREA_POS(board, pos, (board)->stateInfo().candArea) \
    if ((board)->isEmpty(pos) && (board)->cell(pos).isCandidate())

// ============================================================
// CandArea — unchanged from Rapfi
// ============================================================

/// CandArea struct represents a rectangle area on board which can be considered
/// as move candidate.
struct CandArea
{
    int8_t x0, y0, x1, y1;

    CandArea() : x0(INT8_MAX), y0(INT8_MAX), x1(INT8_MIN), y1(INT8_MIN) {}
    CandArea(int8_t x0, int8_t y0, int8_t x1, int8_t y1) : x0(x0), y0(y0), x1(x1), y1(y1) {}

    /// Expand candidate area at pos with a range of square with length 'dist'.
    void expand(Pos pos, int boardSize, int dist)
    {
        int x = pos.x(), y = pos.y();

        x0 = std::min((int)x0, std::max(x - dist, 0));
        y0 = std::min((int)y0, std::max(y - dist, 0));
        x1 = std::max((int)x1, std::min(x + dist, boardSize - 1));
        y1 = std::max((int)y1, std::min(y + dist, boardSize - 1));
    }
};

// ============================================================
// StateInfo — unchanged from Rapfi
// ============================================================

/// StateInfo struct records all incremental board information used in one ply.
struct StateInfo
{
    CandArea candArea;
    Pos      lastMove;
    Pos      lastFlex4AttackMove[SIDE_NB];
    Pos      lastPattern4Move[SIDE_NB][3];
    uint16_t p4Count[SIDE_NB][PATTERN4_NB];
    Value    valueBlack;

    /// Query the last emerged pattern4 pos.
    /// @note p4 must be one of [C_BLOCK4_FLEX3, B_FLEX4, A_FIVE].
    Pos lastPattern4(Color side, Pattern4 p4) const
    {
        assert(p4 >= C_BLOCK4_FLEX3 && p4 <= A_FIVE);
        return lastPattern4Move[side][p4 - C_BLOCK4_FLEX3];
    }
};

// ============================================================
// Cell — unchanged from Rapfi
// ============================================================

/// Cell struct contains all information for a move cell on board, including current
/// stone piece, candidate, pattern, pattern4 and move score.
struct Cell
{
    Color     piece;
    uint8_t   cand;
    Pattern4  pattern4[SIDE_NB];
    Score     score[SIDE_NB];
    Value     valueBlack;
    Pattern2x pattern2x[4];

    /// Check if this cell is a move candidate, which can be used in move generation.
    bool isCandidate() const { return cand > 0; }

    /// Get the line level pattern of this cell.
    Pattern pattern(Color c, int dir) const
    {
        assert(c == BLACK || c == WHITE);
        return c == BLACK ? pattern2x[dir].patBlack : pattern2x[dir].patWhite;
    }

    /// Get the complex pattern code at this cell for one side.
    template <Color C>
    PatternCode pcode() const
    {
        static_assert(C == BLACK || C == WHITE);
        if constexpr (C == BLACK)
            return PatternConfig::PCODE[pattern2x[0].patBlack][pattern2x[1].patBlack]
                                       [pattern2x[2].patBlack][pattern2x[3].patBlack];
        else
            return PatternConfig::PCODE[pattern2x[0].patWhite][pattern2x[1].patWhite]
                                       [pattern2x[2].patWhite][pattern2x[3].patWhite];
    }

    /// Update the pattern4 and score with the new pattern code for both sides.
    template <Rule R>
    void updatePattern4AndScore(PatternCode pcodeBlack, PatternCode pcodeWhite)
    {
        Pattern4Score p4ScoreBlack = Config::getP4Score(R, BLACK, pcodeBlack);
        Pattern4Score p4ScoreWhite = Config::getP4Score(R, WHITE, pcodeWhite);
        pattern4[BLACK]            = (Pattern4)p4ScoreBlack;
        pattern4[WHITE]            = (Pattern4)p4ScoreWhite;
        score[BLACK]               = p4ScoreBlack.scoreSelf() + p4ScoreWhite.scoreOppo();
        score[WHITE]               = p4ScoreWhite.scoreSelf() + p4ScoreBlack.scoreOppo();
    }
};

// ============================================================
// PORTAL: New data structures
// ============================================================

/// PortalPair represents two board cells that teleport lines in ALL 4 directions.
///
/// Portal rule (confirmed semantics):
///   When a line traveling in direction D reaches cell A:
///     → skip A (zero-width), skip B (zero-width)
///     → continue from B + DIRECTION[D] * sign
///   Same applies when line reaches B (→ continue from A + DIRECTION[D] * sign).
///   Works for ALL 4 directions, regardless of geometric alignment of A and B.
///
///  ┌────────────────────────────────────────────────────────────┐
///  │  a   : Pos    — First portal cell                         │
///  │  b   : Pos    — Second portal cell (partner of a)         │
///  └────────────────────────────────────────────────────────────┘
///
/// Both a and b are stored as piece=WALL in cells[] so they cannot
/// be played on and naturally block lines in all non-entering directions.
///
/// IMPORTANT: The portal is zero-width — cells a and b do NOT contribute
/// bits to the pattern window. They are completely transparent in the
/// virtual line for cells marked as portalAffected.
struct PortalPair
{
    Pos a;  ///< First portal cell
    Pos b;  ///< Second portal cell

    /// Check if two positions are collinear in a given direction.
    /// Used to detect gap cells that must NOT use portalAffected path.
    /// @param dir Direction index (0=H, 1=V, 2=UP_RIGHT, 3=DOWN_RIGHT)
    [[nodiscard]] bool collinear(int dir) const
    {
        int dx = b.x() - a.x(), dy = b.y() - a.y();
        switch (dir) {
        case 0: return dy == 0;           // Horizontal: same row
        case 1: return dx == 0;           // Vertical: same column
        case 2: return dx + dy == 0;      // UP_RIGHT anti-diagonal: x+y constant
        case 3: return dx - dy == 0;      // DOWN_RIGHT diagonal: x-y constant
        default: return false;
        }
    }

    /// Check if pos is a gap cell (between a and b along direction dir).
    /// Gap cells on the collinear direction must use normal bitKey (WALL bits)
    /// to avoid infinite loops in the portal walk.
    /// For non-collinear directions, gap cells do not exist → always returns false.
    /// @param dir Direction index (0-3)
    [[nodiscard]] bool isGapCell(Pos pos, int dir) const
    {
        if (!collinear(dir))
            return false;

        // Check if pos lies strictly between a and b on this line
        // Uses line coordinate: project pos onto the line direction
        int posCoord, aCoord, bCoord;
        switch (dir) {
        case 0: posCoord = pos.x(); aCoord = a.x(); bCoord = b.x(); break;
        case 1: posCoord = pos.y(); aCoord = a.y(); bCoord = b.y(); break;
        case 2: posCoord = pos.x(); aCoord = a.x(); bCoord = b.x(); break;  // x+y same, x sufficient
        case 3: posCoord = pos.x(); aCoord = a.x(); bCoord = b.x(); break;
        default: return false;
        }

        int lo = std::min(aCoord, bCoord);
        int hi = std::max(aCoord, bCoord);
        return posCoord > lo && posCoord < hi && pos.y() == (dir == 0 ? a.y() :
               dir == 1 ? a.x() == pos.x() ? pos.y() : -1  // dummy for V
               : pos.y());  // simplified — see detailed impl in initPortals
    }
};

// ============================================================
// Board class
// ============================================================

/// Board class is the main class used to represent a board position state.
/// Extended from Rapfi to support WALL cells and Portal teleportation.
///
/// Key extension: getKeyAt<R>(pos, dir) uses a dual-path design:
///   Fast path (>90% cells): rotr(bitKey[dir], shift)  — identical to Rapfi
///   Slow path (<10% cells): buildPortalKey()           — virtual line walk
///
/// The slow path is only used for cells whose L-step window includes a portal.
class Board
{
public:
    /// MoveType represents the update mode of move/undo.
    enum class MoveType { NORMAL, NO_EVALUATOR, NO_EVAL, NO_EVAL_MULTI };

    /// Creates a board with board size and candidate range.
    /// @param boardSize Size of the board, in range [1, MAX_BOARD_SIZE].
    explicit Board(int boardSize, CandidateRange candRange = Config::DefaultCandidateRange);

    /// Clone a board object from other board and bind a search thread to it.
    explicit Board(const Board &other, Search::SearchThread *thread);

    Board(const Board &) = delete;
    ~Board();

    // ------------------------------------------------------------------------
    // PORTAL: Setup methods (call before newGame)

    /// PORTAL: Add a WALL cell (immovable, blocks lines in all directions).
    /// Must be called before newGame(). Multiple WALLs can be added.
    void addWall(Pos pos);

    /// PORTAL: Add a portal pair (A, B).
    /// Both cells become immovable. Lines teleport through them in all directions.
    /// Must be called before newGame(). Up to MAX_PORTAL_PAIRS pairs allowed.
    void addPortal(Pos a, Pos b);

    /// PORTAL: Clear all WALLs and portals (reset to standard board).
    void clearPortals();

    // ------------------------------------------------------------------------
    // board modifier (and dynamic dispatch version)

    /// Initialize the board to an empty board state of rule R.
    /// Applies all WALLs and portals added via addWall/addPortal.
    template <Rule R>
    void newGame();

    /// Make move and incremental update the board state.
    template <Rule R, MoveType MT = MoveType::NORMAL>
    void move(Pos pos);

    /// Undo move and rollback the board state.
    template <Rule R, MoveType MT = MoveType::NORMAL>
    void undo();

    /// Dynamic dispatch versions
    void newGame(Rule rule);
    void move(Rule rule, Pos pos);
    void undo(Rule rule);

    // ------------------------------------------------------------------------
    // special helper function

    void flipSide() { currentSide = ~currentSide; }

    // ------------------------------------------------------------------------
    // pos-specific info queries

    inline const Cell &cell(Pos pos) const
    {
        assert(pos >= 0 && pos < FULL_BOARD_CELL_COUNT);
        return cells[pos];
    }

    inline Color get(Pos pos) const
    {
        assert(pos >= 0 && pos < FULL_BOARD_CELL_COUNT);
        return cells[pos].piece;
    }

    bool isInBoard(Pos pos) const { return pos.isInBoard(boardSize, boardSize); }
    bool isEmpty(Pos pos) const { return get(pos) == EMPTY; }
    bool isLegal(Pos pos) const { return pos.valid() && (isEmpty(pos) || pos == Pos::PASS); }

    bool checkForbiddenPoint(Pos pos) const;

    /// Get the pattern key for pos in direction dir.
    ///
    /// PORTAL dual-path:
    ///   if portalAffected[dir][pos]:
    ///     → buildPortalKey<R>(pos, dir)   [slow: virtual line walk]
    ///   else:
    ///     → rotr(bitKey[dir], shift)      [fast: unchanged Rapfi]
    template <Rule R>
    uint64_t getKeyAt(Pos pos, int dir) const;

    // ------------------------------------------------------------------------
    // PORTAL: Query portal state

    /// PORTAL: Check if a position is a portal cell.
    [[nodiscard]] bool isPortalCell(Pos pos) const
    {
        return portalPartner[pos] != Pos::NONE;
    }

    /// PORTAL: Check if a position is a WALL cell placed by addWall().
    [[nodiscard]] bool isWallCell(Pos pos) const { return wallMask[pos]; }

    /// PORTAL: Get number of active portal pairs.
    int portalCount() const { return numPortals; }

    /// PORTAL: Check if a cell is portal-affected in a specific direction (debug/test)
    [[nodiscard]] bool isPortalAffected(Pos pos, int dir) const
    {
        return portalAffected[dir][int(pos)];
    }

    /// PORTAL: Get portal pair by index.
    const PortalPair &getPortal(int i) const
    {
        assert(i >= 0 && i < numPortals);
        return portals[i];
    }

    /// PORTAL: Walk one step in direction dir, handling portal teleportation.
    /// Public for testing — this is the core teleportation primitive.
    inline Pos portalStep(Pos cur, int dir, int sign) const;

    // ------------------------------------------------------------------------
    // general board info queries

    int                    size() const { return boardSize; }
    int                    cellCount() const { return boardCellCount; }
    Pos                    centerPos() const { return {boardSize / 2, boardSize / 2}; }
    Pos                    startPos() const { return {0, 0}; }
    Pos                    endPos() const { return {boardSize - 1, boardSize - 1}; }
    Search::SearchThread  *thisThread() const { return thisThread_; }
    Evaluation::Evaluator *evaluator() const { return evaluator_; }

    // ------------------------------------------------------------------------
    // current board state queries

    int   ply() const { return moveCount; }
    int   nonPassMoveCount() const { return moveCount - passMoveCount(); }
    int   passMoveCount() const { return passCount[BLACK] + passCount[WHITE]; }
    int   passMoveCountOfSide(Color side) const { return passCount[side]; }
    int   movesLeft() const { return boardCellCount - nonPassMoveCount(); }
    Color sideToMove() const { return currentSide; }

    HashKey zobristKey() const { return currentZobristKey ^ Hash::zobristSide[currentSide]; }

    HashKey zobristKeyAfter(Pos pos) const
    {
        return currentZobristKey ^ Hash::zobristSide[~currentSide]
               ^ (pos != Pos::PASS ? Hash::zobrist[currentSide][pos] : HashKey {});
    }

    uint16_t p4Count(Color side, Pattern4 p4) const { return stateInfo().p4Count[side][p4]; }

    // ------------------------------------------------------------------------
    // history board state queries

    inline Pos getHistoryMove(int moveIndex) const
    {
        assert(moveIndex >= 0 && moveIndex < moveCount);
        return stateInfos[moveIndex + 1].lastMove;
    }

    inline Pos getRecentMove(int reverseIndex) const
    {
        assert(reverseIndex >= 0);
        int index = moveCount - reverseIndex;
        return index <= 0 ? Pos::NONE : stateInfos[index].lastMove;
    }

    inline const StateInfo &stateInfo(int reverseIndex = 0) const
    {
        assert(0 <= reverseIndex && reverseIndex <= moveCount);
        return stateInfos[moveCount - reverseIndex];
    }

    Pos getLastMove() const { return getRecentMove(0); }
    Pos getLastActualMoveOfSide(Color side) const;

    // ------------------------------------------------------------------------
    // miscellaneous

    void        expandCandArea(Pos pos, int fillDist, int lineDist);
    std::string positionString() const;
    std::string trace() const;

private:
    // -----------------------------------------------------------------------
    // Unchanged from Rapfi

    struct SingleCellUpdateCache
    {
        Pattern4 pattern4[SIDE_NB];
        Score    score[SIDE_NB];
        Value    valueBlack;
    };
    // PORTAL: Size increased from 40 to 60 to accommodate portal update zone extension.
    // Normal zone: 4 dirs × (2L+1) ≤ 4×9 = 36 cells.
    // Portal extension adds up to MAX_PORTAL_PAIRS × 2 × L extra cells per move.
    using UpdateCache = std::array<SingleCellUpdateCache, 60>;

    /// The cells array of the board.
    Cell cells[FULL_BOARD_CELL_COUNT];

    /// Bitkeys of 4 directions — identical to Rapfi.
    /// Portal cells are stored with WALL bits (0b11) in all 4 directions.
    /// For the portalAffected cells in a direction, getKeyAt() overrides
    /// these bits via buildPortalKey() instead.
    uint64_t bitKey0[FULL_BOARD_SIZE];          // [RIGHT(MSB) — LEFT(LSB)]
    uint64_t bitKey1[FULL_BOARD_SIZE];          // [DOWN(MSB)  — UP(LSB)]
    uint64_t bitKey2[FULL_BOARD_SIZE * 2 - 1];  // [UP_RIGHT(MSB) — DOWN_LEFT(LSB)]
    uint64_t bitKey3[FULL_BOARD_SIZE * 2 - 1];  // [DOWN_RIGHT(MSB) — UP_LEFT(LSB)]

    int                    boardSize;
    int                    boardCellCount;
    int                    moveCount;
    int                    passCount[SIDE_NB];
    Color                  currentSide;
    HashKey                currentZobristKey;
    StateInfo             *stateInfos;
    UpdateCache           *updateCache;
    const Direction       *candidateRange;
    uint32_t               candidateRangeSize;
    uint32_t               candAreaExpandDist;
    Evaluation::Evaluator *evaluator_;
    Search::SearchThread  *thisThread_;

    // -----------------------------------------------------------------------
    // PORTAL: New members

    /// PORTAL: Maximum portal pairs supported.
    static constexpr int MAX_PORTAL_PAIRS = 8;

    /// PORTAL: Portal pair definitions (set by addPortal, frozen at newGame).
    PortalPair portals[MAX_PORTAL_PAIRS];
    int        numPortals = 0;

    /// PORTAL: Per-cell partner lookup.
    ///
    ///   portalPartner[A] = B,  portalPartner[B] = A  for each pair
    ///   portalPartner[x] = Pos::NONE                 for non-portal cells
    ///
    /// Enables O(1) teleport detection in portalStep().
    /// Size: FULL_BOARD_CELL_COUNT × sizeof(Pos) = 1024 × 2 = 2 KB
    Pos portalPartner[FULL_BOARD_CELL_COUNT];

    /// PORTAL: Per-direction, per-cell flag: must use buildPortalKey() slow path.
    ///
    ///   portalAffected[dir][pos] = true
    ///     iff the L-step virtual window at (pos, dir) passes through a portal cell.
    ///
    /// Computed once at newGame(). Read on every getKeyAt() call.
    /// Size: 4 × FULL_BOARD_CELL_COUNT × 1 byte = 4 KB (L1 cache friendly)
    bool portalAffected[4][FULL_BOARD_CELL_COUNT];

    /// PORTAL: Static WALL (non-portal immovable cell) flags.
    /// Set by addWall(). Purely for query purposes (wall logic is in cells[].piece=WALL).
    bool wallMask[FULL_BOARD_CELL_COUNT];

    /// PORTAL: Pre-computed list of extra cells to refresh in move() update zone.
    ///
    /// When a stone is placed near portal A in direction dir, cells near portal B
    /// in the same direction also need their virtual pattern refreshed (and vice versa).
    /// This list is computed once at newGame() for each (portal, direction) pair.
    ///
    ///   portalUpdateZone[i][dir] = extra cell positions to refresh for portal i in dir
    ///
    struct PortalUpdateEntry
    {
        Pos  cells[20];  ///< Up to 2×L extra positions to refresh
        int  count = 0;  ///< Number of valid entries
    };
    PortalUpdateEntry portalUpdateZone[MAX_PORTAL_PAIRS][4];

    // -----------------------------------------------------------------------
    // PORTAL: Internal methods

    /// PORTAL: Initialize all portal data structures.
    /// Called at the start of newGame(). Sets WALL bits for portal/wall cells,
    /// fills portalPartner[], computes portalAffected[][], and pre-computes
    /// portalUpdateZone[][].
    template <Rule R>
    void initPortals();

    // portalStep declaration moved to public section for testing

    /// PORTAL: Build the virtual line key for a portal-affected cell.
    ///
    /// Walks the virtual line by calling portalStep() for each of the 2L+1
    /// window positions, collecting 2-bit color values for each real cell visited.
    /// Portal cells themselves are skipped (zero-width) — they contribute no bits.
    ///
    /// The resulting key has the same format as the fast-path bitKey extraction,
    /// and is directly passed to PatternConfig::lookupPattern<R>().
    ///
    /// @param pos  Center position of the pattern window
    /// @param dir  Direction index (0-3)
    /// @return     Raw 64-bit key for lookupPattern<R>()
    template <Rule R>
    uint64_t buildPortalKey(Pos pos, int dir) const;

    // -----------------------------------------------------------------------
    // Unchanged from Rapfi

    void setBitKey(Pos pos, Color c);
    void flipBitKey(Pos pos, Color c);
};

// ============================================================
// Inline implementations
// ============================================================

// --- setBitKey / flipBitKey — identical to Rapfi ----------------------------

inline void Board::setBitKey(Pos pos, Color c)
{
    assert(c == BLACK || c == WHITE);
    int x = pos.x() + BOARD_BOUNDARY;
    int y = pos.y() + BOARD_BOUNDARY;

    const uint64_t mask = 0x1 + c;
    bitKey0[y] |= mask << (2 * x);
    bitKey1[x] |= mask << (2 * y);
    bitKey2[x + y] |= mask << (2 * x);
    bitKey3[FULL_BOARD_SIZE - 1 - x + y] |= mask << (2 * x);
}

inline void Board::flipBitKey(Pos pos, Color c)
{
    assert(c == BLACK || c == WHITE);
    int x = pos.x() + BOARD_BOUNDARY;
    int y = pos.y() + BOARD_BOUNDARY;

    const uint64_t mask = 0x1 + c;
    bitKey0[y] ^= mask << (2 * x);
    bitKey1[x] ^= mask << (2 * y);
    bitKey2[x + y] ^= mask << (2 * x);
    bitKey3[FULL_BOARD_SIZE - 1 - x + y] ^= mask << (2 * x);
}

// --- PORTAL: portalStep() ---------------------------------------------------

/// PORTAL: One virtual step in direction dir with sign.
/// If next cell is a portal → teleport to after partner, skipping both.
/// Otherwise → normal step.
inline Pos Board::portalStep(Pos cur, int dir, int sign) const
{
    // Next physical position (may be out of bounds — handled by WALL bits)
    const int  rawNext = int(cur) + int(DIRECTION[dir]) * sign;

    // Bounds check: out-of-board cells act as WALL (no portal there)
    if (rawNext < 0 || rawNext >= FULL_BOARD_CELL_COUNT)
        return Pos{int16_t(rawNext)};

    Pos next{int16_t(rawNext)};

    // PORTAL: O(1) lookup — partner is Pos::NONE for non-portal cells
    const Pos partner = portalPartner[next];
    if (partner != Pos::NONE) {
        // Skip both portal cells (zero-width); continue from after partner
        // exit = partner + DIRECTION[dir] * sign
        return Pos(int16_t(int(partner) + int(DIRECTION[dir]) * sign));
    }

    return next;
}

// --- PORTAL: buildPortalKey<R>() --------------------------------------------

/// PORTAL: Build virtual line key by walking through portals.
///
/// Window layout (L = HalfLineLen<R>):
///   index: 0 ... L-1 | L (center) | L+1 ... 2L
///   bit position: 2*i and 2*i+1 per index i
///
/// Encoding:
///   EMPTY=00, BLACK=01, WHITE=10, WALL=11
///
/// Portal cells are skipped by portalStep() — the window always
/// contains exactly 2L+1 real (non-portal) cell entries.
template <Rule R>
inline uint64_t Board::buildPortalKey(Pos pos, int dir) const
{
    constexpr int L         = PatternConfig::HalfLineLen<R>;
    constexpr int WindowLen = 2 * L + 1;

    // --- Step 1: Walk backward L steps to find the window start ---
    Pos cur = pos;
    for (int i = 0; i < L; i++)
        cur = portalStep(cur, dir, -1);

    // --- Step 2: Walk forward WindowLen steps, collecting bits ---
    uint64_t key = 0;
    for (int i = 0; i < WindowLen; i++) {
        // Determine color bits for current position
        // PORTAL: Rapfi bitKey convention:
        //   EMPTY = 11 (both BLACK+WHITE bits set via setBitKey)
        //   BLACK = 10 (WHITE bit only → BLACK stone placed, own bit flipped)
        //   WHITE = 01 (BLACK bit only → WHITE stone placed, own bit flipped)
        //   WALL  = 00 (no bits set → boundary/wall)
        uint64_t bits;
        if (int(cur) < 0 || int(cur) >= FULL_BOARD_CELL_COUNT) {
            bits = 0b00;  // Out-of-board = WALL
        }
        else {
            switch (cells[cur].piece) {
            case EMPTY: bits = 0b11; break;
            case BLACK: bits = 0b10; break;
            case WHITE: bits = 0b01; break;
            default:    bits = 0b00; break;  // WALL (includes portal cells themselves,
                                              // but portalStep skips them so this path
                                              // is only hit for real WALLs/boundaries)
            }
        }
        key |= bits << (2 * i);

        // Advance (skip last step to avoid stepping past the window end)
        if (i < WindowLen - 1)
            cur = portalStep(cur, dir, +1);
    }

    return key;
}

// --- getKeyAt<R>() — dual-path ----------------------------------------------

/// PORTAL dual-path getKeyAt:
///   Fast path: bitKey rotation (Rapfi, O(1), single shift)
///   Slow path: buildPortalKey (portal-aware virtual walk)
///
/// Branch: portalAffected[dir][pos] — pre-computed at newGame().
/// For boards with no portals, portalAffected is always false → zero overhead.
template <Rule R>
inline uint64_t Board::getKeyAt(Pos pos, int dir) const
{
    assert(dir >= 0 && dir < 4);

    // PORTAL: Check if this cell's window passes through a portal
    if (portalAffected[dir][int(pos)]) [[unlikely]]
        return buildPortalKey<R>(pos, dir);

    // Fast path — identical to Rapfi
    constexpr int L = PatternConfig::HalfLineLen<R>;
    int           x = pos.x() + BOARD_BOUNDARY;
    int           y = pos.y() + BOARD_BOUNDARY;

    switch (dir) {
    default:
    case 0: return rotr(bitKey0[y], 2 * (x - L));
    case 1: return rotr(bitKey1[x], 2 * (y - L));
    case 2: return rotr(bitKey2[x + y], 2 * (x - L));
    case 3: return rotr(bitKey3[FULL_BOARD_SIZE - 1 - x + y], 2 * (x - L));
    }
}

// --- Dynamic dispatch -------------------------------------------------------

inline void Board::newGame(Rule rule)
{
    assert(rule < RULE_NB);
    void (Board::*F[])() = {&Board::newGame<FREESTYLE>,
                            &Board::newGame<STANDARD>,
                            &Board::newGame<RENJU>};
    (this->*F[rule])();
}

inline void Board::move(Rule rule, Pos pos)
{
    assert(rule < RULE_NB);
    void (Board::*F[])(Pos) = {&Board::move<FREESTYLE>,
                               &Board::move<STANDARD>,
                               &Board::move<RENJU>};
    (this->*F[rule])(pos);
}

inline void Board::undo(Rule rule)
{
    assert(rule < RULE_NB);
    void (Board::*F[])() = {&Board::undo<FREESTYLE>,
                            &Board::undo<STANDARD>,
                            &Board::undo<RENJU>};
    (this->*F[rule])();
}
