/*
 *  Portal Gomoku UI — Board Canvas Implementation
 */

#include "BoardCanvas.hpp"

namespace ui::board {

BoardCanvas::BoardCanvas(model::Board& board) : board_(board) {
    set_content_width(600);
    set_content_height(600);
    set_hexpand(true);
    set_vexpand(true);

    // Drawing
    set_draw_func(sigc::mem_fun(*this, &BoardCanvas::draw_content));

    // Left-Click gesture
    auto clickGesture = Gtk::GestureClick::create();
    clickGesture->set_button(GDK_BUTTON_PRIMARY);
    clickGesture->signal_pressed().connect(
        sigc::mem_fun(*this, &BoardCanvas::on_click));
    add_controller(clickGesture);

    // Right-Click gesture
    auto rightClickGesture = Gtk::GestureClick::create();
    rightClickGesture->set_button(GDK_BUTTON_SECONDARY);
    rightClickGesture->signal_pressed().connect(
        sigc::mem_fun(*this, &BoardCanvas::on_right_click));
    add_controller(rightClickGesture);

    // Motion controller (for hover highlight)
    auto motionCtrl = Gtk::EventControllerMotion::create();
    motionCtrl->signal_motion().connect(
        sigc::mem_fun(*this, &BoardCanvas::on_motion));
    motionCtrl->signal_leave().connect(
        sigc::mem_fun(*this, &BoardCanvas::on_leave));
    add_controller(motionCtrl);
}

void BoardCanvas::refresh() {
    queue_draw();
}

void BoardCanvas::setShowMoveNumbers(bool show) {
    showMoveNumbers_ = show;
    queue_draw();
}

// =============================================================================
// Draw
// =============================================================================

void BoardCanvas::draw_content(const Cairo::RefPtr<Cairo::Context>& cr,
                                int width, int height) {
    // Recompute geometry from current widget size
    geo_ = BoardGeometry::compute(width, height, board_.size());

    // Determine last move for highlight
    std::optional<util::Coord> lastMove;
    if (board_.ply() > 0) {
        const auto& lm = board_.lastMove();
        if (!lm.isPass()) lastMove = lm.coord;
    }

    // Full board draw
    BoardRenderer::drawBoard(cr, geo_, board_, lastMove, hoverCell_);

    // Move numbers overlay (optional)
    if (showMoveNumbers_) {
        for (const auto& move : board_.history()) {
            if (move.isPass()) continue;
            bool isBlack = (move.color == model::Color::Black);
            BoardRenderer::drawMoveNumber(cr, geo_,
                                          move.coord.x, move.coord.y,
                                          move.ply + 1,  // 1-based display
                                          isBlack);
        }
    }
}

// =============================================================================
// Input
// =============================================================================

void BoardCanvas::on_click(int /*n_press*/, double x, double y) {
    auto cell = geo_.pixelToCell(x, y);
    if (cell != util::Coord::none()) {
        signalCellClicked.emit(cell.x, cell.y);
    }
}

void BoardCanvas::on_right_click(int /*n_press*/, double x, double y) {
    auto cell = geo_.pixelToCell(x, y);
    if (cell != util::Coord::none()) {
        signalCellRightClicked.emit(cell.x, cell.y);
    }
}

void BoardCanvas::on_motion(double x, double y) {
    auto cell = geo_.pixelToCell(x, y);
    if (cell != hoverCell_) {
        hoverCell_ = (cell != util::Coord::none())
                     ? std::optional<util::Coord>(cell)
                     : std::nullopt;
        queue_draw();
    }
}

void BoardCanvas::on_leave() {
    if (hoverCell_) {
        hoverCell_ = std::nullopt;
        queue_draw();
    }
}

}  // namespace ui::board
