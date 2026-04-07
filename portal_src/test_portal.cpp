/*
 *  Portal Gomoku Engine — Board Logic Test
 *
 *  Tests:
 *   1. Basic board setup (no portals) — sanity check
 *   2. WALL placement — cells blocked
 *   3. Portal setup — portalPartner lookup
 *   4. portalAffected mask — correct marking
 *   5. buildPortalKey — virtual line through portals
 *   6. Pattern detection through portal (win cases from design doc)
 *   7. move() / undo() with portal — incremental update correctness
 *   8. Non-collinear portal (A=(3,6), B=(7,3)) — all 4 directions
 *   9. Gap cell verification (collinear portals)
 */

#include "game/board.h"
#include "game/pattern.h"
#include "config.h"
#include "core/iohelper.h"
#include "core/pos.h"
#include "core/types.h"

#include <cassert>
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

// Pattern name lookup for printing
static const char *patternName(Pattern p)
{
    switch (p) {
    case DEAD: return "DEAD";
    case OL:   return "OL";
    case B1:   return "B1";
    case F1:   return "F1";
    case B2:   return "B2";
    case F2:   return "F2";
    case F2A:  return "F2A";
    case F2B:  return "F2B";
    case B3:   return "B3";
    case F3:   return "F3";
    case F3S:  return "F3S";
    case B4:   return "B4";
    case F4:   return "F4";
    case F5:   return "F5";
    default:   return "???";
    }
}

static const char *pattern4Name(Pattern4 p)
{
    switch (p) {
    case NONE:            return "NONE";
    case FORBID:          return "FORBID";
    case L_FLEX2:         return "L_FLEX2";
    case K_BLOCK3:        return "K_BLOCK3";
    case J_FLEX2_2X:      return "J_FLEX2_2X";
    case I_BLOCK3_PLUS:   return "I_BLOCK3_PLUS";
    case H_FLEX3:         return "H_FLEX3";
    case G_FLEX3_PLUS:    return "G_FLEX3_PLUS";
    case F_FLEX3_2X:      return "F_FLEX3_2X";
    case E_BLOCK4:        return "E_BLOCK4";
    case D_BLOCK4_PLUS:   return "D_BLOCK4_PLUS";
    case C_BLOCK4_FLEX3:  return "C_BLOCK4_FLEX3";
    case B_FLEX4:         return "B_FLEX4";
    case A_FIVE:          return "A_FIVE";
    default:              return "???";
    }
}

static const char *dirName(int dir)
{
    switch (dir) {
    case 0: return "H";
    case 1: return "V";
    case 2: return "/";
    case 3: return "\\";
    default: return "?";
    }
}

/// Print pattern info for a specific cell
static void printCellInfo(const Board &board, Pos pos)
{
    if (!board.isEmpty(pos)) {
        std::cout << "  (" << pos.x() << "," << pos.y() << ") piece="
                  << (board.get(pos) == BLACK ? "BLACK" : board.get(pos) == WHITE ? "WHITE"
                      : board.get(pos) == WALL ? "WALL" : "?") << "\n";
        return;
    }
    const Cell &c = board.cell(pos);
    std::cout << "  (" << pos.x() << "," << pos.y() << ")";
    std::cout << " p4[B]=" << pattern4Name(c.pattern4[BLACK]);
    std::cout << " p4[W]=" << pattern4Name(c.pattern4[WHITE]);
    std::cout << " pat[B]:";
    for (int d = 0; d < 4; d++)
        std::cout << " " << dirName(d) << "=" << patternName(c.pattern2x[d].patBlack);
    std::cout << "\n";
}

/// Print the board visually (compact)
static void printBoard(const Board &board, int sz)
{
    std::cout << "   ";
    for (int x = 0; x < sz; x++) std::cout << (x % 10) << " ";
    std::cout << "\n";

    for (int y = 0; y < sz; y++) {
        std::cout << (y < 10 ? " " : "") << y << " ";
        for (int x = 0; x < sz; x++) {
            Pos p {x, y};
            if (board.isPortalCell(p))      std::cout << "P ";
            else if (board.isWallCell(p))    std::cout << "# ";
            else if (board.get(p) == BLACK)  std::cout << "X ";
            else if (board.get(p) == WHITE)  std::cout << "O ";
            else if (board.get(p) == WALL)   std::cout << "# ";
            else                             std::cout << ". ";
        }
        std::cout << y << "\n";
    }
    std::cout << "   ";
    for (int x = 0; x < sz; x++) std::cout << (x % 10) << " ";
    std::cout << "\n";
}

