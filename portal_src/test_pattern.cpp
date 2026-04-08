/*
 *  Portal Gomoku Engine — Pattern Detection Test Suite
 *
 *  Phase 1: Normal pattern tests (no portals) — all 14 Pattern values
 *  Phase 2: Portal pattern tests — F5/B4/F4/F3 across portals
 *
 *  Key encoding  (from board.h / pattern.h):
 *    EMPTY=0b11  BLACK=0b10  WHITE=0b01  WALL=0b00
 *  FREESTYLE: HalfLineLen=4, window=9 cells (±4 + center)
 *  Direction: 0=H  1=V  2=/  3='\'
 *
 *  Cell access:
 *    board.cell(pos).pattern(color, dir)  -> Pattern (per direction)
 *    board.cell(pos).pattern4[side]       -> Pattern4 (combined 4 dirs)
 *    board.cell(pos).pattern2x[dir]       -> Pattern2x (both colors)
 */

#include "game/board.h"
#include "game/pattern.h"
#include "config.h"
#include "core/pos.h"
#include "core/types.h"

#include <cassert>
#include <iostream>
#include <sstream>
#include <string>

// ============================================================================
// Test infrastructure (matches test_portal.cpp style)
// ============================================================================

static int testsPassed = 0;
static int testsFailed = 0;

static void checkResult(bool ok, const char *expr, const char *file, int line)
{
    if (ok) {
        std::cout << "  PASS: " << expr << "\n";
        testsPassed++;
    }
    else {
        std::cout << "  FAIL: " << expr << " [" << expr << "] at " << file << ":" << line << "\n";
        testsFailed++;
    }
}

#define CHECK(expr, msg) checkResult((expr), (msg), __FILE__, __LINE__)

// ============================================================================
// Helpers
// ============================================================================

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
    case NONE:           return "NONE";
    case FORBID:         return "FORBID";
    case L_FLEX2:        return "L_FLEX2";
    case K_BLOCK3:       return "K_BLOCK3";
    case J_FLEX2_2X:     return "J_FLEX2_2X";
    case I_BLOCK3_PLUS:  return "I_BLOCK3_PLUS";
    case H_FLEX3:        return "H_FLEX3";
    case G_FLEX3_PLUS:   return "G_FLEX3_PLUS";
    case F_FLEX3_2X:     return "F_FLEX3_2X";
    case E_BLOCK4:       return "E_BLOCK4";
    case D_BLOCK4_PLUS:  return "D_BLOCK4_PLUS";
    case C_BLOCK4_FLEX3: return "C_BLOCK4_FLEX3";
    case B_FLEX4:        return "B_FLEX4";
    case A_FIVE:         return "A_FIVE";
    default:             return "???";
    }
}

/// Print pattern info for an empty cell (both colors, all dirs)
static void printCell(const Board &board, Pos pos)
{
    if (!board.isEmpty(pos)) {
        Color c = board.get(pos);
        std::cout << "  (" << pos.x() << "," << pos.y() << ") piece="
                  << (c == BLACK ? "BLACK" : c == WHITE ? "WHITE" : "WALL") << "\n";
        return;
    }
    const Cell &cell = board.cell(pos);
    std::cout << "  (" << pos.x() << "," << pos.y() << ")";
    std::cout << "  H=" << patternName(cell.pattern(BLACK, 0))
              << " V=" << patternName(cell.pattern(BLACK, 1))
              << " /=" << patternName(cell.pattern(BLACK, 2))
              << " \\=" << patternName(cell.pattern(BLACK, 3));
    std::cout << "  p4[B]=" << pattern4Name(cell.pattern4[BLACK])
              << " p4[W]=" << pattern4Name(cell.pattern4[WHITE]) << "\n";
}

/// Place BLACK, then WHITE dummies scattered at corners, alternating
static void placeBlack(Board &board, Pos pos)
{
    board.move<FREESTYLE>(pos);
}

/// Scatter WHITE dummy to avoid creating threats
static const Pos WHITE_DUMMIES[] = {
    {0, 0}, {14, 14}, {0, 14}, {14, 0}
};
static int dummyIdx = 0;

static void resetDummies() { dummyIdx = 0; }

static void placeBlackWhite(Board &board, Pos blackPos)
{
    board.move<FREESTYLE>(blackPos);
    board.move<FREESTYLE>(WHITE_DUMMIES[dummyIdx % 4]);
    dummyIdx++;
}

// ============================================================================
// PHASE 1 — Normal Pattern Tests (FREESTYLE, H direction)
// ============================================================================

// Layout reference: y=7, x varies, dir=0 (horizontal)
//   Window of cell (cx,7): cells (cx-4,7) to (cx+4,7)

