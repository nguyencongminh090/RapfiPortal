/*
 *  Portal Gomoku UI — Board & PortalTopology Unit Tests
 */

#include "../src/model/Board.hpp"
#include "../src/model/GameRecord.hpp"

#include <cassert>
#include <iostream>

using namespace model;
using util::Coord;

static int passed = 0;
static int failed = 0;

#define CHECK(expr, msg)                                                       \
    do {                                                                        \
        if (expr) {                                                             \
            ++passed;                                                           \
            std::cout << "  PASS: " << msg << "\n";                             \
        } else {                                                                \
            ++failed;                                                           \
            std::cerr << "  FAIL: " << msg << " [" #expr "]" << "\n";           \
        }                                                                       \
    } while (0)

// =============================================================================
// PortalTopology Tests
// =============================================================================

void test_portal_topology() {
    std::cout << "=== PortalTopology Tests ===\n";

    PortalTopology topo;

    // Initially empty
    CHECK(topo.empty(), "new topology is empty");
    CHECK(topo.obstacleCount() == 0, "no obstacles");

    // Add walls
    CHECK(topo.addWall({3, 3}), "addWall(3,3) succeeds");
    CHECK(topo.addWall({4, 4}), "addWall(4,4) succeeds");
    CHECK(!topo.addWall({3, 3}), "addWall duplicate fails");
    CHECK(topo.hasWall({3, 3}), "hasWall(3,3) true");
    CHECK(!topo.hasWall({5, 5}), "hasWall(5,5) false");
    CHECK(topo.walls().size() == 2, "2 walls stored");

    // Remove wall
    CHECK(topo.removeWall({4, 4}), "removeWall(4,4) succeeds");
    CHECK(!topo.hasWall({4, 4}), "wall removed");
    CHECK(!topo.removeWall({4, 4}), "removeWall again fails");

    // Add portals
    CHECK(topo.addPortal({5, 5}, {10, 10}), "addPortal succeeds");
    CHECK(topo.hasPortal({5, 5}), "hasPortal(A) true");
    CHECK(topo.hasPortal({10, 10}), "hasPortal(B) true");
    CHECK(!topo.hasPortal({7, 7}), "hasPortal(other) false");
    CHECK(topo.portals().size() == 1, "1 portal pair");

    // Portal partner lookup
    auto partnerA = topo.portalPartner({5, 5});
    CHECK(partnerA == (Coord{10, 10}), "partner of A is B");
    auto partnerB = topo.portalPartner({10, 10});
    CHECK(partnerB == (Coord{5, 5}), "partner of B is A");
    CHECK(topo.portalPartner({7, 7}) == Coord::none(), "partner of non-portal is none");

    // Cannot add portal where wall exists
    CHECK(!topo.addPortal({3, 3}, {8, 8}), "portal at wall position fails");

    // Cannot add wall where portal exists
    CHECK(!topo.addWall({5, 5}), "wall at portal position fails");

    // Cannot add portal with same endpoints
    CHECK(!topo.addPortal({1, 1}, {1, 1}), "portal A==B fails");

    // Cannot add portal at occupied portal position
    CHECK(!topo.addPortal({5, 5}, {12, 12}), "portal at existing portal fails");

    // isOccupied
    CHECK(topo.isOccupied({3, 3}), "wall is occupied");
    CHECK(topo.isOccupied({5, 5}), "portal is occupied");
    CHECK(!topo.isOccupied({7, 7}), "empty is not occupied");

    // Obstacle count: 1 wall + 1 portal pair (2 endpoints) = 3
    CHECK(topo.obstacleCount() == 3, "obstacleCount = 3");

    // Remove portal
    CHECK(topo.removePortal({10, 10}), "removePortal via B endpoint");
    CHECK(!topo.hasPortal({5, 5}), "A also removed");
    CHECK(!topo.hasPortal({10, 10}), "B removed");

    // Clear
    topo.addPortal({1, 1}, {2, 2});
    topo.clear();
    CHECK(topo.empty(), "clear makes empty");
}

// =============================================================================
// Board — Basic Operations
// =============================================================================

void test_board_basic() {
    std::cout << "=== Board Basic Tests ===\n";

    Board board(15);

    CHECK(board.size() == 15, "size is 15");
    CHECK(board.ply() == 0, "ply starts at 0");
    CHECK(board.sideToMove() == Color::Black, "black moves first");
    CHECK(board.emptyCells() == 225, "15*15 = 225 empty cells");
    CHECK(board.totalStones() == 0, "no stones");

    // Place stones
    CHECK(board.placeStone(7, 7, Color::Black), "place black at (7,7)");
    CHECK(board.cellAt(7, 7) == Cell::Black, "cell is black");
    CHECK(board.ply() == 1, "ply = 1");
    CHECK(board.sideToMove() == Color::White, "white moves next");
    CHECK(board.totalStones() == 1, "1 stone");
    CHECK(board.stoneCount(Color::Black) == 1, "1 black");

    CHECK(board.placeStone(8, 8, Color::White), "place white at (8,8)");
    CHECK(board.cellAt(8, 8) == Cell::White, "cell is white");
    CHECK(board.ply() == 2, "ply = 2");
    CHECK(board.totalStones() == 2, "2 stones");

    // Can't place on occupied cell
    CHECK(!board.placeStone(7, 7, Color::White), "can't place on occupied");

    // Can't place out of bounds
    CHECK(!board.placeStone(15, 0, Color::Black), "can't place OOB x");
    CHECK(!board.placeStone(-1, 0, Color::Black), "can't place OOB negative");

    // Last move
    CHECK(board.lastMove().coord.x == 8, "lastMove x=8");
    CHECK(board.lastMove().coord.y == 8, "lastMove y=8");
    CHECK(board.lastMove().color == Color::White, "lastMove color=White");

    // History
    CHECK(board.history().size() == 2, "history has 2 entries");
}