static int testsPassed = 0;
static int testsFailed = 0;

#define CHECK(cond, msg)                                              \
    do {                                                              \
        if (!(cond)) {                                                \
            std::cout << "  FAIL: " << msg << " [" #cond "]\n";      \
            testsFailed++;                                            \
        } else {                                                      \
            std::cout << "  PASS: " << msg << "\n";                   \
            testsPassed++;                                            \
        }                                                             \
    } while (0)

// ============================================================================
// TEST 1: Basic board (no portals) — sanity check
// ============================================================================
static void test1_basic_board()
{
    std::cout << "\n=== TEST 1: Basic Board (no portals) ===\n";

    Board board(15);
    board.newGame<FREESTYLE>();

    CHECK(board.size() == 15, "board size = 15");
    CHECK(board.isEmpty(Pos{7, 7}), "center is empty");
    CHECK(board.get(Pos{7, 7}) == EMPTY, "center piece = EMPTY");
    CHECK(board.portalCount() == 0, "no portals");

    // Place a stone and check pattern
    board.move<FREESTYLE>(Pos{7, 7});
    CHECK(board.get(Pos{7, 7}) == BLACK, "center = BLACK after move");
    CHECK(board.ply() == 1, "ply = 1");

    // Undo
    board.undo<FREESTYLE>();
    CHECK(board.isEmpty(Pos{7, 7}), "center empty after undo");
    CHECK(board.ply() == 0, "ply = 0 after undo");

    std::cout << "  [basic board works]\n";
}

// ============================================================================
// TEST 2: WALL placement
// ============================================================================
static void test2_wall()
{
    std::cout << "\n=== TEST 2: WALL Placement ===\n";

    Board board(10);
    board.addWall(Pos{5, 5});
    board.addWall(Pos{3, 3});
    board.newGame<FREESTYLE>();

    CHECK(board.get(Pos{5, 5}) == WALL, "WALL at (5,5)");
    CHECK(board.get(Pos{3, 3}) == WALL, "WALL at (3,3)");
    CHECK(board.isWallCell(Pos{5, 5}), "isWallCell(5,5) = true");
    CHECK(!board.isEmpty(Pos{5, 5}), "WALL cell not empty");
    CHECK(board.isEmpty(Pos{4, 4}), "non-wall cell IS empty");

    // Check that pattern near wall shows blocked line
    printCellInfo(board, Pos{4, 5});
    printCellInfo(board, Pos{6, 5});

    std::cout << "  [WALL placement works]\n";
}

// ============================================================================
// TEST 3: Portal setup
// ============================================================================
static void test3_portal_setup()
{
    std::cout << "\n=== TEST 3: Portal Setup ===\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    CHECK(board.portalCount() == 1, "1 portal pair");
    CHECK(board.get(Pos{3, 6}) == WALL, "portal A is WALL");
    CHECK(board.get(Pos{7, 3}) == WALL, "portal B is WALL");
    CHECK(board.isPortalCell(Pos{3, 6}), "isPortalCell(3,6)");
    CHECK(board.isPortalCell(Pos{7, 3}), "isPortalCell(7,3)");
    CHECK(!board.isPortalCell(Pos{5, 5}), "non-portal cell");

    printBoard(board, 15);
}