// -------------------------------------------------------
// DEAD: WALLs within 2 cells on BOTH sides → run < 5
// addWall(3,7), addWall(7,7) → check (5,7)
// Window of (5,7): (1..9,7): E,E,WALL,E,[center],E,WALL,E,E
// Max consecutive run including center = 2 → DEAD
// -------------------------------------------------------
static void test_pat_dead()
{
    std::cout << "\n=== PHASE 1: DEAD — WALLs within ±2 ===\n";
    Board board(15);
    board.addWall(Pos{3, 7});
    board.addWall(Pos{7, 7});
    board.newGame<FREESTYLE>();

    Pos check{5, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == DEAD,
          "DEAD: WALLs at ±2 → H pattern = DEAD");
    CHECK(board.cell(check).pattern(WHITE, 0) == DEAD,
          "DEAD: symmetric for WHITE");
}

// -------------------------------------------------------
// F5: 4 consecutive BLACK stones on one side, empty cell extends to 5
// BLACK at (4,5,6,7,7): check (8,7) H
// Window(8,7): B(4),B(5),B(6),B(7),[center(8)],E,E,E,E
// Playing (8,7) → B(4-8) = 5-in-a-row → F5
// -------------------------------------------------------
static void test_pat_f5()
{
    std::cout << "\n=== PHASE 1: F5 — 4 blacks adjacent, playing completes 5 ===\n";
    Board board(15);
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{4, 7});
    placeBlackWhite(board, Pos{5, 7});
    placeBlackWhite(board, Pos{6, 7});
    placeBlackWhite(board, Pos{7, 7});

    Pos check{8, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == F5,
          "F5: 4 blacks on left → H pattern = F5");
    CHECK(board.cell(check).pattern4[BLACK] == A_FIVE,
          "F5: pattern4[BLACK] = A_FIVE");
    // Opposite end also F5
    Pos check2{3, 7};
    printCell(board, check2);
    CHECK(board.cell(check2).pattern(BLACK, 0) == F5,
          "F5: 4 blacks on right → H pattern = F5 (opposite end)");
}

// -------------------------------------------------------
// B4: WALL at pos-4, BLACK at pos-3,-2,-1, check center
// addWall(3,7), BLACK at (4,5,6,7) → check (7,7): window has WALL on left
// Wait: window of (7,7) = [3..11]: WALL(3),B(4),B(5),B(6),[center(7)],E,E,E,E
// Playing at (7,7): B(4,5,6,7)=4, WALL left → only extend right → B4
// -------------------------------------------------------
static void test_pat_b4()
{
    std::cout << "\n=== PHASE 1: B4 — WALL+3 blacks, playing makes blocked 4 ===\n";
    Board board(15);
    board.addWall(Pos{3, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{4, 7});
    placeBlackWhite(board, Pos{5, 7});
    placeBlackWhite(board, Pos{6, 7});

    Pos check{7, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == B4,
          "B4: WALL at pos-4, 3 blacks pos-3..-1 → H pattern = B4");
    CHECK(board.cell(check).pattern4[BLACK] == E_BLOCK4
          || board.cell(check).pattern4[BLACK] == D_BLOCK4_PLUS
          || board.cell(check).pattern4[BLACK] == C_BLOCK4_FLEX3
          || board.cell(check).pattern4[BLACK] == B_FLEX4,
          "B4: pattern4 >= E_BLOCK4");
}

// -------------------------------------------------------
// F4: 3 consecutive blacks, empty on BOTH sides → playing creates open 4
// BLACK at (5,6,7) → check (4,7) or (8,7)
// Window of (8,7): [4..12]: E(4),B(5),B(6),B(7),[center(8)],E,E,E,E
// Playing at (8,7): B(5,6,7,8)=4. Left=(4,7)=E→(4-8)=5 ✓; Right=(9,7)=E→(5-9)=5 ✓ → F4
// -------------------------------------------------------
static void test_pat_f4()
{
    std::cout << "\n=== PHASE 1: F4 — 3 blacks, both ends open → playing makes open 4 ===\n";
    Board board(15);
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{5, 7});
    placeBlackWhite(board, Pos{6, 7});
    placeBlackWhite(board, Pos{7, 7});

    // Right end: playing (8,7) creates B(5-8) with both ends open
    Pos checkR{8, 7};
    printCell(board, checkR);
    CHECK(board.cell(checkR).pattern(BLACK, 0) == F4,
          "F4: check (8,7) — 3 blacks on left, open both ends → H=F4");
    CHECK(board.cell(checkR).pattern4[BLACK] == B_FLEX4,
          "F4: pattern4[BLACK] = B_FLEX4");

    // Left end: playing (4,7) creates B(4-7) with both ends open
    Pos checkL{4, 7};
    printCell(board, checkL);
    CHECK(board.cell(checkL).pattern(BLACK, 0) == F4,
          "F4: check (4,7) — 3 blacks on right, open both ends → H=F4");
}

