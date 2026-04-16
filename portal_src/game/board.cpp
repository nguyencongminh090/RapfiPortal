/*
 *  Portal Gomoku Engine — based on Rapfi
 *  Original Rapfi Copyright (C) 2022  Rapfi developers
 *
 *  PORTAL MODIFICATIONS in board.cpp:
 *  - Constructor/copy-constructor: copy portal data (portalPartner, portalAffected, etc.)
 *  - addWall(), addPortal(), clearPortals(): setup API
 *  - initPortals<R>(): pre-compute portalPartner[], portalAffected[][], portalUpdateZone[][]
 *  - newGame<R>(): calls initPortals, sets WALL bits for portals/walls
 *  - move<R,MT>(): extended update zone for portal-affected neighbors
 *  - undo<R,MT>(): matching extended rollback
 *  - All other functions: unchanged from Rapfi
 */

#include "board.h"

#include "../core/iohelper.h"
#include "../core/pos.h"
#include "../core/utils.h"
#include "../eval/evaluator.h"
#include "../search/searchthread.h"

#include <algorithm>
#include <bitset>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <tuple>

namespace {

/// Checks whether current p4Count in stateInfo matches that on board (debug).
bool checkP4(const Board *board)
{
    int p4[SIDE_NB][PATTERN4_NB] = {0};
    FOR_EVERY_EMPTY_POS(board, pos)
    {
        p4[BLACK][board->cell(pos).pattern4[BLACK]]++;
        p4[WHITE][board->cell(pos).pattern4[WHITE]]++;
    }
    for (Color c : {BLACK, WHITE})
        for (Pattern4 i = FORBID; i < PATTERN4_NB; i = Pattern4(i + 1)) {
            if (p4[c][i] != board->stateInfo().p4Count[c][i])
                return false;
        }
    return true;
}

/// PORTAL: Compute the physical distance between two positions along a direction.
/// Returns -1 if the positions are not strictly collinear in that direction.
int physicalDistAlongDir(Pos a, Pos b, int dir)
{
    int dx = b.x() - a.x();
    int dy = b.y() - a.y();

    switch (dir) {
    case 0:  // Horizontal: same row, distance = |dx|
        return (dy == 0) ? std::abs(dx) : -1;
    case 1:  // Vertical: same column, distance = |dy|
        return (dx == 0) ? std::abs(dy) : -1;
    case 2:  // UP_RIGHT anti-diagonal (x+y constant): same x+y, distance = |dx|
        return (dx + dy == 0) ? std::abs(dx) : -1;
    case 3:  // DOWN_RIGHT diagonal (x-y constant): same x-y, distance = |dx|
        return (dx == dy) ? std::abs(dx) : -1;
    default: return -1;
    }
}

/// PORTAL: Check if pos is strictly between a and b on a specific direction line.
/// Both a and b must be on the same line in direction dir.
bool isBetweenOnLine(Pos pos, Pos a, Pos b, int dir)
{
    // Project all positions onto the direction's primary coordinate
    int posCoord, aCoord, bCoord;
    switch (dir) {
    case 0:  // Horizontal: use x
        if (pos.y() != a.y()) return false;
        posCoord = pos.x(); aCoord = a.x(); bCoord = b.x();
        break;
    case 1:  // Vertical: use y
        if (pos.x() != a.x()) return false;
        posCoord = pos.y(); aCoord = a.y(); bCoord = b.y();
        break;
    case 2:  // UP_RIGHT: x+y must match, use x to order
        if (pos.x() + pos.y() != a.x() + a.y()) return false;
        posCoord = pos.x(); aCoord = a.x(); bCoord = b.x();
        break;
    case 3:  // DOWN_RIGHT: x-y must match, use x to order
        if (pos.x() - pos.y() != a.x() - a.y()) return false;
        posCoord = pos.x(); aCoord = a.x(); bCoord = b.x();
        break;
    default: return false;
    }

    int lo = std::min(aCoord, bCoord);
    int hi = std::max(aCoord, bCoord);
    return posCoord > lo && posCoord < hi;
}

}  // namespace

// =============================================================================
// Constructors / Destructor
// =============================================================================

Board::Board(int boardSize, CandidateRange candRange)
    : boardSize(boardSize)
    , boardCellCount(boardSize * boardSize)
    , moveCount(0)
    , passCount {0, 0}
    , currentSide(BLACK)
    , currentZobristKey(0)
    , candidateRange(nullptr)
    , candidateRangeSize(0)
    , evaluator_(nullptr)
    , thisThread_(nullptr)
    , numPortals(0)  // PORTAL
{
    assert(0 < boardSize && boardSize <= MAX_BOARD_SIZE);
    stateInfos  = new StateInfo[1 + boardCellCount * 2] {};
    updateCache = new UpdateCache[1 + boardCellCount * 2];

    // PORTAL: Initialize portal data to safe defaults
    std::fill_n(portalPartner, FULL_BOARD_CELL_COUNT, Pos::PASS);
    std::memset(portalAffected, 0, sizeof(portalAffected));
    std::memset(wallMask, 0, sizeof(wallMask));

    // Set candidate range of the board
    switch (candRange) {
    case CandidateRange::SQUARE2:
        candidateRange     = RANGE_SQUARE2;
        candidateRangeSize = arraySize(RANGE_SQUARE2);
        candAreaExpandDist = 2;
        break;
    case CandidateRange::SQUARE2_LINE3:
        candidateRange     = RANGE_SQUARE2_LINE3;
        candidateRangeSize = arraySize(RANGE_SQUARE2_LINE3);
        candAreaExpandDist = 3;
        break;
    case CandidateRange::SQUARE3:
        candidateRange     = RANGE_SQUARE3;
        candidateRangeSize = arraySize(RANGE_SQUARE3);
        candAreaExpandDist = 3;
        break;
    case CandidateRange::SQUARE3_LINE4:
        candidateRange     = RANGE_SQUARE3_LINE4;
        candidateRangeSize = arraySize(RANGE_SQUARE3_LINE4);
        candAreaExpandDist = 3;
        break;
    case CandidateRange::SQUARE4:
        candidateRange     = RANGE_SQUARE4;
        candidateRangeSize = arraySize(RANGE_SQUARE4);
        candAreaExpandDist = 4;
        break;
    default:  // Full board candidate
        break;
    }
}