// ============================================================================
// TEST 4: portalAffected mask
// ============================================================================
static void test4_portal_affected()
{
    std::cout << "\n=== TEST 4: portalAffected Mask (Diagnostic) ===\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Portal A=(3,6), B=(7,3). L=4. FREESTYLE.
    // H-direction: cells within 4 steps of A or B should be marked.
    // A=(3,6) left:  (2,6),(1,6),(0,6)  (3 cells before boundary)
    // A=(3,6) right: (4,6),(5,6),(6,6),(7,6)  (4 cells)
    // B=(7,3) left:  (6,3),(5,3),(4,3),(3,3)
    // B=(7,3) right: (8,3),(9,3),(10,3),(11,3)

    std::cout << "  --- H-direction (dir=0) ---\n";
    std::cout << "  Row 6 (near portal A=(3,6)):\n";
    for (int x = 0; x < 15; x++) {
        Pos p{x, 6};
        bool affected = board.isPortalAffected(p, 0);
        bool isPortal = board.isPortalCell(p);
        std::cout << "    (" << x << ",6): affected=" << affected
                  << (isPortal ? " [PORTAL]" : "") << "\n";
    }

    std::cout << "  Row 3 (near portal B=(7,3)):\n";
    for (int x = 0; x < 15; x++) {
        Pos p{x, 3};
        bool affected = board.isPortalAffected(p, 0);
        bool isPortal = board.isPortalCell(p);
        std::cout << "    (" << x << ",3): affected=" << affected
                  << (isPortal ? " [PORTAL]" : "") << "\n";
    }

    // Check V-direction (dir=1)
    std::cout << "  --- V-direction (dir=1) ---\n";
    std::cout << "  Col 3 (near portal A=(3,6)):\n";
    for (int y = 0; y < 15; y++) {
        Pos p{3, y};
        bool affected = board.isPortalAffected(p, 1);
        bool isPortal = board.isPortalCell(p);
        std::cout << "    (3," << y << "): affected=" << affected
                  << (isPortal ? " [PORTAL]" : "") << "\n";
    }

    // Check specific cells that SHOULD be affected
    CHECK(board.isPortalAffected(Pos{0, 6}, 0), "portalAffected[H][(0,6)] = true");
    CHECK(board.isPortalAffected(Pos{1, 6}, 0), "portalAffected[H][(1,6)] = true");
    CHECK(board.isPortalAffected(Pos{2, 6}, 0), "portalAffected[H][(2,6)] = true");
    CHECK(board.isPortalAffected(Pos{4, 6}, 0), "portalAffected[H][(4,6)] = true");
    CHECK(board.isPortalAffected(Pos{8, 3}, 0), "portalAffected[H][(8,3)] = true");
    CHECK(board.isPortalAffected(Pos{9, 3}, 0), "portalAffected[H][(9,3)] = true");

    // Check portalStep from (2,6) going right → should teleport at portal A
    Pos next_from_2_6 = board.portalStep(Pos{2, 6}, 0, +1);
    std::cout << "\n  portalStep((2,6), H, +1) = ("
              << next_from_2_6.x() << "," << next_from_2_6.y() << ")\n";
    CHECK(next_from_2_6.x() == 8 && next_from_2_6.y() == 3,
          "portalStep (2,6)→right = (8,3) [teleport through A→B]");

    // Check portalStep from (8,3) going left → should teleport at portal B
    Pos next_from_8_3 = board.portalStep(Pos{8, 3}, 0, -1);
    std::cout << "  portalStep((8,3), H, -1) = ("
              << next_from_8_3.x() << "," << next_from_8_3.y() << ")\n";
    CHECK(next_from_8_3.x() == 2 && next_from_8_3.y() == 6,
          "portalStep (8,3)→left = (2,6) [teleport through B→A]");

    // Direct key comparison: getKeyAt vs manual buildPortalKey
    // Place some stones first to have non-trivial patterns
    board.move<FREESTYLE>(Pos{1, 6});   // B at (1,6)
    board.move<FREESTYLE>(Pos{0, 0});   // W

    uint64_t key_0_6 = board.getKeyAt<FREESTYLE>(Pos{0, 6}, 0);
    std::cout << "\n  After placing B@(1,6):\n";
    std::cout << "    getKeyAt((0,6), H) = 0x" << std::hex << key_0_6 << std::dec << "\n";
    printCellInfo(board, Pos{0, 6});

    // Also print what the fast-path would give (for comparison)
    // Fast path: rotr(bitKey0[y], 2*(x-L))
    // We can't easily call this without the portal check, but we can check
    // if the cell's pattern makes sense for the portal case
}

