/*
 *  Portal Gomoku UI — Coordinate Overlay Implementation
 */

#include "CoordOverlay.hpp"

namespace ui::widgets {

CoordOverlay::CoordOverlay() {
    set_draw_func(sigc::mem_fun(*this, &CoordOverlay::draw_content));
}

void CoordOverlay::setBoardSize(int size) {
    if (boardSize_ != size) {
        boardSize_ = size;
        queue_draw();
    }
}

void CoordOverlay::setCellSize(double cellSize) {
    if (cellSize_ != cellSize) {
        cellSize_ = cellSize;
        queue_draw();
    }
}

void CoordOverlay::draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width [[maybe_unused]], int height [[maybe_unused]]) {
    cr->set_source_rgb(0.3, 0.3, 0.3); // Dark gray text
    cr->select_font_face("Sans", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::BOLD);
    cr->set_font_size(cellSize_ * 0.4);

    // Assuming the top-left cell is at (offset, offset)
    // We will draw letters on the top edge and numbers on the left edge.
    // For a real integration, the BoardCanvas and CoordOverlay might share geometry.
    // Here we just draw a basic set of labels spaced by cellSize_.

    double offset = cellSize_; // Leave space for the labels themselves

    // Draw Top Letters (A, B, C...)
    for (int x = 0; x < boardSize_; ++x) {
        char label = 'A' + x;
        // Skip 'I' in some Gomoku traditions, but Gomocup usually just uses A-O
        std::string text(1, label); 

        Cairo::TextExtents extents;
        cr->get_text_extents(text, extents);

        double tx = offset + x * cellSize_ + (cellSize_ / 2.0) - (extents.width / 2.0);
        double ty = offset - (cellSize_ * 0.2); // Just above the grid

        cr->move_to(tx, ty);
        cr->show_text(text);
    }

    // Draw Left Numbers (15, 14, 13...1 or 1, 2, 3...15)
    // Standard Gomoku uses 15 at top, 1 at bottom typically. Let's use 1-15 top to bottom.
    for (int y = 0; y < boardSize_; ++y) {
        std::string text = std::to_string(y + 1);

        Cairo::TextExtents extents;
        cr->get_text_extents(text, extents);

        double tx = offset - (cellSize_ * 0.2) - extents.width; // Just left of the grid
        double ty = offset + y * cellSize_ + (cellSize_ / 2.0) + (extents.height / 2.0);

        cr->move_to(tx, ty);
        cr->show_text(text);
    }
}

} // namespace ui::widgets