Board::Board(const Board &other, Search::SearchThread *thread)
    : boardSize(other.boardSize)
    , boardCellCount(other.boardCellCount)
    , moveCount(other.moveCount)
    , passCount {other.passCount[0], other.passCount[1]}
    , currentSide(other.currentSide)
    , currentZobristKey(other.currentZobristKey)
    , candidateRange(other.candidateRange)
    , candidateRangeSize(other.candidateRangeSize)
    , candAreaExpandDist(other.candAreaExpandDist)
    , evaluator_(thread ? thread->evaluator.get() : nullptr)
    , thisThread_(thread)
    , numPortals(other.numPortals)  // PORTAL
{
    std::copy_n(other.cells, FULL_BOARD_CELL_COUNT, cells);
    std::copy_n(other.bitKey0, arraySize(bitKey0), bitKey0);
    std::copy_n(other.bitKey1, arraySize(bitKey1), bitKey1);
    std::copy_n(other.bitKey2, arraySize(bitKey2), bitKey2);
    std::copy_n(other.bitKey3, arraySize(bitKey3), bitKey3);

    stateInfos  = new StateInfo[1 + boardCellCount * 2] {};
    updateCache = new UpdateCache[1 + boardCellCount * 2];
    std::copy_n(other.stateInfos, 1 + moveCount, stateInfos);
    std::copy_n(other.updateCache, 1 + moveCount, updateCache);

    // PORTAL: Copy portal data structures
    std::copy_n(other.portals, MAX_PORTAL_PAIRS, portals);
    std::copy_n(other.portalPartner, FULL_BOARD_CELL_COUNT, portalPartner);
    std::memcpy(portalAffected, other.portalAffected, sizeof(portalAffected));
    std::memcpy(wallMask, other.wallMask, sizeof(wallMask));
    std::memcpy(portalUpdateZone, other.portalUpdateZone, sizeof(portalUpdateZone));

    // Sync evaluator state with board state
    if (evaluator_)
        evaluator_->syncWithBoard(*this);
}

Board::~Board()
{
    delete[] stateInfos;
    delete[] updateCache;
}

// =============================================================================
// PORTAL: Setup methods
// =============================================================================

/// PORTAL: Add a static WALL cell.
/// Must be called before newGame(). The actual WALL placement in cells[] and
/// bitKeys[] happens in newGame() → initPortals().
void Board::addWall(Pos pos)
{
    assert(pos.isInBoard(boardSize, boardSize));
    wallMask[pos] = true;
}

/// PORTAL: Add a portal pair.
/// Both cells become immovable WALLs that also allow line teleportation.
/// Must be called before newGame().
void Board::addPortal(Pos a, Pos b)
{
    assert(numPortals < MAX_PORTAL_PAIRS);
    assert(a.isInBoard(boardSize, boardSize));
    assert(b.isInBoard(boardSize, boardSize));
    assert(int(a) != int(b));

    portals[numPortals++] = {a, b};
}

/// PORTAL: Reset all WALLs and portals.
void Board::clearPortals()
{
    numPortals = 0;
    std::fill_n(portalPartner, FULL_BOARD_CELL_COUNT, Pos::PASS);
    std::memset(portalAffected, 0, sizeof(portalAffected));
    std::memset(wallMask, 0, sizeof(wallMask));
    std::memset(portalUpdateZone, 0, sizeof(portalUpdateZone));
}

// =============================================================================
// PORTAL: initPortals<R>() — pre-compute all portal data structures
// =============================================================================