// ============================================================================
// TEST 5: Horizontal win through portal (Case 1 from design)
//
// Portal: A=(3,6), B=(7,3)
// Stones: X at (0,6),(1,6),(2,6) — left of A
//         X at (8,3),(9,3) — right of B
// Virtual horizontal: (0,6)(1,6)(2,6)[A→B](8,3)(9,3) = 5 in a row
// Winning move: last stone placed
// ============================================================================
static void test5_horizontal_win()
{
    std::cout << "\n=== TEST 5: Horizontal Win Through Portal ===\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Virtual H line: X(0,6) X(1,6) X(2,6) [A=(3,6)→B=(7,3)] X(8,3) X(9,3)
    // Stone at (0,6) completes 5-in-a-row through portal — before placing check F5
    board.move<FREESTYLE>(Pos{8, 3});   // B
    board.move<FREESTYLE>(Pos{0, 0});   // W
    board.move<FREESTYLE>(Pos{9, 3});   // B
    board.move<FREESTYLE>(Pos{1, 0});   // W
    board.move<FREESTYLE>(Pos{1, 6});   // B
    board.move<FREESTYLE>(Pos{2, 0});   // W
    board.move<FREESTYLE>(Pos{2, 6});   // B
    board.move<FREESTYLE>(Pos{3, 0});   // W

    printBoard(board, 15);
    printCellInfo(board, Pos{0, 6});

    const Cell &c = board.cell(Pos{0, 6});
    CHECK(c.pattern2x[0].patBlack == F5, "H-dir pattern = F5 at (0,6)");
    CHECK(c.pattern4[BLACK] == A_FIVE, "p4[BLACK] = A_FIVE at (0,6)");
    CHECK(board.p4Count(BLACK, A_FIVE) > 0, "p4Count(BLACK, A_FIVE) > 0");
}

// ============================================================================
// TEST 6: Vertical win through portal (Case 2)
//
// Portal: A=(3,6), B=(7,3)
// Virtual V line at col=3: ★(3,3) X(3,4) X(3,5) [A→B+DOWN=(7,4)] X(7,4) X(7,5)
// We need to check if the engine sees this correctly
// ============================================================================
static void test6_vertical_win()
{
    std::cout << "\n=== TEST 6: Vertical Win Through Portal ===\n";
    std::cout << "  Portal: A=(3,6), B=(7,3)\n";
    std::cout << "  Virtual V line: ★(3,3) X(3,4) X(3,5) [A→exit(7,4)] X(7,4) X(7,5)\n\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Place stones: B at (3,4),(7,4),(3,5),(7,5) with W fillers
    board.move<FREESTYLE>(Pos{3, 4});   // B
    board.move<FREESTYLE>(Pos{0, 0});   // W
    board.move<FREESTYLE>(Pos{7, 4});   // B
    board.move<FREESTYLE>(Pos{1, 0});   // W
    board.move<FREESTYLE>(Pos{3, 5});   // B
    board.move<FREESTYLE>(Pos{2, 0});   // W
    board.move<FREESTYLE>(Pos{7, 5});   // B
    board.move<FREESTYLE>(Pos{4, 0});   // W

    std::cout << "  Before winning move (3,3):\n";
    printBoard(board, 15);
    printCellInfo(board, Pos{3, 3});

    const Cell &c = board.cell(Pos{3, 3});
    std::cout << "  Cell (3,3) V pattern for BLACK: " << patternName(c.pattern2x[1].patBlack) << "\n";
    std::cout << "  Cell (3,3) p4[BLACK]: " << pattern4Name(c.pattern4[BLACK]) << "\n";

    CHECK(c.pattern2x[1].patBlack == F5, "V-dir pattern = F5 at (3,3)");
    CHECK(c.pattern4[BLACK] == A_FIVE, "p4[BLACK] = A_FIVE at (3,3)");
}

// ============================================================================
// TEST 7: Diagonal \ win through portal (Case 3)
//
// Portal: A=(3,6), B=(7,3)
// Diagonal \ through A=(3,6): ...(1,4),(2,5),(3,6)=A...
// Exit: A→B + DOWN_RIGHT = (7,3)+(1,1) = (8,4)
// Virtual: ★(0,3) X(1,4) X(2,5) [A→exit(8,4)] X(8,4) X(9,5) = 5
// ============================================================================
static void test7_diagonal_win()
{
    std::cout << "\n=== TEST 7: Diagonal \\ Win Through Portal ===\n";
    std::cout << "  Portal: A=(3,6), B=(7,3)\n";
    std::cout << "  Virtual \\ line: ★(0,3) X(1,4) X(2,5) [A→exit(8,4)] X(8,4) X(9,5)\n\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Place stones
    board.move<FREESTYLE>(Pos{1, 4});   // B
    board.move<FREESTYLE>(Pos{0, 0});   // W
    board.move<FREESTYLE>(Pos{2, 5});   // B
    board.move<FREESTYLE>(Pos{1, 0});   // W
    board.move<FREESTYLE>(Pos{8, 4});   // B
    board.move<FREESTYLE>(Pos{2, 0});   // W
    board.move<FREESTYLE>(Pos{9, 5});   // B
    board.move<FREESTYLE>(Pos{4, 0});   // W

    std::cout << "  Before winning move (0,3):\n";
    printBoard(board, 15);
    printCellInfo(board, Pos{0, 3});

    const Cell &c = board.cell(Pos{0, 3});
    // dir 3 = DOWN_RIGHT = \  diagonal
    std::cout << "  Cell (0,3) \\ pattern for BLACK: " << patternName(c.pattern2x[3].patBlack) << "\n";
    std::cout << "  Cell (0,3) p4[BLACK]: " << pattern4Name(c.pattern4[BLACK]) << "\n";

    CHECK(c.pattern2x[3].patBlack == F5, "\\-dir pattern = F5 at (0,3)");
    CHECK(c.pattern4[BLACK] == A_FIVE, "p4[BLACK] = A_FIVE at (0,3)");
}

