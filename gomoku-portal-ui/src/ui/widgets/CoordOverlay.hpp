/*
 *  Portal Gomoku UI — Coordinate Overlay
 *  Draws A-O column and 1-15 row labels.
 *  Designed to be called FROM BoardCanvas::draw_content, NOT as a standalone overlay.
 *  Kept as a utility class with a static draw method.
 */

#pragma once

#include <cairomm/context.h>
#include <string>

namespace ui::widgets {

/// Utility for drawing coordinate labels around a board grid.
/// Not a standalone widget — called from BoardCanvas's draw function
/// to ensure pixel-perfect alignment with the grid.
class CoordOverlay {
public:
    /// Draw coordinate labels around the board.
    /// @param cr        Cairo context (from the BoardCanvas draw function)
    /// @param boardSize Number of lines (e.g. 15)
    /// @param cellSize  Pixel distance between grid lines
    /// @param originX   X pixel of grid line 0 (top-left intersection)
    /// @param originY   Y pixel of grid line 0 (top-left intersection)
    static void draw(const Cairo::RefPtr<Cairo::Context>& cr,
                     int boardSize, double cellSize,
                     double originX, double originY);
};

}  // namespace ui::widgets