/// PORTAL: Initialize portal lookup tables.
///
/// This is called once at the start of newGame(), AFTER cells[] are zeroed
/// and BEFORE pattern initialization. It does the following:
///
///   1. Set cells[].piece = WALL for all static walls and portal cells
///   2. Fill portalPartner[] for O(1) teleport lookup
///   3. Compute portalAffected[dir][pos] mask
///   4. Pre-compute portalUpdateZone[][] for move() extension
///
/// The key insight for portalAffected:
///   Walk from each portal cell outward in each direction (using portalStep,
///   which itself uses portalPartner — already filled in step 2). Any cell
///   within L steps of a portal has its window passing through that portal.
///
///   EXCEPTION: "gap cells" — cells strictly between two portals that happen
///   to be collinear in a direction. These cells would cause infinite loops
///   in portalStep (A→B→...→gap→A→B→...). Gap cells must NOT be marked as
///   portalAffected; they use the normal bitKey path where A and B appear
///   as WALL bits, which is correct (the line is blocked in that direction).
template <Rule R>
void Board::initPortals()
{
    constexpr int L = PatternConfig::HalfLineLen<R>;

    // --- Step 1: Reset portal data ---
    std::fill_n(portalPartner, FULL_BOARD_CELL_COUNT, Pos::PASS);
    std::memset(portalAffected, 0, sizeof(portalAffected));
    std::memset(portalUpdateZone, 0, sizeof(portalUpdateZone));

    // --- Step 2: Place static WALLs ---
    // (cells[] was already zeroed by newGame; boundary cells already set to WALL)
    for (Pos p = startPos(); p <= endPos(); p++) {
        if (wallMask[p])
            cells[p].piece = WALL;
    }

    // --- Step 3: Place portal cells as WALL and fill portalPartner[] ---
    for (int i = 0; i < numPortals; i++) {
        Pos a = portals[i].a;
        Pos b = portals[i].b;

        cells[a].piece  = WALL;
        cells[b].piece  = WALL;
        portalPartner[a] = b;
        portalPartner[b] = a;
    }

    // --- Step 4: Compute portalAffected[][] ---
    //
    // For each portal cell, walk outward in each direction up to L steps.
    // Each real cell we land on has its pattern window touching this portal → mark it.
    //
    // We use physical stepping (not portalStep) to avoid chaining through other portals
    // for the "does this cell's window reach a portal?" question. The cell's window
    // will use portalStep during buildPortalKey anyway — the mask just needs to be
    // conservative (may slightly over-mark, never under-mark).
    //
    // Actually, we need to think about this more carefully:
    // If cell C is near portal A, and C's window walks through A→B, and B is near
    // another portal D, the window might also pass through D. The cell IS affected
    // by both portals. But portalAffected only needs to be true/false — and
    // buildPortalKey handles ALL portals naturally via portalStep chaining.
    // So we just need to mark any cell within L physical steps of any portal.
    //
    // BUT: gap cells (between collinear portals in that direction) must NOT be marked.
    for (int i = 0; i < numPortals; i++) {
        for (int dir = 0; dir < 4; dir++) {
            Direction step = DIRECTION[dir];
            Pos       endpoints[2] = {portals[i].a, portals[i].b};

            for (Pos endpoint : endpoints) {
                // Walk outward in both positive and negative directions from the portal
                for (int sign : {-1, +1}) {
                    Pos cur = endpoint;
                    for (int dist = 1; dist <= L; dist++) {
                        cur = Pos(int16_t(int(cur) + int(step) * sign));

                        // Bounds check
                        if (int(cur) < 0 || int(cur) >= FULL_BOARD_CELL_COUNT)
                            break;

                        // Don't mark portal cells themselves
                        if (portalPartner[cur] != Pos::PASS)
                            continue;

                        // Skip if it's a boundary/wall cell
                        if (cells[cur].piece == WALL)
                            break;

                        // PORTAL BUG-005 FIX: Gap cell exclusion.
                        // A "gap cell" is a cell strictly between two portals A and B
                        // that are collinear in direction `dir`. These cells must NOT be
                        // marked portalAffected — their window uses the fast bitKey path
                        // where A and B appear as WALL bits, creating two isolated sub-lines.
                        // Without this check, buildPortalKey is called for gap cells,
                        // producing non-monotonic windows and false pattern counts.
                        // See PortalPair::isGapCell() in board.h for the exact predicate.
                        bool gap = false;
                        for (int j = 0; j < numPortals; j++) {
                            if (portals[j].isGapCell(cur, dir)) {
                                gap = true;
                                break;
                            }
                        }
                        if (gap)
                            continue;  // NOTE: continue, not break — cells further out may be valid

                        portalAffected[dir][int(cur)] = true;
                    }
                }
            }
        }
    }

    // --- Step 5: Pre-compute portalUpdateZone[][] ---
    //
    // When a stone is placed at position P and P is within L steps of portal A
    // in direction D, then cells near portal B (the partner) in direction D also
    // need their patterns refreshed. This is because placing a stone near A
    // changes the virtual line that passes through A→B for cells on B's side.
    //
    // For each (portal_pair, direction), we store the list of cells near the
    // PARTNER that also need refreshing when any cell near THIS endpoint changes.
    //
    // Strategy: for each portal pair i, direction dir:
    //   portalUpdateZone[i][dir] = all cells within L steps of BOTH endpoints
    //   that are portalAffected in this direction.
    // During move(), we check: if the placed stone pos has portalAffected[dir][pos],
    // then also update all cells in portalUpdateZone for EVERY portal pair in dir.
    //
    // This is a simplification: we update ALL portal zones in that direction,
    // not just the one nearest to pos. This is correct but slightly over-refreshes
    // (at most 2*L extra cells per portal pair — negligible for <=8 pairs).
    for (int i = 0; i < numPortals; i++) {
        for (int dir = 0; dir < 4; dir++) {
            PortalUpdateEntry &entry = portalUpdateZone[i][dir];
            entry.count = 0;

            Direction step = DIRECTION[dir];
            Pos       endpoints[2] = {portals[i].a, portals[i].b};

            for (Pos endpoint : endpoints) {
                for (int sign : {-1, +1}) {
                    Pos cur = endpoint;
                    for (int dist = 1; dist <= L; dist++) {
                        cur = Pos(int16_t(int(cur) + int(step) * sign));

                        if (int(cur) < 0 || int(cur) >= FULL_BOARD_CELL_COUNT)
                            break;
                        if (cells[cur].piece == WALL)
                            break;
                        if (!portalAffected[dir][int(cur)])
                            continue;

                        // Check for duplicates (a cell might be near both endpoints)
                        bool dup = false;
                        for (int k = 0; k < entry.count; k++) {
                            if (entry.cells[k] == cur) { dup = true; break; }
                        }
                        if (!dup && entry.count < 20) {
                            entry.cells[entry.count++] = cur;
                        }
                    }
                }
            }
        }
    }
}

// Explicit instantiation
template void Board::initPortals<FREESTYLE>();
template void Board::initPortals<STANDARD>();
template void Board::initPortals<RENJU>();

// =============================================================================
// newGame<R>() — board initialization
// =============================================================================

