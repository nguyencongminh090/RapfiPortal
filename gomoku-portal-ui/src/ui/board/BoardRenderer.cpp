/*
 *  Portal Gomoku UI — Board Renderer Implementation
 *  Paper-style board with X/O pieces, brick walls, and colored ring portals.
 */

#include "BoardRenderer.hpp"
#include "../widgets/CoordOverlay.hpp"

#include <cmath>
#include <string>

namespace ui::board {

// =============================================================================
// Portal Color Palette (blue, pink, green, orange, purple, teal, gold, red)
// =============================================================================

static const PortalColor kPortalColors[] = {
    {0.20, 0.70, 0.95},   // 0: Cyan/Blue     (matches your screenshot pair 1)
    {0.90, 0.40, 0.60},   // 1: Pink/Magenta   (matches your screenshot pair 2)
    {0.30, 0.80, 0.40},   // 2: Green
    {0.95, 0.60, 0.20},   // 3: Orange
    {0.65, 0.35, 0.85},   // 4: Purple
    {0.20, 0.75, 0.70},   // 5: Teal
    {0.85, 0.75, 0.20},   // 6: Gold
    {0.85, 0.25, 0.25},   // 7: Red
};
static constexpr int kNumPortalColors = 8;

const PortalColor& BoardRenderer::portalColor(int pairIndex) {
    return kPortalColors[pairIndex % kNumPortalColors];
}

// =============================================================================
// Geometry
// =============================================================================

BoardGeometry BoardGeometry::compute(int widgetWidth, int widgetHeight, int boardSize) {
    BoardGeometry geo;
    geo.boardSize = boardSize;

    // Reserve margin for coordinate labels
    double margin = std::min(widgetWidth, widgetHeight) * 0.06;
    margin = std::max(margin, 24.0);

    double availW = widgetWidth  - 2.0 * margin;
    double availH = widgetHeight - 2.0 * margin;

    // Cell size: N cells fit in available space
    geo.cellSize = std::min(availW, availH) / boardSize;

    // Center the grid in the available space
    double gridW = boardSize * geo.cellSize;
    double gridH = boardSize * geo.cellSize;
    geo.originX = margin + (availW - gridW) / 2.0;
    geo.originY = margin + (availH - gridH) / 2.0;

    return geo;
}

// =============================================================================
// Background & Grid
// =============================================================================

void BoardRenderer::drawBackground(const Cairo::RefPtr<Cairo::Context>& cr,
                                    const BoardGeometry& geo) {
    // Paper-style background: warm beige
    cr->set_source_rgb(0.96, 0.93, 0.85);  // #F5EDDA
    cr->paint();

    double gridW = geo.boardSize * geo.cellSize;
    double gridH = geo.boardSize * geo.cellSize;

    // Draw grid lines: boardSize + 1 lines create boardSize cells
    cr->set_source_rgba(0.70, 0.65, 0.55, 0.6);
    cr->set_line_width(0.8);

    for (int i = 0; i <= geo.boardSize; ++i) {
        double pos;

        // Vertical lines
        pos = geo.originX + i * geo.cellSize;
        cr->move_to(pos, geo.originY);
        cr->line_to(pos, geo.originY + gridH);

        // Horizontal lines
        pos = geo.originY + i * geo.cellSize;
        cr->move_to(geo.originX, pos);
        cr->line_to(geo.originX + gridW, pos);
    }
    cr->stroke();

    // Board border (slightly thicker)
    cr->set_line_width(1.5);
    cr->set_source_rgba(0.55, 0.50, 0.40, 0.8);
    cr->rectangle(geo.originX, geo.originY, gridW, gridH);
    cr->stroke();
}

// =============================================================================
// Star Points
// =============================================================================

void BoardRenderer::drawStarPoints(const Cairo::RefPtr<Cairo::Context>& cr,
                                    const BoardGeometry& geo) {
    // Standard star point positions for common board sizes
    std::vector<util::Coord> stars;

    if (geo.boardSize == 15) {
        stars = {{3,3},{3,7},{3,11},{7,3},{7,7},{7,11},{11,3},{11,7},{11,11}};
    } else if (geo.boardSize == 19) {
        stars = {{3,3},{3,9},{3,15},{9,3},{9,9},{9,15},{15,3},{15,9},{15,15}};
    } else if (geo.boardSize == 20) {
        stars = {{3,3},{3,9},{3,16},{9,3},{9,9},{9,16},{16,3},{16,9},{16,16}};
    }

    cr->set_source_rgb(0.45, 0.40, 0.35);
    double dotRadius = geo.cellSize * 0.08;

    for (auto& s : stars) {
        double px, py;
        geo.cellToPixel(s.x, s.y, px, py);
        cr->arc(px, py, dotRadius, 0, 2 * M_PI);
        cr->fill();
    }
}

// =============================================================================
// Coordinates
// =============================================================================

void BoardRenderer::drawCoordinates(const Cairo::RefPtr<Cairo::Context>& cr,
                                     const BoardGeometry& geo) {
    // Draw coordinates ourselves since CoordOverlay was designed for intersection-based grids.
    cr->save();
    cr->set_source_rgb(0.3, 0.3, 0.3);
    cr->select_font_face("Sans",
                         Cairo::ToyFontFace::Slant::NORMAL,
                         Cairo::ToyFontFace::Weight::BOLD);
    cr->set_font_size(geo.cellSize * 0.35);

    double labelOffset = geo.cellSize * 0.55;

    // Column letters (A, B, C, ...) centered above each cell
    for (int x = 0; x < geo.boardSize; ++x) {
        char ch = 'A' + x;
        std::string text(1, ch);
        Cairo::TextExtents ext;
        cr->get_text_extents(text, ext);

        double px = geo.originX + (x + 0.5) * geo.cellSize - ext.width / 2.0;
        double py = geo.originY - labelOffset + ext.height / 2.0;
        cr->move_to(px, py);
        cr->show_text(text);
    }

    // Row numbers (1, 2, 3, ...) centered left of each cell
    for (int y = 0; y < geo.boardSize; ++y) {
        std::string text = std::to_string(y + 1);
        Cairo::TextExtents ext;
        cr->get_text_extents(text, ext);

        double px = geo.originX - labelOffset - ext.width / 2.0;
        double py = geo.originY + (y + 0.5) * geo.cellSize + ext.height / 2.0;
        cr->move_to(px, py);
        cr->show_text(text);
    }

    cr->restore();
}

// =============================================================================
// Highlights
// =============================================================================

void BoardRenderer::drawLastMoveHighlight(const Cairo::RefPtr<Cairo::Context>& cr,
                                           const BoardGeometry& geo,
                                           util::Coord pos) {
    double px, py;
    geo.cellToPixel(pos.x, pos.y, px, py);
    double half = geo.cellSize * 0.45;

    // Yellow highlight square
    cr->set_source_rgba(0.98, 0.88, 0.30, 0.75);  // Golden yellow
    cr->rectangle(px - half, py - half, half * 2, half * 2);
    cr->fill();
}

void BoardRenderer::drawHoverHighlight(const Cairo::RefPtr<Cairo::Context>& cr,
                                        const BoardGeometry& geo,
                                        util::Coord pos) {
    double px, py;
    geo.cellToPixel(pos.x, pos.y, px, py);
    double half = geo.cellSize * 0.42;

    cr->set_source_rgba(0.5, 0.5, 0.5, 0.15);
    cr->rectangle(px - half, py - half, half * 2, half * 2);
    cr->fill();
}

// =============================================================================
// Black Piece — Filled Diagonal Cross ✕ (fills cell)
// =============================================================================

void BoardRenderer::drawBlackPiece(const Cairo::RefPtr<Cairo::Context>& cr,
                                    const BoardGeometry& geo,
                                    int x, int y) {
    double px, py;
    geo.cellToPixel(x, y, px, py);

    // Diagonal cross — 60% visual extent (accounting for line width + round caps)
    // Visual span = 2 * (arm + lw/2) = 2 * (0.22 + 0.08) = 0.60
    double arm = geo.cellSize * 0.22;
    double lw  = geo.cellSize * 0.16;

    cr->set_source_rgb(0.10, 0.10, 0.10);  // Near-black
    cr->set_line_width(lw);
    cr->set_line_cap(Cairo::Context::LineCap::ROUND);

    // Diagonal 1: top-left to bottom-right
    cr->move_to(px - arm, py - arm);
    cr->line_to(px + arm, py + arm);
    // Diagonal 2: top-right to bottom-left
    cr->move_to(px + arm, py - arm);
    cr->line_to(px - arm, py + arm);

    cr->stroke();
}

// =============================================================================
// White Piece — Filled White Circle with Bold Black Outline (fills cell)
// =============================================================================

void BoardRenderer::drawWhitePiece(const Cairo::RefPtr<Cairo::Context>& cr,
                                    const BoardGeometry& geo,
                                    int x, int y) {
    double px, py;
    geo.cellToPixel(x, y, px, py);
    // Circle — 60% visual extent (accounting for outline width)
    // Visual span = 2 * (radius + lw/2) = 2 * (0.26 + 0.04) = 0.60
    double radius = geo.cellSize * 0.26;
    double lw     = geo.cellSize * 0.08;

    // White fill
    cr->set_source_rgb(1.0, 1.0, 1.0);
    cr->arc(px, py, radius, 0, 2 * M_PI);
    cr->fill();

    // Bold black outline
    cr->set_source_rgb(0.10, 0.10, 0.10);
    cr->set_line_width(lw);
    cr->arc(px, py, radius, 0, 2 * M_PI);
    cr->stroke();
}

// =============================================================================
// Wall — Brick Pattern (7 rows, 3 columns per row)
// =============================================================================

void BoardRenderer::drawWall(const Cairo::RefPtr<Cairo::Context>& cr,
                              const BoardGeometry& geo,
                              int x, int y) {
    double px, py;
    geo.cellToPixel(x, y, px, py);
    double half = geo.cellSize * 0.45;  // Nearly fill the cell
    double w = half * 2;
    double h = half * 2;
    double left = px - half;
    double top  = py - half;

    int rows = 7;
    int cols = 3;
    double rowH = h / rows;
    double colW = w / cols;

    // Brick background
    cr->set_source_rgb(0.80, 0.35, 0.25);  // Brick red
    cr->rectangle(left, top, w, h);
    cr->fill();

    // Mortar lines
    cr->set_source_rgb(0.90, 0.85, 0.75);  // Mortar color
    cr->set_line_width(1.0);

    // Horizontal mortar lines between rows
    for (int r = 1; r < rows; ++r) {
        double ly = top + r * rowH;
        cr->move_to(left, ly);
        cr->line_to(left + w, ly);
    }
    cr->stroke();

    // Vertical mortar lines (offset every other row for brick pattern)
    for (int r = 0; r < rows; ++r) {
        double y0 = top + r * rowH;
        double y1 = y0 + rowH;
        double xOff = (r % 2 == 0) ? 0.0 : colW * 0.5;  // Half-brick offset

        for (int c = 1; c < cols; ++c) {
            double lx = left + c * colW + xOff;
            if (lx > left && lx < left + w) {
                cr->move_to(lx, y0);
                cr->line_to(lx, y1);
            }
        }
        // Also draw the offset line that wraps from the left edge on odd rows
        if (r % 2 != 0) {
            double lx = left + colW * 0.5;
            cr->move_to(lx, y0);
            cr->line_to(lx, y1);
        }
    }
    cr->stroke();

    // Subtle border
    cr->set_source_rgba(0.50, 0.20, 0.15, 0.6);
    cr->set_line_width(1.0);
    cr->rectangle(left, top, w, h);
    cr->stroke();
}

// =============================================================================
// Portal — Colored Ring with Center Dot
// =============================================================================

void BoardRenderer::drawPortal(const Cairo::RefPtr<Cairo::Context>& cr,
                                const BoardGeometry& geo,
                                int x, int y,
                                int pairIndex) {
    double px, py;
    geo.cellToPixel(x, y, px, py);

    auto& col = portalColor(pairIndex);
    double ringRadius = geo.cellSize * 0.32;
    double ringWidth  = geo.cellSize * 0.08;
    double dotRadius  = geo.cellSize * 0.08;

    // Outer glow (subtle)
    cr->set_source_rgba(col.r, col.g, col.b, 0.25);
    cr->arc(px, py, ringRadius + ringWidth, 0, 2 * M_PI);
    cr->fill();

    // Main ring
    cr->set_source_rgb(col.r, col.g, col.b);
    cr->set_line_width(ringWidth);
    cr->arc(px, py, ringRadius, 0, 2 * M_PI);
    cr->stroke();

    // Center dot
    cr->set_source_rgb(0.15, 0.15, 0.15);
    cr->arc(px, py, dotRadius, 0, 2 * M_PI);
    cr->fill();
}

// =============================================================================
// Move Number
// =============================================================================

void BoardRenderer::drawMoveNumber(const Cairo::RefPtr<Cairo::Context>& cr,
                                    const BoardGeometry& geo,
                                    int x, int y,
                                    int moveNum,
                                    bool isBlack) {
    double px, py;
    geo.cellToPixel(x, y, px, py);

    std::string text = std::to_string(moveNum);

    cr->select_font_face("Sans",
                         Cairo::ToyFontFace::Slant::NORMAL,
                         Cairo::ToyFontFace::Weight::BOLD);
    cr->set_font_size(geo.cellSize * 0.28);

    Cairo::TextExtents ext;
    cr->get_text_extents(text, ext);

    // Contrast: white text on black piece, dark text on white piece
    if (isBlack) {
        cr->set_source_rgb(1.0, 1.0, 1.0);
    } else {
        cr->set_source_rgb(0.10, 0.10, 0.10);
    }

    cr->move_to(px - ext.width / 2.0, py + ext.height / 2.0);
    cr->show_text(text);
}

// =============================================================================
// Full Board Draw — Orchestrator
// =============================================================================

void BoardRenderer::drawBoard(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo,
                               const model::Board& board,
                               std::optional<util::Coord> lastMove,
                               std::optional<util::Coord> hoverCell) {
    cr->save();

    // 1. Background + grid
    drawBackground(cr, geo);
    drawStarPoints(cr, geo);
    drawCoordinates(cr, geo);

    // 2. Last-move highlight (behind the piece)
    if (lastMove && lastMove->isValid(geo.boardSize)) {
        drawLastMoveHighlight(cr, geo, *lastMove);
    }

    // 3. Hover highlight
    if (hoverCell && hoverCell->isValid(geo.boardSize)) {
        drawHoverHighlight(cr, geo, *hoverCell);
    }

    // 4. Draw all cells
    const auto& portals = board.topology().portals();

    for (int y = 0; y < geo.boardSize; ++y) {
        for (int x = 0; x < geo.boardSize; ++x) {
            model::Cell cell = board.cellAt(x, y);

            switch (cell) {
            case model::Cell::Black:
                drawBlackPiece(cr, geo, x, y);
                break;

            case model::Cell::White:
                drawWhitePiece(cr, geo, x, y);
                break;

            case model::Cell::Wall:
                drawWall(cr, geo, x, y);
                break;

            case model::Cell::PortalA:
            case model::Cell::PortalB: {
                // Find which portal pair this belongs to for color indexing
                int pairIdx = 0;
                for (int p = 0; p < static_cast<int>(portals.size()); ++p) {
                    if ((portals[p].a.x == x && portals[p].a.y == y) ||
                        (portals[p].b.x == x && portals[p].b.y == y)) {
                        pairIdx = p;
                        break;
                    }
                }
                drawPortal(cr, geo, x, y, pairIdx);
                break;
            }

            case model::Cell::Empty:
                break;  // Nothing to draw
            }
        }
    }

    cr->restore();
}

}  // namespace ui::board