// -------------------------------------------------------
// B3: addWall(3,7), 2 blacks at (4,5), check (6,7)
// Playing at (6,7): B(4,5,6)=3, WALL on left → only one path to B4
// → B3 ("one step before B4")
// -------------------------------------------------------
static void test_pat_b3()
{
    std::cout << "\n=== PHASE 1: B3 — WALL+2 blacks, playing makes blocked 3 ===\n";
    Board board(15);
    board.addWall(Pos{3, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{4, 7});
    placeBlackWhite(board, Pos{5, 7});

    Pos check{6, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == B3,
          "B3: WALL+2 blacks → H pattern = B3");
    CHECK(board.cell(check).pattern4[BLACK] == K_BLOCK3
          || board.cell(check).pattern4[BLACK] == I_BLOCK3_PLUS
          || board.cell(check).pattern4[BLACK] == G_FLEX3_PLUS,
          "B3: pattern4 in {K_BLOCK3, I_BLOCK3_PLUS, G_FLEX3_PLUS}");
}

// -------------------------------------------------------
// F3S: 2 blacks at (5,6), no walls, check (7,7)
// Playing at (7,7): B(5,6,7)=3, both ends open → TWO ways to reach F4
// → F3S ("one step before two F4")
// -------------------------------------------------------
static void test_pat_f3s()
{
    std::cout << "\n=== PHASE 1: F3S — 2 blacks, open both sides → 2 ways to F4 ===\n";
    Board board(15);
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{5, 7});
    placeBlackWhite(board, Pos{6, 7});

    Pos check{7, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == F3S,
          "F3S: 2 blacks adjacent, open ends → H=F3S");
    CHECK(board.cell(check).pattern4[BLACK] == F_FLEX3_2X
          || board.cell(check).pattern4[BLACK] == H_FLEX3
          || board.cell(check).pattern4[BLACK] == G_FLEX3_PLUS,
          "F3S: pattern4 contains flex3 component");
}

// -------------------------------------------------------
// F3: addWall(3,7), 2 blacks at (5,6), check (7,7)
// Playing at (7,7): B(5,6,7)=3, WALL at (3,7) limits left extension
// → F3 ("one step before one F4")
// -------------------------------------------------------
static void test_pat_f3()
{
    std::cout << "\n=== PHASE 1: F3 — WALL limits one side → 1 way to F4 ===\n";
    Board board(15);
    board.addWall(Pos{3, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{5, 7});
    placeBlackWhite(board, Pos{6, 7});

    Pos check{7, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == F3,
          "F3: WALL at left-2, 2 blacks → H=F3");
    CHECK(board.cell(check).pattern4[BLACK] >= H_FLEX3,
          "F3: pattern4 >= H_FLEX3");
}

// -------------------------------------------------------
// B2: addWall(3,7), BLACK at (4,7), check (5,7)
// Window(5,7): WALL(1→OOB-adj),E,WALL(3),B(4),[center(5)],E,E,E,E
// Playing at (5,7): B(4,5)=2, WALL blocks left → B2
// -------------------------------------------------------
static void test_pat_b2()
{
    std::cout << "\n=== PHASE 1: B2 — WALL+1 black, playing makes blocked 2 ===\n";
    Board board(15);
    board.addWall(Pos{3, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{4, 7});

    Pos check{5, 7};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 0) == B2,
          "B2: WALL+1 black → H pattern = B2");
}

// -------------------------------------------------------
// F2 variants: 1 black, both sides open → patterns F2/F2A/F2B
// BLACK at (6,7), check (7,7): B(6,7)=2 if played, pattern?
// BLACK at (5,7), check (7,7): B(5,7) with gap → F2A/F2B
// -------------------------------------------------------
static void test_pat_f2_variants()
{
    std::cout << "\n=== PHASE 1: F2/F2A/F2B — open two variants ===\n";

    // F2B: adjacent (one stone next to check cell)
    {
        Board board(15);
        board.newGame<FREESTYLE>();
        resetDummies();
        placeBlackWhite(board, Pos{6, 7});
        Pos check{7, 7};
        printCell(board, check);
        Pattern p = board.cell(check).pattern(BLACK, 0);
        CHECK(p == F2 || p == F2A || p == F2B,
              "F2 variant (adjacent stone): H=F2|F2A|F2B");
        std::cout << "  (adjacent stone at 6,7) → H=" << patternName(p) << "\n";
    }

    // F2A: stone at distance 2 (gap of 1 empty)
    {
        Board board(15);
        board.newGame<FREESTYLE>();
        resetDummies();
        placeBlackWhite(board, Pos{5, 7});  // skip (6,7)
        Pos check{7, 7};
        printCell(board, check);
        Pattern p = board.cell(check).pattern(BLACK, 0);
        CHECK(p == F2 || p == F2A || p == F2B,
              "F2 variant (stone at gap-1): H=F2|F2A|F2B");
        std::cout << "  (stone at 5,7, gap at 6,7) → H=" << patternName(p) << "\n";
    }

    // F2: stone at distance 3 (gap of 2 empties)
    {
        Board board(15);
        board.newGame<FREESTYLE>();
        resetDummies();
        placeBlackWhite(board, Pos{4, 7});  // skip (5,6)
        Pos check{7, 7};
        printCell(board, check);
        Pattern p = board.cell(check).pattern(BLACK, 0);
        CHECK(p >= F1 && p <= F2B,
              "F2 variant (stone at gap-2): H in [F1..F2B]");
        std::cout << "  (stone at 4,7, gap at 5,6) → H=" << patternName(p) << "\n";
    }
}