template <Rule R>
void Board::newGame()
{
    // Zero out cells and bitkeys
    std::fill_n(cells, FULL_BOARD_CELL_COUNT, Cell {});
    std::fill_n(bitKey0, arraySize(bitKey0), 0);
    std::fill_n(bitKey1, arraySize(bitKey1), 0);
    std::fill_n(bitKey2, arraySize(bitKey2), 0);
    std::fill_n(bitKey3, arraySize(bitKey3), 0);

    // Init board state to empty
    moveCount         = 0;
    passCount[BLACK]  = 0;
    passCount[WHITE]  = 0;
    currentSide       = BLACK;
    currentZobristKey = Hash::zobrist[BLACK][FULL_BOARD_CELL_COUNT - 1];
    for (Pos i = Pos::FULL_BOARD_START; i < Pos::FULL_BOARD_END; i++) {
        cells[i].piece = i.isInBoard(boardSize, boardSize) ? EMPTY : WALL;

        if (cells[i].piece == EMPTY) {
            setBitKey(i, BLACK);
            setBitKey(i, WHITE);
        }
    }

    // PORTAL: Initialize portal structures and place WALL/portal cells.
    // This must happen AFTER cells[] boundary init, BEFORE pattern computation,
    // because initPortals sets cells[portal].piece = WALL and fills portalPartner[].
    // The bitKeys already have boundary WALL bits (absence of setBitKey calls for
    // boundary cells = zero bits = correct encoding since 00=EMPTY, but boundary
    // cells are outside the bitKey active range anyway).
    // Portal cells inside the board: cells[].piece was set to EMPTY above, then
    // initPortals sets it to WALL. BUT we already called setBitKey for them (marking
    // them as EMPTY in the bitKey). We need to fix this.
    initPortals<R>();

    // PORTAL: Fix bitKeys for portal and wall cells that were incorrectly
    // marked EMPTY. In Rapfi's encoding: 0b11=EMPTY, 0b00=WALL.
    // The init loop above called setBitKey(BLACK) + setBitKey(WHITE) for ALL
    // in-board cells, giving them 0b11 (EMPTY). After initPortals set some
    // cells to WALL, their bitKey must be corrected: flip both bits back to 0.
    for (Pos p = Pos::FULL_BOARD_START; p < Pos::FULL_BOARD_END; p++) {
        if (cells[p].piece == WALL && p.isInBoard(boardSize, boardSize)) {
            flipBitKey(p, BLACK);  // XOR 0b01 → 0b11 ^ 0b01 = 0b10
            flipBitKey(p, WHITE);  // XOR 0b10 → 0b10 ^ 0b10 = 0b00 = WALL ✓
        }
    }

    // Init state info of the first ply with rule R
    StateInfo &st = stateInfos[moveCount];
    std::memset(&st, 0, sizeof(StateInfo));

    Value valueBlack = VALUE_ZERO;
    FOR_EVERY_POSITION(this, pos)
    {
        Cell &c = cells[pos];

        for (int dir = 0; dir < 4; dir++) {
            c.pattern2x[dir] = PatternConfig::lookupPattern<R>(getKeyAt<R>(pos, dir));

            assert(c.pattern2x[dir].patBlack <= F1);
            assert(c.pattern2x[dir].patWhite <= F1);
        }

        PatternCode pcode[SIDE_NB] = {c.pcode<BLACK>(), c.pcode<WHITE>()};
        c.updatePattern4AndScore<R>(pcode[BLACK], pcode[WHITE]);
        st.p4Count[BLACK][c.pattern4[BLACK]]++;
        st.p4Count[WHITE][c.pattern4[WHITE]]++;
        valueBlack += c.valueBlack = Config::getValueBlack(R, pcode[BLACK], pcode[WHITE]);
    }
    st.valueBlack = valueBlack;
    st.candArea   = CandArea();

    // For full board candidate range, manually set all empty cells to candidates.
    if (candidateRangeSize == 0)
        expandCandArea(centerPos(), size() / 2, 0);

    assert(checkP4(this));

    // Reset evaluator state to empty board
    if (evaluator_)
        evaluator_->initEmptyBoard();
}

template void Board::newGame<FREESTYLE>();
template void Board::newGame<STANDARD>();
template void Board::newGame<RENJU>();

// =============================================================================
// move<R, MT>() — make a move with portal-extended update zone
// =============================================================================