// ============================================================================
// TEST 8: Anti-diagonal / win through portal (Case 4)
//
// Portal: A=(3,6), B=(7,3)
// Anti-diagonal / through A: x+y=9 → ...(2,7),(3,6)=A,(4,5)...
// Exit: A→B + UP_RIGHT = (7,3)+(1,-1) = (8,2)
// Virtual: ★(0,9) X(1,8) X(2,7) [A→exit(8,2)] X(8,2) X(9,1) = 5
// ============================================================================
static void test8_antidiag_win()
{
    std::cout << "\n=== TEST 8: Anti-diagonal / Win Through Portal ===\n";
    std::cout << "  Portal: A=(3,6), B=(7,3)\n";
    std::cout << "  Virtual / line: ★(0,9) X(1,8) X(2,7) [A→exit(8,2)] X(8,2) X(9,1)\n\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    board.move<FREESTYLE>(Pos{1, 8});   // B
    board.move<FREESTYLE>(Pos{0, 0});   // W
    board.move<FREESTYLE>(Pos{2, 7});   // B
    board.move<FREESTYLE>(Pos{1, 0});   // W
    board.move<FREESTYLE>(Pos{8, 2});   // B
    board.move<FREESTYLE>(Pos{2, 0});   // W
    board.move<FREESTYLE>(Pos{9, 1});   // B
    board.move<FREESTYLE>(Pos{4, 0});   // W

    std::cout << "  Before winning move (0,9):\n";
    printBoard(board, 15);
    printCellInfo(board, Pos{0, 9});

    const Cell &c = board.cell(Pos{0, 9});
    // dir 2 = UP_RIGHT = / anti-diagonal
    std::cout << "  Cell (0,9) / pattern for BLACK: " << patternName(c.pattern2x[2].patBlack) << "\n";
    std::cout << "  Cell (0,9) p4[BLACK]: " << pattern4Name(c.pattern4[BLACK]) << "\n";

    CHECK(c.pattern2x[2].patBlack == F5, "/-dir pattern = F5 at (0,9)");
    CHECK(c.pattern4[BLACK] == A_FIVE, "p4[BLACK] = A_FIVE at (0,9)");
}

// ============================================================================
// TEST 9: Reverse direction — going through B to A
//
// Portal: A=(3,6), B=(7,3)
// Horizontal going LEFT through B:
// Virtual: X(0,6) X(1,6) X(2,6) [A←B] X(8,3) X(9,3) ★
// This is the same virtual line as test5 but from B's side
// Winning move: check from (9,3) side
// ============================================================================
static void test9_reverse_direction()
{
    std::cout << "\n=== TEST 9: Reverse Direction (B→A) ===\n";
    std::cout << "  Portal: A=(3,6), B=(7,3)\n";
    std::cout << "  Same virtual line as Test 5, checking from B side\n\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Place 4 black stones with white fillers
    board.move<FREESTYLE>(Pos{0, 6});   // B
    board.move<FREESTYLE>(Pos{0, 0});   // W
    board.move<FREESTYLE>(Pos{1, 6});   // B
    board.move<FREESTYLE>(Pos{1, 0});   // W
    board.move<FREESTYLE>(Pos{2, 6});   // B
    board.move<FREESTYLE>(Pos{2, 0});   // W
    board.move<FREESTYLE>(Pos{8, 3});   // B
    board.move<FREESTYLE>(Pos{3, 0});   // W

    std::cout << "  Before winning move (9,3):\n";
    printBoard(board, 15);
    printCellInfo(board, Pos{9, 3});

    const Cell &c = board.cell(Pos{9, 3});
    std::cout << "  Cell (9,3) H pattern for BLACK: " << patternName(c.pattern2x[0].patBlack) << "\n";
    std::cout << "  Cell (9,3) p4[BLACK]: " << pattern4Name(c.pattern4[BLACK]) << "\n";

    CHECK(c.pattern2x[0].patBlack == F5, "H-dir pattern = F5 at (9,3)");
    CHECK(c.pattern4[BLACK] == A_FIVE, "p4[BLACK] = A_FIVE at (9,3)");
}