// =============================================================================
// Board — Undo
// =============================================================================

void test_board_undo() {
    std::cout << "=== Board Undo Tests ===\n";

    Board board(15);
    board.placeStone(7, 7, Color::Black);
    board.placeStone(8, 8, Color::White);

    // Undo white
    Move undone = board.undoLast();
    CHECK(undone.coord.x == 8 && undone.coord.y == 8, "undone move is (8,8)");
    CHECK(undone.color == Color::White, "undone color white");
    CHECK(board.isEmpty(8, 8), "cell (8,8) is empty after undo");
    CHECK(board.ply() == 1, "ply = 1 after undo");
    CHECK(board.totalStones() == 1, "1 stone after undo");

    // Undo black
    board.undoLast();
    CHECK(board.isEmpty(7, 7), "cell (7,7) empty after undo");
    CHECK(board.ply() == 0, "ply = 0");

    // Undo on empty throws
    bool threw = false;
    try { board.undoLast(); } catch (...) { threw = true; }
    CHECK(threw, "undoLast on empty history throws");
}

// =============================================================================
// Board — Pass Moves
// =============================================================================

void test_board_pass() {
    std::cout << "=== Board Pass Tests ===\n";

    Board board(15);
    board.placeStone(7, 7, Color::Black);
    board.pass(Color::White);

    CHECK(board.ply() == 2, "ply = 2 after stone + pass");
    CHECK(board.totalStones() == 1, "pass doesn't add stone");
    CHECK(board.lastMove().isPass(), "last move is pass");

    // Undo pass
    Move undone = board.undoLast();
    CHECK(undone.isPass(), "undone move is pass");
    CHECK(board.ply() == 1, "ply = 1 after undo pass");
    CHECK(board.totalStones() == 1, "stone count unchanged");
}

// =============================================================================
// Board — Topology
// =============================================================================

void test_board_topology() {
    std::cout << "=== Board Topology Tests ===\n";

    Board board(15);

    PortalTopology topo;
    topo.addWall({3, 3});
    topo.addWall({4, 4});
    topo.addPortal({5, 5}, {10, 10});

    board.setTopology(topo);

    // Walls and portals are on the grid
    CHECK(board.isWall(3, 3), "cell (3,3) is wall");
    CHECK(board.isWall(4, 4), "cell (4,4) is wall");
    CHECK(board.isPortal(5, 5), "cell (5,5) is portal");
    CHECK(board.isPortal(10, 10), "cell (10,10) is portal");
    CHECK(board.cellAt(5, 5) == Cell::PortalA, "portal A cell type");
    CHECK(board.cellAt(10, 10) == Cell::PortalB, "portal B cell type");

    // Can't place stone on wall or portal
    CHECK(!board.placeStone(3, 3, Color::Black), "can't place on wall");
    CHECK(!board.placeStone(5, 5, Color::Black), "can't place on portal");

    // Portal partner lookup
    auto partner = board.portalPartner(5, 5);
    CHECK(partner.has_value(), "portal partner exists");
    CHECK(partner->x == 10 && partner->y == 10, "partner is (10,10)");

    // Non-portal returns nullopt
    CHECK(!board.portalPartner(7, 7).has_value(), "non-portal has no partner");

    // Empty cells = 225 - 4 obstacles
    CHECK(board.emptyCells() == 221, "221 empty cells with topology");

    // Reset/keep topology
    board.placeStone(7, 7, Color::Black);
    board.resetKeepTopology();
    CHECK(board.ply() == 0, "ply = 0 after resetKeepTopology");
    CHECK(board.isWall(3, 3), "wall survives resetKeepTopology");
    CHECK(board.isPortal(5, 5), "portal survives resetKeepTopology");
    CHECK(board.isEmpty(7, 7), "stone cleared by resetKeepTopology");

    // Clear topology
    board.clearTopology();
    CHECK(board.isEmpty(3, 3), "wall cleared");
    CHECK(board.isEmpty(5, 5), "portal cleared");
    CHECK(board.topology().empty(), "topology is empty");
    CHECK(board.emptyCells() == 225, "all cells empty after clearTopology");
}

// =============================================================================
// Board — Reset
// =============================================================================