template <Rule R, Board::MoveType MT>
void Board::move(Pos pos)
{
    // Handle PASS move
    if (UNLIKELY(pos == Pos::PASS)) {
        assert(passMoveCount() < cellCount());

        StateInfo &st = stateInfos[++moveCount];
        st            = stateInfos[moveCount - 1];
        st.lastMove   = Pos::PASS;

        passCount[currentSide]++;
        currentSide = ~currentSide;

        if (MT == MoveType::NORMAL && evaluator_)
            evaluator_->afterPass(*this);
        return;
    }

    assert(pos.valid());
    assert(isEmpty(pos));

    // Before move evaluator update
    if (MT == MoveType::NORMAL && evaluator_)
        evaluator_->beforeMove(*this, pos);

    UpdateCache &pc = updateCache[moveCount];
    StateInfo   &st = stateInfos[++moveCount];
    st              = stateInfos[moveCount - 1];
    st.lastMove     = pos;
    st.candArea.expand(pos, boardSize, candAreaExpandDist);

    cells[pos].piece = currentSide;
    currentZobristKey ^= Hash::zobrist[currentSide][pos];
    flipBitKey(pos, currentSide);

    Value deltaValueBlack            = VALUE_ZERO;
    int   f4CountBeforeMove[SIDE_NB] = {p4Count(BLACK, B_FLEX4), p4Count(WHITE, B_FLEX4)};
    int   updateCacheIdx             = 0;

    constexpr int L = PatternConfig::HalfLineLen<R>;

    // PORTAL: Check if placed stone pos is near any portal in any direction.
    // We determine this by checking portalAffected for this pos.
    // If so, we cannot use the fast bitKey sliding window trick for neighbors
    // that are portalAffected. Instead, we use getKeyAt per cell (which
    // dispatches to buildPortalKey for portalAffected cells).
    //
    // Strategy: We ALWAYS use per-cell getKeyAt for the update loop.
    // For non-portal boards, getKeyAt fast-path (single rotr) is still very fast.
    // For portal boards, portalAffected cells use buildPortalKey automatically.
    //
    // This simplifies the code vs trying to use the sliding bitKey trick and
    // then patching up portal cells separately.

    for (int i = -L; i <= L; i += 1 + (i == -1)) {
        for (int dir = 0; dir < 4; dir++) {
            Pos   posi = pos + DIRECTION[dir] * i;
            Cell &c    = cells[posi];
            if (c.piece != EMPTY)
                continue;

            if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                deltaValueBlack -= c.valueBlack;
            }

            // PORTAL: Use getKeyAt which handles both fast-path and portal-path
            c.pattern2x[dir] = PatternConfig::lookupPattern<R>(getKeyAt<R>(posi, dir));

            pc[updateCacheIdx].pattern4[BLACK] = c.pattern4[BLACK];
            pc[updateCacheIdx].pattern4[WHITE] = c.pattern4[WHITE];
            pc[updateCacheIdx].score[BLACK]    = c.score[BLACK];
            pc[updateCacheIdx].score[WHITE]    = c.score[WHITE];
            if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                pc[updateCacheIdx].valueBlack = c.valueBlack;
            }
            updateCacheIdx++;

            PatternCode pcode[SIDE_NB] = {c.pcode<BLACK>(), c.pcode<WHITE>()};

            if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                deltaValueBlack += c.valueBlack =
                    Config::getValueBlack(R, pcode[BLACK], pcode[WHITE]);
            }

            st.p4Count[BLACK][c.pattern4[BLACK]]--;
            st.p4Count[WHITE][c.pattern4[WHITE]]--;
            c.updatePattern4AndScore<R>(pcode[BLACK], pcode[WHITE]);
            st.p4Count[BLACK][c.pattern4[BLACK]]++;
            st.p4Count[WHITE][c.pattern4[WHITE]]++;

            if (c.pattern4[BLACK] >= C_BLOCK4_FLEX3)
                st.lastPattern4Move[BLACK][c.pattern4[BLACK] - C_BLOCK4_FLEX3] = posi;
            if (c.pattern4[WHITE] >= C_BLOCK4_FLEX3)
                st.lastPattern4Move[WHITE][c.pattern4[WHITE] - C_BLOCK4_FLEX3] = posi;
        }
    }

    // PORTAL: Extended update zone — refresh cells near portal partners
    //
    // When a stone is placed near portal A, cells near portal B (partner) also
    // need their virtual-line patterns refreshed because the virtual line through
    // B→A now has a new stone on A's side.
    //
    // We iterate through pre-computed portalUpdateZone lists. For each cell in
    // the list, we check if it was NOT already updated in the main loop above
    // (to avoid double-counting in p4Count). A cell was already updated if it
    // is within L physical steps of pos in some direction.
    if (numPortals > 0) {
        for (int i = 0; i < numPortals; i++) {
            for (int dir = 0; dir < 4; dir++) {
                // Check if pos is near either endpoint of this portal in this direction
                int distA = physicalDistAlongDir(pos, portals[i].a, dir);
                int distB = physicalDistAlongDir(pos, portals[i].b, dir);
                bool posNearA = (distA >= 0 && distA <= L);
                bool posNearB = (distB >= 0 && distB <= L);

                if (!posNearA && !posNearB)
                    continue;

                // Refresh all cells in the update zone for this (portal, dir)
                const PortalUpdateEntry &entry = portalUpdateZone[i][dir];
                for (int k = 0; k < entry.count; k++) {
                    Pos cellPos = entry.cells[k];

                    // PORTAL: Skip if this cell was already handled by the main
                    // loop. The main loop walks L steps in ALL 4 directions from
                    // pos. We must check ALL directions here — not just `dir` —
                    // because a cell like (4,3) may be on the anti-diagonal of pos
                    // even though we are currently updating its H pattern.
                    // Failing to check all directions causes double-processing:
                    // move() writes 2 cache entries for the cell, undo() reads 2,
                    // but undo() restores pattern4 twice, leaving a stale value.
                    bool alreadyInMainLoop = false;
                    int dist = physicalDistAlongDir(pos, cellPos, dir);
                    if (dist >= 0 && dist <= L) {
                        alreadyInMainLoop = true;
                    }
                    if (alreadyInMainLoop)
                        continue;

                    Cell &c = cells[cellPos];
                    if (c.piece != EMPTY)
                        continue;

                    // Refresh this single direction for this cell
                    if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                        deltaValueBlack -= c.valueBlack;
                    }

                    c.pattern2x[dir] = PatternConfig::lookupPattern<R>(
                        getKeyAt<R>(cellPos, dir));

                    pc[updateCacheIdx].pattern4[BLACK] = c.pattern4[BLACK];
                    pc[updateCacheIdx].pattern4[WHITE] = c.pattern4[WHITE];
                    pc[updateCacheIdx].score[BLACK]    = c.score[BLACK];
                    pc[updateCacheIdx].score[WHITE]    = c.score[WHITE];
                    if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                        pc[updateCacheIdx].valueBlack = c.valueBlack;
                    }
                    updateCacheIdx++;

                    PatternCode pcode[SIDE_NB] = {c.pcode<BLACK>(), c.pcode<WHITE>()};

                    if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                        deltaValueBlack += c.valueBlack =
                            Config::getValueBlack(R, pcode[BLACK], pcode[WHITE]);
                    }

                    st.p4Count[BLACK][c.pattern4[BLACK]]--;
                    st.p4Count[WHITE][c.pattern4[WHITE]]--;
                    c.updatePattern4AndScore<R>(pcode[BLACK], pcode[WHITE]);
                    st.p4Count[BLACK][c.pattern4[BLACK]]++;
                    st.p4Count[WHITE][c.pattern4[WHITE]]++;

                    if (c.pattern4[BLACK] >= C_BLOCK4_FLEX3)
                        st.lastPattern4Move[BLACK][c.pattern4[BLACK] - C_BLOCK4_FLEX3] = cellPos;
                    if (c.pattern4[WHITE] >= C_BLOCK4_FLEX3)
                        st.lastPattern4Move[WHITE][c.pattern4[WHITE] - C_BLOCK4_FLEX3] = cellPos;
                }
            }
        }
    }

    const Cell &c = cell(pos);
    if (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
        st.valueBlack += deltaValueBlack - c.valueBlack;
    }
    st.p4Count[BLACK][c.pattern4[BLACK]]--;
    st.p4Count[WHITE][c.pattern4[WHITE]]--;

    if (MT != MoveType::NO_EVAL_MULTI)
        currentSide = ~currentSide;

    assert(checkP4(this));
    assert(updateCacheIdx <= std::tuple_size_v<UpdateCache>);

    for (size_t i = 0; i < candidateRangeSize; i++)
        cells[pos + candidateRange[i]].cand++;

    for (Color c : {BLACK, WHITE}) {
        if (!f4CountBeforeMove[c] && p4Count(c, B_FLEX4))
            st.lastFlex4AttackMove[c] = pos;
    }

    // After move evaluator update
    if (MT == MoveType::NORMAL && evaluator_)
        evaluator_->afterMove(*this, pos);
}