// ============================================================================
// TEST 10: Gap in line — (1,6) is a WINNING move (B4 pattern)
//
// Portal: A=(3,6), B=(7,3)
// Stones: X at (0,6),(2,6) + X at (8,3),(9,3) — (1,6) is empty
// Virtual: X(0,6) .(1,6) X(2,6) [A→B] X(8,3) X(9,3)
// Placing at (1,6) completes 5 through portal → A_FIVE expected at (1,6)
//
// Also test a TRUE negative: with only 3 stones + gap, no cell can complete 5.
// ============================================================================
static void test10_gap_in_line()
{
    std::cout << "\n=== TEST 10: Gap in Line (Winning Move) ===\n";

    // --- Part A: Gap at (1,6) IS a winning move ---
    {
        Board board(15);
        board.addPortal(Pos{3, 6}, Pos{7, 3});
        board.newGame<FREESTYLE>();

        board.move<FREESTYLE>(Pos{0, 6});    // B
        board.move<FREESTYLE>(Pos{0, 0});    // W
        board.move<FREESTYLE>(Pos{2, 6});    // B
        board.move<FREESTYLE>(Pos{1, 0});    // W
        board.move<FREESTYLE>(Pos{8, 3});    // B
        board.move<FREESTYLE>(Pos{2, 0});    // W
        board.move<FREESTYLE>(Pos{9, 3});    // B
        board.move<FREESTYLE>(Pos{3, 0});    // W

        const Cell &c = board.cell(Pos{1, 6});
        std::cout << "  (1,6) p4=" << pattern4Name(c.pattern4[BLACK])
                  << " H=" << patternName(c.pattern2x[0].patBlack) << "\n";

        // Placing at (1,6) makes: X(0,6) X(1,6) X(2,6) [portal→] X(8,3) X(9,3) = 5
        CHECK(c.pattern2x[0].patBlack == F5, "(1,6) H=F5 (gap is winning move)");
        CHECK(c.pattern4[BLACK] == A_FIVE, "(1,6) p4=A_FIVE");
        CHECK(board.p4Count(BLACK, A_FIVE) > 0, "p4Count(A_FIVE) > 0 (gap is winning)");
    }

    // --- Part B: True negative — only 3 stones, no cell can complete 5 ---
    {
        Board board(15);
        board.addPortal(Pos{3, 6}, Pos{7, 3});
        board.newGame<FREESTYLE>();

        // Only 3 BLACK stones: (0,6), (2,6), (8,3) — NOT enough for 5
        board.move<FREESTYLE>(Pos{0, 6});    // B
        board.move<FREESTYLE>(Pos{0, 0});    // W
        board.move<FREESTYLE>(Pos{2, 6});    // B
        board.move<FREESTYLE>(Pos{1, 0});    // W
        board.move<FREESTYLE>(Pos{8, 3});    // B
        board.move<FREESTYLE>(Pos{2, 0});    // W

        CHECK(board.p4Count(BLACK, A_FIVE) == 0, "3 stones: no A_FIVE possible");
    }
}

