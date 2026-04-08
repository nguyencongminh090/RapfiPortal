/*
 *  Portal Gomoku UI — Coordinate Overlay
 *  Draws the A-Z and 1-15 labels around the edges of the board.
 */

#pragma once

#include <gtkmm/drawingarea.h>

namespace ui::widgets {

class CoordOverlay : public Gtk::DrawingArea {
public:
    CoordOverlay();

    /// Set board size (e.g. 15 for 15x15)
    void setBoardSize(int size);

    /// Set the pixel size of a single grid cell
    void setCellSize(double cellSize);

protected:
    void draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

private:
    int boardSize_ = 15;
    double cellSize_ = 30.0;
};

} // namespace ui::widgets