template void Board::move<FREESTYLE, Board::MoveType::NORMAL>(Pos pos);
template void Board::move<FREESTYLE, Board::MoveType::NO_EVAL>(Pos pos);
template void Board::move<STANDARD, Board::MoveType::NORMAL>(Pos pos);
template void Board::move<STANDARD, Board::MoveType::NO_EVAL>(Pos pos);
template void Board::move<RENJU, Board::MoveType::NORMAL>(Pos pos);
template void Board::move<RENJU, Board::MoveType::NO_EVAL>(Pos pos);

// =============================================================================
// undo<R, MT>() — undo a move with portal-extended rollback
// =============================================================================

template <Rule R, Board::MoveType MT>
void Board::undo()
{
    assert(moveCount > 0);
    Pos lastPos = getLastMove();

    // Handle PASS undo
    if (UNLIKELY(lastPos == Pos::PASS)) {
        currentSide = ~currentSide;
        assert(passCount[currentSide] > 0);
        passCount[currentSide]--;
        moveCount--;

        if (MT == MoveType::NORMAL && evaluator_)
            evaluator_->afterUndoPass(*this);
        return;
    }

    // Before undo evaluator update
    if (MT == MoveType::NORMAL && evaluator_)
        evaluator_->beforeUndo(*this, lastPos);

    if (MT != MoveType::NO_EVAL_MULTI)
        currentSide = ~currentSide;
    assert(get(lastPos) == currentSide);

    flipBitKey(lastPos, currentSide);
    currentZobristKey ^= Hash::zobrist[currentSide][lastPos];
    cells[lastPos].piece = EMPTY;

    moveCount--;
    const UpdateCache &pc             = updateCache[moveCount];
    int                updateCacheIdx = 0;

    constexpr int L = PatternConfig::HalfLineLen<R>;

    // Main undo loop — mirrors move() main loop
    for (int i = -L; i <= L; i += 1 + (i == -1)) {
        for (int dir = 0; dir < 4; dir++) {
            Pos   posi = lastPos + DIRECTION[dir] * i;
            Cell &c    = cells[posi];
            if (c.piece != EMPTY)
                continue;

            // PORTAL: Use getKeyAt (dual-path) to restore pattern
            c.pattern2x[dir]  = PatternConfig::lookupPattern<R>(getKeyAt<R>(posi, dir));
            PatternCode pcode[SIDE_NB] = {c.pcode<BLACK>(), c.pcode<WHITE>()};
            c.updatePattern4AndScore<R>(pcode[BLACK], pcode[WHITE]);
            if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                c.valueBlack = Config::getValueBlack(R, pcode[BLACK], pcode[WHITE]);
            }
            updateCacheIdx++;
        }
    }

    // PORTAL: Extended undo zone — restore cells near portal partners
    // Must mirror the exact same iteration order as move() to match updateCacheIdx
    if (numPortals > 0) {
        for (int i = 0; i < numPortals; i++) {
            for (int dir = 0; dir < 4; dir++) {
                int distA = physicalDistAlongDir(lastPos, portals[i].a, dir);
                int distB = physicalDistAlongDir(lastPos, portals[i].b, dir);
                bool posNearA = (distA >= 0 && distA <= L);
                bool posNearB = (distB >= 0 && distB <= L);

                if (!posNearA && !posNearB)
                    continue;

                const PortalUpdateEntry &entry = portalUpdateZone[i][dir];
                for (int k = 0; k < entry.count; k++) {
                    Pos cellPos = entry.cells[k];

                    // PORTAL: Same all-direction check as move() — must mirror
                    // exactly so updateCacheIdx stays in sync.
                    bool alreadyInMainLoop = false;
                    int dist = physicalDistAlongDir(lastPos, cellPos, dir);
                    if (dist >= 0 && dist <= L) {
                        alreadyInMainLoop = true;
                    }
                    if (alreadyInMainLoop)
                        continue;

                    Cell &c = cells[cellPos];
                    if (c.piece != EMPTY)
                        continue;

                    c.pattern2x[dir]  = PatternConfig::lookupPattern<R>(
                        getKeyAt<R>(cellPos, dir));
                    PatternCode pcode[SIDE_NB] = {c.pcode<BLACK>(), c.pcode<WHITE>()};
                    c.updatePattern4AndScore<R>(pcode[BLACK], pcode[WHITE]);
                    if constexpr (MT == MoveType::NORMAL || MT == MoveType::NO_EVALUATOR) {
                        c.valueBlack = Config::getValueBlack(R, pcode[BLACK], pcode[WHITE]);
                    }
                    updateCacheIdx++;
                }
            }
        }
    }

    assert(checkP4(this));
    assert(updateCacheIdx <= std::tuple_size_v<UpdateCache>);

    for (size_t i = 0; i < candidateRangeSize; i++)
        cells[lastPos + candidateRange[i]].cand--;

    // After undo evaluator update
    if (MT == MoveType::NORMAL && evaluator_)
        evaluator_->afterUndo(*this, lastPos);
}