// ============================================================================
// TEST 11: Move/Undo consistency — patterns unchanged after undo
// ============================================================================
static void test11_move_undo_consistency()
{
    std::cout << "\n=== TEST 11: Move/Undo Consistency ===\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Place 2 stones
    board.move<FREESTYLE>(Pos{2, 6});
    board.move<FREESTYLE>(Pos{8, 3});

    // Save state of some cells
    Pattern p1 = board.cell(Pos{1, 6}).pattern2x[0].patBlack;
    Pattern p2 = board.cell(Pos{9, 3}).pattern2x[0].patBlack;
    Pattern4 p4_1 = board.cell(Pos{1, 6}).pattern4[BLACK];

    std::cout << "  State after 2 moves:\n";
    printCellInfo(board, Pos{1, 6});
    printCellInfo(board, Pos{9, 3});

    // Place and undo a 3rd stone
    board.move<FREESTYLE>(Pos{1, 6});
    std::cout << "  After move(1,6):\n";
    printCellInfo(board, Pos{9, 3});

    board.undo<FREESTYLE>();
    std::cout << "  After undo:\n";
    printCellInfo(board, Pos{1, 6});
    printCellInfo(board, Pos{9, 3});

    Pattern p1_after = board.cell(Pos{1, 6}).pattern2x[0].patBlack;
    Pattern p2_after = board.cell(Pos{9, 3}).pattern2x[0].patBlack;
    Pattern4 p4_1_after = board.cell(Pos{1, 6}).pattern4[BLACK];

    CHECK(p1 == p1_after, "pattern at (1,6) restored after undo");
    CHECK(p2 == p2_after, "pattern at (9,3) restored after undo");
    CHECK(p4_1 == p4_1_after, "pattern4 at (1,6) restored after undo");
}

// ============================================================================
// TEST 12: Collinear portal — gap cells
// Portal pair on same row: A=(3,5), B=(8,5)
// Gap cells: (4,5),(5,5),(6,5),(7,5) in H direction
// These gap cells should see A and B as WALL (not teleport) in H direction
// ============================================================================
static void test12_collinear_gap()
{
    std::cout << "\n=== TEST 12: Collinear Portal — Gap Cells ===\n";
    std::cout << "  Portal: A=(3,5), B=(8,5) (same row = H-collinear)\n";
    std::cout << "  Gap cells (4-7, 5) in H dir should see WALL, not teleport\n\n";

    Board board(15);
    board.addPortal(Pos{3, 5}, Pos{8, 5});
    board.newGame<FREESTYLE>();

    printBoard(board, 15);

    // Place X at (5,5) (gap cell) — its H pattern should be DEAD (blocked by walls on both sides)
    board.move<FREESTYLE>(Pos{5, 5});   // B
    board.move<FREESTYLE>(Pos{0, 0});   // W

    std::cout << "  Gap cell pattern info (before placing at gap):\n";
    printCellInfo(board, Pos{4, 5});
    printCellInfo(board, Pos{6, 5});

    // (4,5) in H-dir: left side is blocked by A=(3,5)=WALL, limited window
    // (6,5) in H-dir: right side is blocked by B=(8,5)=WALL, limited window
    // These cells should NOT see through the portal in H direction

    std::cout << "  [collinear gap test completed — verify patterns visually]\n";
}

// ============================================================================
// TEST 13: Incremental update — place stone near portal, check remote update
//
// Portal: A=(3,6), B=(7,3)
// Place X at (2,6) (near A in H dir)
// Cell (8,3) (near B in H dir) should see the stone through the portal
// ============================================================================
static void test13_incremental_remote_update()
{
    std::cout << "\n=== TEST 13: Incremental Remote Update ===\n";
    std::cout << "  Portal: A=(3,6), B=(7,3)\n";
    std::cout << "  Place X at (2,6) near A → check (8,3) near B sees it\n\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Before any move, (8,3) H pattern should be all empty
    const Cell &before = board.cell(Pos{8, 3});
    std::cout << "  Before move - (8,3) H pat[B]: " << patternName(before.pattern2x[0].patBlack) << "\n";

    board.move<FREESTYLE>(Pos{2, 6});   // B near portal A

    const Cell &after = board.cell(Pos{8, 3});
    std::cout << "  After B@(2,6) - (8,3) H pat[B]: " << patternName(after.pattern2x[0].patBlack) << "\n";
    std::cout << "  After B@(2,6) - (8,3) p4[B]: " << pattern4Name(after.pattern4[BLACK]) << "\n";

    // (8,3) should see the black stone at (2,6) through portal → not DEAD, some pattern > DEAD
    // The virtual H line from (8,3) going LEFT: (8,3)→B=(7,3)→teleport→A→(2,6)=X
    // So (8,3) sees a friendly stone 1 virtual step away → should be at least B1 or better

    CHECK(after.pattern2x[0].patBlack > DEAD, "(8,3) H pattern sees stone through portal");
}