void test_board_reset() {
    std::cout << "=== Board Reset Tests ===\n";

    Board board(15);
    board.placeStone(7, 7, Color::Black);

    PortalTopology topo;
    topo.addWall({3, 3});
    board.setTopology(topo);

    board.reset(20);
    CHECK(board.size() == 20, "size = 20 after reset");
    CHECK(board.ply() == 0, "ply = 0 after reset");
    CHECK(board.emptyCells() == 400, "20*20 = 400 empty");
    CHECK(board.topology().empty(), "topology cleared on reset");
}

// =============================================================================
// GameRecord
// =============================================================================

void test_game_record() {
    std::cout << "=== GameRecord Tests ===\n";

    Board board(15);

    PortalTopology topo;
    topo.addWall({3, 3});
    topo.addPortal({5, 5}, {10, 10});
    board.setTopology(topo);

    board.placeStone(7, 7, Color::Black);
    board.placeStone(8, 8, Color::White);

    // Create record from board
    auto rec = GameRecord::fromBoard(board);
    CHECK(rec.boardSize == 15, "record boardSize = 15");
    CHECK(rec.moves.size() == 2, "record has 2 moves");
    CHECK(rec.topology.walls().size() == 1, "record has 1 wall");
    CHECK(rec.topology.portals().size() == 1, "record has 1 portal pair");

    // toBoardEntries
    auto entries = rec.toBoardEntries(Color::Black);
    // Should have: 1 wall(color=3) + 2 stones
    CHECK(entries.size() == 3, "3 board entries (1 wall + 2 stones)");

    // Find the wall entry
    bool foundWall = false;
    for (auto& [x, y, c] : entries) {
        if (x == 3 && y == 3 && c == 3) foundWall = true;
    }
    CHECK(foundWall, "wall entry present with color=3");

    // Find black stone (SELF=1 when selfColor=Black)
    bool foundBlack = false;
    for (auto& [x, y, c] : entries) {
        if (x == 7 && y == 7 && c == 1) foundBlack = true;
    }
    CHECK(foundBlack, "black stone has color=1 (SELF)");

    // Find white stone (OPPO=2 when selfColor=Black)
    bool foundWhite = false;
    for (auto& [x, y, c] : entries) {
        if (x == 8 && y == 8 && c == 2) foundWhite = true;
    }
    CHECK(foundWhite, "white stone has color=2 (OPPO)");

    // Flip self color
    auto entries2 = rec.toBoardEntries(Color::White);
    bool blackIsOppo = false;
    for (auto& [x, y, c] : entries2) {
        if (x == 7 && y == 7 && c == 2) blackIsOppo = true;
    }
    CHECK(blackIsOppo, "when selfColor=White, black stone is color=2 (OPPO)");
}

// =============================================================================
// Cell utility tests
// =============================================================================

void test_cell_utils() {
    std::cout << "=== Cell Utility Tests ===\n";

    CHECK(isStone(Cell::Black), "Black is a stone");
    CHECK(isStone(Cell::White), "White is a stone");
    CHECK(!isStone(Cell::Empty), "Empty is not a stone");
    CHECK(!isStone(Cell::Wall), "Wall is not a stone");

    CHECK(model::isPortal(Cell::PortalA), "PortalA is portal");
    CHECK(model::isPortal(Cell::PortalB), "PortalB is portal");
    CHECK(!model::isPortal(Cell::Wall), "Wall is not portal");

    CHECK(isObstacle(Cell::Wall), "Wall is obstacle");
    CHECK(isObstacle(Cell::PortalA), "PortalA is obstacle");
    CHECK(!isObstacle(Cell::Empty), "Empty is not obstacle");
    CHECK(!isObstacle(Cell::Black), "Stone is not obstacle");

    CHECK(opponent(Color::Black) == Color::White, "opponent of Black = White");
    CHECK(opponent(Color::White) == Color::Black, "opponent of White = Black");

    CHECK(colorToCell(Color::Black) == Cell::Black, "colorToCell Black");
    CHECK(colorToCell(Color::White) == Cell::White, "colorToCell White");

    CHECK(cellToProtocolColor(Cell::Black) == 1, "protocol color Black=1");
    CHECK(cellToProtocolColor(Cell::White) == 2, "protocol color White=2");
    CHECK(cellToProtocolColor(Cell::Wall) == 3, "protocol color Wall=3");
    CHECK(cellToProtocolColor(Cell::Empty) == 0, "protocol color Empty=0");
}

// =============================================================================
// Move tests
// =============================================================================

void test_move() {
    std::cout << "=== Move Tests ===\n";

    Move m = {{7, 7}, Color::Black, 0};
    CHECK(!m.isPass(), "normal move is not pass");
    CHECK(m.toString() == "B(7,7)@0", "toString normal move");

    Move p = Move::pass(Color::White, 3);
    CHECK(p.isPass(), "pass move is pass");
    CHECK(p.toString() == "W(PASS)@3", "toString pass move");
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "================================================================\n";
    std::cout << "  Portal Gomoku UI — Board & Model Test Suite\n";
    std::cout << "================================================================\n";

    test_cell_utils();
    test_move();
    test_portal_topology();
    test_board_basic();
    test_board_undo();
    test_board_pass();
    test_board_topology();
    test_board_reset();
    test_game_record();

    std::cout << "\n  RESULTS: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