template void Board::undo<FREESTYLE, Board::MoveType::NORMAL>();
template void Board::undo<FREESTYLE, Board::MoveType::NO_EVAL>();
template void Board::undo<STANDARD, Board::MoveType::NORMAL>();
template void Board::undo<STANDARD, Board::MoveType::NO_EVAL>();
template void Board::undo<RENJU, Board::MoveType::NORMAL>();
template void Board::undo<RENJU, Board::MoveType::NO_EVAL>();

// =============================================================================
// checkForbiddenPoint — unchanged from Rapfi
// =============================================================================

bool Board::checkForbiddenPoint(Pos pos) const
{
    const Cell &fpCell = cell(pos);
    if (fpCell.pattern4[BLACK] != FORBID)
        return false;

    int winByFour = 0;
    for (int dir = 0; dir < 4; dir++) {
        if (fpCell.pattern2x[dir].patBlack == OL)
            return true;
        else if (fpCell.pattern2x[dir].patBlack == B4 || fpCell.pattern2x[dir].patBlack == F4) {
            if (++winByFour >= 2)
                return true;
        }
    }

    Board &board    = const_cast<Board &>(*this);
    Color  prevSide = board.currentSide;

    board.currentSide = BLACK;
    board.move<Rule::RENJU, MoveType::NO_EVAL_MULTI>(pos);

    constexpr int MaxFindDist = 4;
    int           winByThree  = 0;

    for (int dir = 0; dir < 4; dir++) {
        Pattern p = fpCell.pattern2x[dir].patBlack;

        if (p != F3 && p != F3S)
            continue;

        Pos posi = pos;
        for (int i = 0; i < MaxFindDist; i++) {
            posi = portalStep(posi, dir, -1);

            if (const Cell &c = cell(posi); c.piece == EMPTY) {
                if (c.pattern4[BLACK] == B_FLEX4 || c.pattern(BLACK, dir) == F5
                    || c.pattern4[BLACK] == FORBID && c.pattern(BLACK, dir) == F4
                           && !checkForbiddenPoint(posi)) {
                    winByThree++;
                    goto next_direction;
                }
                break;
            }
            else if (c.piece != BLACK)
                break;
        }
        posi = pos;
        for (int i = 0; i < MaxFindDist; i++) {
            posi = portalStep(posi, dir, 1);

            if (const Cell &c = cell(posi); c.piece == EMPTY) {
                if (c.pattern4[BLACK] == B_FLEX4 || c.pattern(BLACK, dir) == F5
                    || c.pattern4[BLACK] == FORBID && c.pattern(BLACK, dir) == F4
                           && !checkForbiddenPoint(posi)) {
                    winByThree++;
                    goto next_direction;
                }
                break;
            }
            else if (c.piece != BLACK)
                break;
        }

    next_direction:
        if (winByThree >= 2)
            break;
    }

    board.undo<Rule::RENJU, MoveType::NO_EVAL_MULTI>();
    board.currentSide = prevSide;

    return winByThree >= 2;
}

// =============================================================================
// Utility functions — unchanged from Rapfi
// =============================================================================

Pos Board::getLastActualMoveOfSide(Color side) const
{
    assert(side == BLACK || side == WHITE);

    for (int reverseIdx = 0; reverseIdx < moveCount; reverseIdx++) {
        Pos move = getRecentMove(reverseIdx);
        if (move == Pos::PASS)
            continue;
        if (get(move) == side)
            return move;
    }

    return Pos::NONE;
}

void Board::expandCandArea(Pos pos, int fillDist, int lineDist)
{
    CandArea &area = stateInfos[moveCount].candArea;
    int       x = pos.x(), y = pos.y();

    auto candCondition = [&](Pos p) {
        return p >= 0 && p < FULL_BOARD_CELL_COUNT && isEmpty(p) && !cell(p).isCandidate();
    };

    area.expand(pos, boardSize, std::max(fillDist, lineDist));

    for (int dir = 0; dir < 4; dir++) {
        Pos posi1 = pos;
        Pos posi2 = pos;
        for (int i = 1; i <= lineDist; i++) {
            posi1 = portalStep(posi1, dir, -1);
            posi2 = portalStep(posi2, dir, 1);
            
            if (i >= std::max(3, fillDist + 1)) {
                if (candCondition(posi1)) cells[posi1].cand++;
                if (candCondition(posi2)) cells[posi2].cand++;
            }
        }
    }
    for (int xi = -fillDist; xi <= fillDist; xi++) {
        for (int yi = -fillDist; yi <= fillDist; yi++) {
            Pos posi {x + xi, y + yi};
            if (candCondition(posi))
                cells[posi].cand++;
        }
    }
}