// ============================================================================
// TEST 14: Multiple move/undo cycles — stress test incremental update
// ============================================================================
static void test14_stress_move_undo()
{
    std::cout << "\n=== TEST 14: Stress Test — Multiple Move/Undo Cycles ===\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.newGame<FREESTYLE>();

    // Do 10 move/undo cycles
    Pos moves[] = {
        {2, 6}, {8, 3}, {1, 6}, {9, 3}, {0, 6},
        {7, 4}, {3, 5}, {8, 2}, {9, 1}, {7, 5}
    };

    for (int cycle = 0; cycle < 3; cycle++) {
        std::cout << "  Cycle " << cycle << ": placing " << 10 << " stones...\n";
        for (int i = 0; i < 10; i++) {
            board.move<FREESTYLE>(moves[i]);
        }
        std::cout << "    ply=" << board.ply() << "\n";

        // Undo all
        for (int i = 9; i >= 0; i--) {
            board.undo<FREESTYLE>();
        }
        std::cout << "    after undo: ply=" << board.ply() << "\n";
        CHECK(board.ply() == 0, "ply back to 0 after full undo");
    }

    // Verify board is back to initial state
    CHECK(board.isEmpty(Pos{2, 6}), "cell back to empty after stress test");
}

// ============================================================================
// TEST 15: Two portal pairs
// ============================================================================
static void test15_two_portals()
{
    std::cout << "\n=== TEST 15: Two Portal Pairs ===\n";

    Board board(15);
    board.addPortal(Pos{3, 6}, Pos{7, 3});  // pair 0
    board.addPortal(Pos{5, 1}, Pos{10, 10}); // pair 1
    board.newGame<FREESTYLE>();

    CHECK(board.portalCount() == 2, "2 portal pairs");
    CHECK(board.isPortalCell(Pos{3, 6}), "pair 0 A");
    CHECK(board.isPortalCell(Pos{7, 3}), "pair 0 B");
    CHECK(board.isPortalCell(Pos{5, 1}), "pair 1 A");
    CHECK(board.isPortalCell(Pos{10, 10}), "pair 1 B");
    CHECK(!board.isEmpty(Pos{5, 1}), "portal cell = WALL");

    printBoard(board, 15);
    std::cout << "  [two portal pairs set up OK]\n";
}

// ============================================================================
// TEST 16: Full board trace() — visual verification
// ============================================================================
static void test16_trace()
{
    std::cout << "\n=== TEST 16: Board Trace (visual) ===\n";

    Board board(10);
    board.addPortal(Pos{3, 6}, Pos{7, 3});
    board.addWall(Pos{5, 5});
    board.newGame<FREESTYLE>();

    board.move<FREESTYLE>(Pos{2, 6});
    board.move<FREESTYLE>(Pos{8, 3});
    board.move<FREESTYLE>(Pos{1, 6});
    board.move<FREESTYLE>(Pos{9, 3});

    // Print first part of trace (board section only)
    std::string traceStr = board.trace();
    // Print just the first ~40 lines
    std::istringstream iss(traceStr);
    std::string line;
    int lineCount = 0;
    while (std::getline(iss, line) && lineCount < 30) {
        std::cout << "  " << line << "\n";
        lineCount++;
    }
    if (lineCount >= 30)
        std::cout << "  ... (truncated)\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    std::cout << "================================================================\n";
    std::cout << "  Portal Gomoku Engine — Board Logic Test Suite\n";
    std::cout << "================================================================\n";

    // Initialize config (needed for P4SCORES, EVALS, PCODE tables)
    // Load internal config which contains the classical eval model
    if (!Config::InternalConfig.empty()) {
        std::istringstream iss(Config::InternalConfig);
        if (!Config::loadConfig(iss)) {
            std::cerr << "ERROR: Failed to load internal config!\n";
            return 1;
        }
        std::cout << "Config loaded from internal config.\n";
    }
    else {
        std::cerr << "ERROR: No internal config available!\n";
        return 1;
    }

    test1_basic_board();
    test2_wall();
    test3_portal_setup();
    test4_portal_affected();
    test5_horizontal_win();
    test6_vertical_win();
    test7_diagonal_win();
    test8_antidiag_win();
    test9_reverse_direction();
    test10_gap_in_line();
    test11_move_undo_consistency();
    test12_collinear_gap();
    test13_incremental_remote_update();
    test14_stress_move_undo();
    test15_two_portals();
    test16_trace();

    std::cout << "\n================================================================\n";
    std::cout << "  RESULTS: " << testsPassed << " passed, " << testsFailed << " failed\n";
    std::cout << "================================================================\n";

    return testsFailed > 0 ? 1 : 0;
}