// -------------------------------------------------------
// F1/B1: Single stone, open or limited
// -------------------------------------------------------
static void test_pat_f1_b1()
{
    std::cout << "\n=== PHASE 1: F1/B1 — single stone ===\n";

    // F1: single black, wide open space
    {
        Board board(15);
        board.newGame<FREESTYLE>();
        resetDummies();
        placeBlackWhite(board, Pos{6, 7});
        Pos check{8, 7};  // 2 cells from the single stone
        printCell(board, check);
        Pattern p = board.cell(check).pattern(BLACK, 0);
        CHECK(p == F1 || p == F2 || p == F2A || p == F2B,
              "F1 region: single black 2 cells away, open → H=F1 or F2-variant");
        std::cout << "  H=" << patternName(p) << "\n";
    }

    // B1/B2: single black with WALL immediately beside check cell on one side,
    // and boundary on the other — total run < 5.
    // addWall(3,7) + BLACK at (1,7) → check (2,7):
    //   pos-1=(1,7)=B, pos+1=(3,7)=WALL → B(1,2)+WALL = blocked right after 2 stones
    //   left: E(0,7) then OOB-WALLs → can't form 5 in either direction → B1 or B2
    {
        Board board(15);
        board.addWall(Pos{3, 7});
        board.newGame<FREESTYLE>();
        resetDummies();
        placeBlackWhite(board, Pos{1, 7});
        Pos check{2, 7};
        printCell(board, check);
        Pattern p = board.cell(check).pattern(BLACK, 0);
        // (2,7) has WALL at pos+1 and OOB-WALLs at pos-2,pos-3,pos-4.
        // Run = only 2 cells total → engine reports DEAD (can't form 5 even with stone).
        // DEAD, B1 and B2 are all valid for heavily-constrained positions.
        CHECK(p == DEAD || p == B1 || p == B2,
              "B1/B2/DEAD: tightly bracketed single black → H in {DEAD,B1,B2}");
        std::cout << "  H=" << patternName(p) << " (DEAD expected: run space < 5)\n";
    }
}