std::string Board::positionString() const
{
    std::stringstream ss;
    for (int i = 0; i < ply(); i++) {
        Pos pos = getHistoryMove(i);
        if (pos == Pos::PASS)
            ss << "--";
        else
            ss << char('a' + pos.x()) << (1 + pos.y());
    }
    return ss.str();
}

std::string Board::trace() const
{
    std::stringstream ss;
    const StateInfo  &st = stateInfo();

    ss << "Hash: " << std::hex << zobristKey() << std::dec << '\n';
    ss << "Ply: " << ply() << "\n";
    ss << "NonPassCount: " << nonPassMoveCount() << "\n";
    ss << "PassCount[Black]: " << passMoveCountOfSide(BLACK)
       << "  PassCount[White]: " << passMoveCountOfSide(WHITE) << "\n";
    ss << "SideToMove: " << sideToMove() << "\n";
    ss << "LastPos: " << getLastMove() << '\n';
    ss << "Eval[Black]: " << st.valueBlack << '\n';
    ss << "LastP4[Black][A]: " << st.lastPattern4(BLACK, A_FIVE)
       << "  LastP4[White][A]: " << st.lastPattern4(WHITE, A_FIVE) << '\n';
    ss << "LastP4[Black][B]: " << st.lastPattern4(BLACK, B_FLEX4)
       << "  LastP4[White][B]: " << st.lastPattern4(WHITE, B_FLEX4) << '\n';
    ss << "LastP4[Black][C]: " << st.lastPattern4(BLACK, C_BLOCK4_FLEX3)
       << "  LastP4[White][C]: " << st.lastPattern4(WHITE, C_BLOCK4_FLEX3) << '\n';
    ss << "LastF4[Black]: " << st.lastFlex4AttackMove[BLACK]
       << "  LastF4[White]: " << st.lastFlex4AttackMove[WHITE] << '\n';

    // PORTAL: Enhanced board printing to show portal/wall cells
    auto printBoard = [&](auto &&posTextFunc, int textWidth = 1) {
        FOR_EVERY_POSITION(this, pos)
        {
            int x = pos.x(), y = pos.y();
            if (x != 0 || y != 0)
                ss << ' ';
            if (x == 0 && y != 0)
                ss << '\n';
            posTextFunc(pos);
            if (x == size() - 1)
                ss << ' ' << y + 1;
        }
        ss << '\n';
        for (int x = 0; x < size(); x++)
            ss << std::setw(textWidth) << char(x + 65) << " ";
        ss << '\n';
    };

    auto printPiece = [&](Pos pos) {
        // PORTAL: Show portal cells as 'P' and wall cells as '#'
        if (isPortalCell(pos)) {
            ss << 'P';
        }
        else {
            switch (get(pos)) {
            case BLACK: ss << 'X'; break;
            case WHITE: ss << 'O'; break;
            case EMPTY: ss << (cell(pos).isCandidate() ? '*' : '.'); break;
            default: ss << '#'; break;  // PORTAL: WALL shown as '#'
            }
        }
    };

    ss << "----------------Board----------------\n";
    // PORTAL: Show portal/wall info
    if (numPortals > 0) {
        ss << "Portals: " << numPortals << " pair(s)\n";
        for (int i = 0; i < numPortals; i++) {
            ss << "  P" << i << ": (" << portals[i].a.x() << "," << portals[i].a.y()
               << ") <-> (" << portals[i].b.x() << "," << portals[i].b.y() << ")\n";
        }
    }
    printBoard(printPiece);

    ss << "----------Pattern4----Black----------\n";
    printBoard([&](Pos pos) {
        if (isEmpty(pos))
            ss << cell(pos).pattern4[BLACK];
        else
            ss << '.';
    });

    ss << "----------Pattern4----White----------\n";
    printBoard([&](Pos pos) {
        if (isEmpty(pos))
            ss << cell(pos).pattern4[WHITE];
        else
            ss << '.';
    });

    ss << "----------Score-------Black----------\n";
    printBoard(
        [&](Pos pos) {
            if (isEmpty(pos))
                ss << std::setw(3) << cell(pos).score[BLACK];
            else {
                ss << '[';
                printPiece(pos);
                ss << ']';
            }
        },
        3);

    ss << "----------Score-------White----------\n";
    printBoard(
        [&](Pos pos) {
            if (isEmpty(pos))
                ss << std::setw(3) << cell(pos).score[WHITE];
            else {
                ss << '[';
                printPiece(pos);
                ss << ']';
            }
        },
        3);

    if (evaluator_) {
        Evaluation::PolicyBuffer policyBuf(boardSize);
        policyBuf.setComputeFlagForAllEmptyCell(*this);

        evaluator_->evaluatePolicy(*this, policyBuf);

        ss << "----------Policy------Self-----------\n";
        printBoard(
            [&](Pos pos) {
                if (isEmpty(pos))
                    ss << std::setw(4) << policyBuf.score(pos);
                else {
                    ss << " [";
                    printPiece(pos);
                    ss << "]";
                }
            },
            4);

        const_cast<Board *>(this)->flipSide();
        evaluator_->evaluatePolicy(*this, policyBuf);
        const_cast<Board *>(this)->flipSide();

        ss << "----------Policy------Oppo-----------\n";
        printBoard(
            [&](Pos pos) {
                if (isEmpty(pos))
                    ss << std::setw(4) << policyBuf.score(pos);
                else {
                    ss << " [";
                    printPiece(pos);
                    ss << "]";
                }
            },
            4);
    }

    return ss.str();
}
