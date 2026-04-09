/*
 *  Portal Gomoku UI — Board Model Implementation
 */

#include "Board.hpp"

#include <algorithm>
#include <stdexcept>

namespace model {

Board::Board(int size) : size_(size), cells_(size * size, Cell::Empty) {}

// =============================================================================
// Mutators
// =============================================================================

void Board::reset(int size) {
    size_ = size;
    cells_.assign(size * size, Cell::Empty);
    history_.clear();
    redoStack_.clear();
    topology_.clear();
}

void Board::resetKeepTopology() {
    cells_.assign(size_ * size_, Cell::Empty);
    history_.clear();
    redoStack_.clear();
    applyTopologyToGrid();
}

bool Board::placeStone(int x, int y, Color color) {
    if (!inBounds(x, y)) return false;
    if (cells_[idx(x, y)] != Cell::Empty) return false;

    cells_[idx(x, y)] = colorToCell(color);
    history_.push_back(Move{{x, y}, color, ply() - 1});
    redoStack_.clear(); // Clear redo stack on new action
    // Note: ply() already incremented because we pushed to history_
    // Fix: set ply to history size - 1
    history_.back().ply = static_cast<int>(history_.size()) - 1;
    return true;
}

bool Board::placeStone(util::Coord pos, Color color) {
    return placeStone(pos.x, pos.y, color);
}

void Board::pass(Color color) {
    history_.push_back(Move::pass(color, ply()));
}

void Board::passInternal(Color color, int ply) {
    // BUG-007 FIX: intentionally does NOT clear redoStack_
    history_.push_back(Move::pass(color, ply));
}

Move Board::undoLast() {
    if (history_.empty())
        throw std::runtime_error("Board::undoLast() called on empty history");

    Move last = history_.back();
    history_.pop_back();

    // Remove the stone from the grid (if it wasn't a pass)
    if (!last.isPass() && inBounds(last.coord.x, last.coord.y)) {
        cells_[idx(last.coord.x, last.coord.y)] = Cell::Empty;
    }
    
    // Add to redo stack
    redoStack_.push_back(last);
    
    return last;
}

bool Board::redoMove() {
    if (redoStack_.empty()) return false;

    Move next = redoStack_.back();
    redoStack_.pop_back();

    if (next.isPass()) {
        passInternal(next.color, next.ply);  // BUG-007 FIX: preserves remaining redo entries
    } else {
        if (inBounds(next.coord.x, next.coord.y)) {
            cells_[idx(next.coord.x, next.coord.y)] = colorToCell(next.color);
        }
        history_.push_back(next);
    }

    return true;
}

void Board::setTopology(const PortalTopology& topo) {
    clearTopologyFromGrid();
    topology_ = topo;
    applyTopologyToGrid();
}

void Board::clearTopology() {
    clearTopologyFromGrid();
    topology_.clear();
}

// =============================================================================
// Queries
// =============================================================================

Cell Board::cellAt(int x, int y) const {
    if (!inBounds(x, y))
        throw std::out_of_range("Board::cellAt out of bounds");
    return cells_[idx(x, y)];
}

Cell Board::cellAt(util::Coord pos) const {
    return cellAt(pos.x, pos.y);
}

bool Board::inBounds(int x, int y) const {
    return x >= 0 && x < size_ && y >= 0 && y < size_;
}

bool Board::isEmpty(int x, int y) const {
    return inBounds(x, y) && cells_[idx(x, y)] == Cell::Empty;
}

bool Board::isWall(int x, int y) const {
    return inBounds(x, y) && cells_[idx(x, y)] == Cell::Wall;
}

bool Board::isPortal(int x, int y) const {
    return inBounds(x, y) && model::isPortal(cells_[idx(x, y)]);
}

std::optional<util::Coord> Board::portalPartner(int x, int y) const {
    if (!isPortal(x, y)) return std::nullopt;
    auto partner = topology_.portalPartner({x, y});
    if (partner == util::Coord::none()) return std::nullopt;
    return partner;
}

const Move& Board::lastMove() const {
    if (history_.empty())
        throw std::runtime_error("Board::lastMove() called on empty history");
    return history_.back();
}

Color Board::sideToMove() const {
    return (ply() % 2 == 0) ? Color::Black : Color::White;
}

int Board::stoneCount(Color color) const {
    Cell target = colorToCell(color);
    return static_cast<int>(std::count(cells_.begin(), cells_.end(), target));
}

int Board::totalStones() const {
    return stoneCount(Color::Black) + stoneCount(Color::White);
}

int Board::emptyCells() const {
    return static_cast<int>(std::count(cells_.begin(), cells_.end(), Cell::Empty));
}

// =============================================================================
// Private
// =============================================================================

void Board::applyTopologyToGrid() {
    for (auto& w : topology_.walls()) {
        if (inBounds(w.x, w.y))
            cells_[idx(w.x, w.y)] = Cell::Wall;
    }
    for (auto& p : topology_.portals()) {
        if (inBounds(p.a.x, p.a.y))
            cells_[idx(p.a.x, p.a.y)] = Cell::PortalA;
        if (inBounds(p.b.x, p.b.y))
            cells_[idx(p.b.x, p.b.y)] = Cell::PortalB;
    }
}

void Board::clearTopologyFromGrid() {
    for (auto& w : topology_.walls()) {
        if (inBounds(w.x, w.y) && cells_[idx(w.x, w.y)] == Cell::Wall)
            cells_[idx(w.x, w.y)] = Cell::Empty;
    }
    for (auto& p : topology_.portals()) {
        if (inBounds(p.a.x, p.a.y) && cells_[idx(p.a.x, p.a.y)] == Cell::PortalA)
            cells_[idx(p.a.x, p.a.y)] = Cell::Empty;
        if (inBounds(p.b.x, p.b.y) && cells_[idx(p.b.x, p.b.y)] == Cell::PortalB)
            cells_[idx(p.b.x, p.b.y)] = Cell::Empty;
    }
}

}  // namespace model