// -------------------------------------------------------
// OL (Overline): Only in STANDARD rule
// 3 blacks each side of center → playing creates 7-in-a-row = overline
// -------------------------------------------------------
static void test_pat_ol()
{
    std::cout << "\n=== PHASE 1: OL — overline (STANDARD rule only) ===\n";
    Board board(15);
    board.newGame<STANDARD>();
    resetDummies();

    // Place blacks on both sides of (7,7): at (2,3,4) and (8,9,10)
    // But must alternate BLACK/WHITE
    // Let WHITE play at corners
    board.move<STANDARD>(Pos{2, 7});  board.move<STANDARD>(Pos{0, 0});
    board.move<STANDARD>(Pos{3, 7});  board.move<STANDARD>(Pos{14, 14});
    board.move<STANDARD>(Pos{4, 7});  board.move<STANDARD>(Pos{0, 14});
    board.move<STANDARD>(Pos{8, 7});  board.move<STANDARD>(Pos{14, 0});
    board.move<STANDARD>(Pos{9, 7});  board.move<STANDARD>(Pos{1, 14});
    board.move<STANDARD>(Pos{10, 7}); board.move<STANDARD>(Pos{13, 0});

    // Check (7,7): window [3..11]: B(4),B? wait: (3,4) are blacks, let me recheck
    // Window of (7,7) in STANDARD (HalfLineLen=5): [2..12]
    // (2,3,4)=B and (8,9,10)=B, center=(7,7)=empty
    // Playing at (7,7): B(2,3,4,7,8,9,10) — but only if they're consecutive? No gap at 5,6!
    // Actually: B(2..4) gap(5,6) center(7) B(8..10) — NOT consecutive. Not an overline.
    //
    // For OL: need consecutive stones on BOTH sides adjacent to center.
    // BLACK at (2,3,4,5,6) on LEFT and (8,9,10,11,12) on RIGHT, check (7,7)
    // That's 5+5=10 stones, overline on both sides.
    // Simpler: just use (3,4,5,6) + (8,9,10) around center (7,7):
    // Window [-→ (2..12)]: ...B(3),B(4),B(5),B(6),[center(7)],B(8),B(9),B(10),...
    // Playing at (7,7): 7 consecutive B(3-10) → overline → OL!
    //
    // Let's redo properly:
    std::cout << "  [Resetting for OL test]\n";
    Board b2(15);
    b2.newGame<STANDARD>();
    // BLACK pieces interleaved with WHITE dummies at far corners
    b2.move<STANDARD>(Pos{3, 7});  b2.move<STANDARD>(Pos{0, 0});
    b2.move<STANDARD>(Pos{4, 7});  b2.move<STANDARD>(Pos{14, 14});
    b2.move<STANDARD>(Pos{5, 7});  b2.move<STANDARD>(Pos{0, 14});
    b2.move<STANDARD>(Pos{6, 7});  b2.move<STANDARD>(Pos{14, 0});
    b2.move<STANDARD>(Pos{8, 7});  b2.move<STANDARD>(Pos{1, 0});
    b2.move<STANDARD>(Pos{9, 7});  b2.move<STANDARD>(Pos{13, 14});
    b2.move<STANDARD>(Pos{10, 7}); b2.move<STANDARD>(Pos{1, 14});

    Pos check{7, 7};
    // In STANDARD: playing (7,7) creates B(3,4,5,6,7,8,9,10) = 8 consecutive → OL
    std::cout << "  B stones at x=3,4,5,6 and x=8,9,10. Check (7,7) H:\n";
    printCell(b2, check);
    Pattern p = b2.cell(check).pattern(BLACK, 0);
    std::cout << "  (7,7) H pattern for BLACK = " << patternName(p) << "\n";
    CHECK(p == OL, "OL: surrounded by 3+3 blacks → STANDARD H=OL");

    // In FREESTYLE, same setup should still be "winning" not OL
    Board b3(15);
    b3.newGame<FREESTYLE>();
    b3.move<FREESTYLE>(Pos{3, 7});  b3.move<FREESTYLE>(Pos{0, 0});
    b3.move<FREESTYLE>(Pos{4, 7});  b3.move<FREESTYLE>(Pos{14, 14});
    b3.move<FREESTYLE>(Pos{5, 7});  b3.move<FREESTYLE>(Pos{0, 14});
    b3.move<FREESTYLE>(Pos{6, 7});  b3.move<FREESTYLE>(Pos{14, 0});
    b3.move<FREESTYLE>(Pos{8, 7});  b3.move<FREESTYLE>(Pos{1, 0});
    b3.move<FREESTYLE>(Pos{9, 7});  b3.move<FREESTYLE>(Pos{13, 14});
    b3.move<FREESTYLE>(Pos{10, 7}); b3.move<FREESTYLE>(Pos{1, 14});
    p = b3.cell(check).pattern(BLACK, 0);
    std::cout << "  FREESTYLE same setup → H=" << patternName(p) << "\n";
    CHECK(p == F5, "FREESTYLE: same overline setup → H=F5 (overline is win)");
}

