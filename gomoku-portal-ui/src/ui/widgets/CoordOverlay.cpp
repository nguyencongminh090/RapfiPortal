/*
 *  Portal Gomoku UI — Coordinate Overlay Implementation
 */

#include "CoordOverlay.hpp"

namespace ui::widgets {

void CoordOverlay::draw(const Cairo::RefPtr<Cairo::Context>& cr,
                        int boardSize, double cellSize,
                        double originX, double originY) {
    cr->save();

    cr->set_source_rgb(0.3, 0.3, 0.3);
    cr->select_font_face("Sans",
                         Cairo::ToyFontFace::Slant::NORMAL,
                         Cairo::ToyFontFace::Weight::BOLD);
    cr->set_font_size(cellSize * 0.35);

    double labelOffset = cellSize * 0.55;  // Distance from grid edge to label center

    // Column labels (A, B, C, ...) across the top
    for (int x = 0; x < boardSize; ++x) {
        char ch = 'A' + x;
        std::string text(1, ch);

        Cairo::TextExtents ext;
        cr->get_text_extents(text, ext);

        double tx = originX + x * cellSize - ext.width / 2.0;
        double ty = originY - labelOffset + ext.height / 2.0;

        cr->move_to(tx, ty);
        cr->show_text(text);
    }

    // Row labels (1, 2, 3, ...) down the left side
    for (int y = 0; y < boardSize; ++y) {
        std::string text = std::to_string(y + 1);

        Cairo::TextExtents ext;
        cr->get_text_extents(text, ext);

        double tx = originX - labelOffset - ext.width / 2.0;
        double ty = originY + y * cellSize + ext.height / 2.0;

        cr->move_to(tx, ty);
        cr->show_text(text);
    }

    cr->restore();
}

}  // namespace ui::widgets