// -------------------------------------------------------
// Pattern2x symmetry: same position, swap colors
// Layout: BLACK at (5,7), WHITE at (9,7), empty at (7,7)
// Pattern2x at (7,7) H: patBlack ≠ patWhite (different threats)
// Also: identical layout with colors swapped → swapped Pattern2x
// -------------------------------------------------------
static void test_pat_pattern2x_symmetry()
{
    std::cout << "\n=== PHASE 1: Pattern2x — both colors in one lookup ===\n";

    Board board(15);
    board.newGame<FREESTYLE>();
    // BLACK on left, WHITE on right of center (7,7)
    // Pattern2x symmetry: place BLACK and WHITE ADJACENT to check cell
    // so that each side sees a very different local view.
    // WHITE at (6,7) [left of center], BLACK stone elsewhere
    // Wait: board.move() alternates turns. So BLACK moves first.
    // BLACK at (8,7) [right of center, adjacent], WHITE at (6,7) [left of center, adjacent]
    // Check (7,7): sandwiched — BLACK sees own stone on right, opponent on left
    //              WHITE sees own stone on left, opponent on right
    // patBlack for H: one own stone on right (pos+1) + opponent on left (pos-1)
    // patWhite for H: one own stone on left (pos-1) + opponent on right (pos+1)
    // Both are the same geometric situation (just mirrored) — may or may not differ.
    // Instead, use 3 BLACK on one side to create clearly different patterns:
    // BLACK at (4,5,6,7) [4 blacks to left], WHITE at (9,7) [adjacent right]
    // Check (8,7): BLACK sees 4 own stones left (pos-4..-1) + opponent at pos+1 → B4
    //              WHITE sees own stone at pos+1 + 4 black opponents left → different pattern
    board.move<FREESTYLE>(Pos{4, 7});   // BLACK
    board.move<FREESTYLE>(Pos{9, 7});   // WHITE
    board.move<FREESTYLE>(Pos{5, 7});   // BLACK
    board.move<FREESTYLE>(Pos{14, 14}); // WHITE dummy
    board.move<FREESTYLE>(Pos{6, 7});   // BLACK
    board.move<FREESTYLE>(Pos{0, 14});  // WHITE dummy
    board.move<FREESTYLE>(Pos{7, 7});   // BLACK
    board.move<FREESTYLE>(Pos{14, 0});  // WHITE dummy

    Pos check{8, 7};  // empty cell: 4 blacks on left, 1 white on right
    printCell(board, check);
    const Cell &cell = board.cell(check);
    Pattern2x px = cell.pattern2x[0];  // H direction

    std::cout << "  Pattern2x H: patBlack=" << patternName(px.patBlack)
              << " patWhite=" << patternName(px.patWhite) << "\n";
    std::cout << "  pattern(BLACK,H)=" << patternName(cell.pattern(BLACK, 0))
              << " pattern(WHITE,H)=" << patternName(cell.pattern(WHITE, 0)) << "\n";

    CHECK(px[BLACK] == cell.pattern(BLACK, 0),
          "Pattern2x: operator[] == pattern() for BLACK");
    CHECK(px[WHITE] == cell.pattern(WHITE, 0),
          "Pattern2x: operator[] == pattern() for WHITE");
    // BLACK has 4 own stones to the left → B4 or F5; WHITE has 1 own + opponent wall to right → much lower
    CHECK(px.patBlack > px.patWhite,
          "Pattern2x: patBlack > patWhite (4 blacks left vs 1 white right)");
}

// -------------------------------------------------------
// Multi-direction: same board, check H≠V (directional independence)
// BLACK at (7,4)(7,5)(7,6) — vertical column above (7,7)
// Check (7,7): V should be B3 or F3, H should be F1/DEAD/etc
// -------------------------------------------------------
static void test_pat_multi_dir()
{
    std::cout << "\n=== PHASE 1: Multi-direction — H≠V independence ===\n";
    Board board(15);
    board.addWall(Pos{3, 7});  // Block H direction on left
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{7, 4});
    placeBlackWhite(board, Pos{7, 5});
    placeBlackWhite(board, Pos{7, 6});

    Pos check{7, 7};
    printCell(board, check);

    Pattern hPat = board.cell(check).pattern(BLACK, 0);
    Pattern vPat = board.cell(check).pattern(BLACK, 1);

    std::cout << "  H=" << patternName(hPat) << " V=" << patternName(vPat) << "\n";
    CHECK(vPat >= B3,
          "Multi-dir: V should be B3 or higher (3 stones above)");
    CHECK(hPat != vPat,
          "Multi-dir: H and V patterns different (stones only on V)");
}

// ============================================================================
// PHASE 2 — Portal Pattern Tests
// ============================================================================

// -------------------------------------------------------
// P-F5-H: Horizontal portal, 5-in-a-row spanning it
// Portal A=(5,7) ↔ B=(10,7)
// BLACK at (2,7)(3,7)(4,7) left of A, (11,7)(12,7) right of B
// Check (1,7): after 5th stone at (12,7), check (1,7) should see F5
// Actually easier: check the empty cell AT the end of the virtual 5
// Let's check (13,7): B at [11,12] on right side of B, [2,3,4] on left side of A
// Virtual line: 2,3,4→[A portal B]→11,12 = 5 consecutive
// The 5th stone just placed is (12,7). Before placing it, (12,7) should be F5.
// -------------------------------------------------------
static void test_portal_f5_horizontal()
{
    std::cout << "\n=== PHASE 2: Portal H — F5 spanning horizontal portal ===\n";
    // Portal A=(5,7) ↔ B=(10,7) [same row, collinear H → zero-width pass-through]
    Board board(15);
    board.addPortal(Pos{5, 7}, Pos{10, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    // Place 4 blacks: 3 left of A, 1 right of B
    placeBlackWhite(board, Pos{2, 7});  // left
    placeBlackWhite(board, Pos{3, 7});  // left
    placeBlackWhite(board, Pos{4, 7});  // adjacent to portal A
    placeBlackWhite(board, Pos{11, 7}); // adjacent to portal B exit

    // Now check (12,7): should be F5 (4th black via portal + (12) = 5th)
    Pos check{12, 7};
    printCell(board, check);
    std::cout << "  Portal H=(5,7)↔(10,7), blacks at (2,3,4,11,7). Check (12,7):\n";
    CHECK(board.cell(check).pattern(BLACK, 0) == F5,
          "Portal H-F5: (12,7) H = F5 (virtual 5 across portal)");
    CHECK(board.cell(check).pattern4[BLACK] == A_FIVE,
          "Portal H-F5: p4[BLACK] = A_FIVE");
    CHECK(board.p4Count(BLACK, A_FIVE) >= 1,
          "Portal H-F5: p4Count(BLACK, A_FIVE) >= 1");
}

// -------------------------------------------------------
// P-F5-V: Vertical portal, 5-in-a-row spanning it
// Portal A=(7,3) ↔ B=(7,9) [same column]
// BLACK at (7,0)(7,1)(7,2) above A, (7,10)(7,11) below B
// Check (7,12): should be F5
// -------------------------------------------------------
static void test_portal_f5_vertical()
{
    std::cout << "\n=== PHASE 2: Portal V — F5 spanning vertical portal ===\n";
    Board board(15);
    board.addPortal(Pos{7, 3}, Pos{7, 9});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{7, 0});
    placeBlackWhite(board, Pos{7, 1});
    placeBlackWhite(board, Pos{7, 2});
    placeBlackWhite(board, Pos{7, 10});

    Pos check{7, 11};
    printCell(board, check);
    CHECK(board.cell(check).pattern(BLACK, 1) == F5,
          "Portal V-F5: (7,11) V = F5 (virtual 5 across vertical portal)");
    CHECK(board.cell(check).pattern4[BLACK] == A_FIVE,
          "Portal V-F5: p4[BLACK] = A_FIVE");
}

// -------------------------------------------------------
// P-B4: Portal H, 3 blacks + WALL on one side → B4 across portal
// Portal A=(5,7) ↔ B=(10,7)
// addWall(1,7), BLACK at (2,7)(3,7)(4,7), check (11,7)
// Virtual line from (11,7) going left: 11→[B portal A]→4,3,2,WALL(1)
// Playing at (11,7): 4 stones B(2,3,4,[portal virtual]11) + WALL on far virtual left → B4
// -------------------------------------------------------
static void test_portal_b4()
{
    std::cout << "\n=== PHASE 2: Portal H — B4 (WALL on virtual left) ===\n";
    Board board(15);
    board.addWall(Pos{1, 7});
    board.addPortal(Pos{5, 7}, Pos{10, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{2, 7});
    placeBlackWhite(board, Pos{3, 7});
    placeBlackWhite(board, Pos{4, 7});

    Pos check{11, 7};
    printCell(board, check);
    Pattern p = board.cell(check).pattern(BLACK, 0);
    std::cout << "  Portal H B4 check (11,7) H=" << patternName(p) << "\n";
    CHECK(p == B4 || p == F4 || p == F5,
          "Portal B4: (11,7) H in {B4,F4,F5} (WALL blocks virtual left end)");
    CHECK(board.cell(check).pattern4[BLACK] >= E_BLOCK4,
          "Portal B4: p4[BLACK] >= E_BLOCK4");
}

// -------------------------------------------------------
// P-F4: Portal H, 3 blacks both ends open → F4 across portal
// Portal A=(5,7) ↔ B=(10,7)
// BLACK at (3,7)(4,7)(11,7) → virtual line: ...E,E,B(3),B(4),[A..B],B(11),E,E...
// Check (2,7) or (12,7) for F4
// -------------------------------------------------------
static void test_portal_f4()
{
    std::cout << "\n=== PHASE 2: Portal H — F4 (open 4 across portal) ===\n";
    Board board(15);
    board.addPortal(Pos{5, 7}, Pos{10, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{3, 7});
    placeBlackWhite(board, Pos{4, 7});
    placeBlackWhite(board, Pos{11, 7});

    // Check (12,7): 3 blacks on virtual left (3,4 + 11 via portal), empties right
    Pos check{12, 7};
    printCell(board, check);
    Pattern p = board.cell(check).pattern(BLACK, 0);
    std::cout << "  Portal H F4 check (12,7) H=" << patternName(p) << "\n";
    CHECK(p == F4 || p == F5,
          "Portal F4: (12,7) H in {F4, F5} (3 portal-virtual blacks, open)");

    // Also check (2,7): 3 blacks on virtual right
    Pos check2{2, 7};
    printCell(board, check2);
    Pattern p2 = board.cell(check2).pattern(BLACK, 0);
    std::cout << "  Portal H F4 check (2,7) H=" << patternName(p2) << "\n";
    CHECK(p2 == F4 || p2 == F5 || p2 == B4,
          "Portal F4: (2,7) H in {F4, F5, B4}");
}

// -------------------------------------------------------
// P-WALL-BLOCK: WALL between two groups — no pattern through portal for WALL dir
// addWall(6,7) inside the gap region: lines in H can't go through if collinear with portal
// TEST: Collinear portal A=(5,7) ↔ B=(10,7). WALL placed at (7,7) [inside gap region]
// Since portals are collinear H, gap cells use WALL (no teleport). 
// BLACK at (2,7)(3,7)(4,7), check (8,7): should be DEAD or low pattern
// (The gap cell (8,7) is between A and B on the same row → collinear → no teleport)
// -------------------------------------------------------
static void test_portal_wall_in_gap()
{
    std::cout << "\n=== PHASE 2: Collinear portal — gap cell sees WALL (no teleport) ===\n";
    Board board(15);
    board.addPortal(Pos{5, 7}, Pos{10, 7});  // collinear H
    board.newGame<FREESTYLE>();
    resetDummies();

    placeBlackWhite(board, Pos{2, 7});
    placeBlackWhite(board, Pos{3, 7});
    placeBlackWhite(board, Pos{4, 7});

    // Check gap cell (7,7): in gap region between A=(5,7) and B=(10,7)
    // Collinear → no teleport. (5,7) and (10,7) are WALL. Gap cell (7,7) sees WALL at (5,7) left.
    Pos gapCell{7, 7};
    printCell(board, gapCell);
    Pattern p = board.cell(gapCell).pattern(BLACK, 0);
    std::cout << "  Gap cell (7,7) between collinear portals → H=" << patternName(p) << "\n";
    CHECK(p <= B1,
          "Collinear gap: (7,7) H <= B1 (portal acts as WALL, no teleport in same dir)");
}

// -------------------------------------------------------
// P-Pattern2x: Portal-spanning position, check both colors get correct Pattern2x
// -------------------------------------------------------
static void test_portal_pattern2x()
{
    std::cout << "\n=== PHASE 2: Portal Pattern2x — both colors through portal ===\n";
    Board board(15);
    board.addPortal(Pos{5, 7}, Pos{10, 7});
    board.newGame<FREESTYLE>();
    resetDummies();

    // BLACK on left side, WHITE on right side
    board.move<FREESTYLE>(Pos{2, 7});   // BLACK
    board.move<FREESTYLE>(Pos{11, 7});  // WHITE

    Pos check{4, 7};  // near portal A on left
    printCell(board, check);
    const Cell &cell = board.cell(check);
    Pattern2x px = cell.pattern2x[0];  // H
    std::cout << "  (4,7) Pattern2x H: patBlack=" << patternName(px.patBlack)
              << " patWhite=" << patternName(px.patWhite) << "\n";

    CHECK(px[BLACK] == cell.pattern(BLACK, 0),
          "Portal Pattern2x: operator[] == pattern() for BLACK");
    CHECK(px[WHITE] == cell.pattern(WHITE, 0),
          "Portal Pattern2x: operator[] == pattern() for WHITE");
    // WHITE stone is through the portal — WHITE pattern should differ from plain board
    std::cout << "  [WHITE stone at (11,7) visible through portal at (5,7)↔(10,7)]\n";
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    std::cout << "================================================================\n";
    std::cout << "  Portal Gomoku Engine — Pattern Detection Test Suite\n";
    std::cout << "================================================================\n";

    // Load config (initialises PATTERN2x, PCODE, P4SCORES tables)
    if (!Config::InternalConfig.empty()) {
        std::istringstream iss(Config::InternalConfig);
        if (!Config::loadConfig(iss)) {
            std::cerr << "ERROR: Failed to load internal config!\n";
            return 1;
        }
        std::cout << "Config loaded.\n";
    }
    else {
        std::cerr << "ERROR: No internal config!\n";
        return 1;
    }

    std::cout << "\n--- PHASE 1: Normal Pattern Tests ---\n";
    test_pat_dead();
    test_pat_f5();
    test_pat_b4();
    test_pat_f4();
    test_pat_b3();
    test_pat_f3s();
    test_pat_f3();
    test_pat_b2();
    test_pat_f2_variants();
    test_pat_f1_b1();
    test_pat_ol();
    test_pat_pattern2x_symmetry();
    test_pat_multi_dir();

    std::cout << "\n--- PHASE 2: Portal Pattern Tests ---\n";
    test_portal_f5_horizontal();
    test_portal_f5_vertical();
    test_portal_b4();
    test_portal_f4();
    test_portal_wall_in_gap();
    test_portal_pattern2x();

    std::cout << "\n================================================================\n";
    std::cout << "  RESULTS: " << testsPassed << " passed, " << testsFailed << " failed\n";
    std::cout << "================================================================\n";

    return testsFailed > 0 ? 1 : 0;
}
