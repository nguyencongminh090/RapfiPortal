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
                                       util::Coord pos,
                                       const model::Board& board,
                                       std::optional<HoverSetupInfo> setupHover) {
    double px, py;
    geo.cellToPixel(pos.x, pos.y, px, py);

    cr->save();
    
    if (setupHover) {
        // Setup Mode Hover
        switch (setupHover->tool) {
            case HoverSetupInfo::Tool::Wall:
                if (!board.topology().hasWall(pos) && !board.topology().hasPortal(pos)) {
                    cr->set_source_rgba(1.0, 1.0, 1.0, 0.4); // White backdrop
                    cr->rectangle(px - geo.cellSize/2, py - geo.cellSize/2, geo.cellSize, geo.cellSize);
                    cr->fill();
                    
                    cr->push_group();
                    drawWall(cr, geo, pos.x, pos.y);
                    cr->pop_group_to_source();
                    cr->paint_with_alpha(0.6); // 60% opacity for pending wall
                }
                break;

            case HoverSetupInfo::Tool::PortalPair:
                if (!board.topology().hasWall(pos) && !board.topology().hasPortal(pos)) {
                    cr->set_source_rgba(1.0, 1.0, 1.0, 0.4);
                    cr->rectangle(px - geo.cellSize/2, py - geo.cellSize/2, geo.cellSize, geo.cellSize);
                    cr->fill();
                    
                    int pairIndex = static_cast<int>(board.topology().portals().size());
                    cr->push_group();
                    drawPortal(cr, geo, pos.x, pos.y, pairIndex);
                    cr->pop_group_to_source();
                    cr->paint_with_alpha(0.6); // 60% opacity for pending portal
                    
                    // If it's the second endpoint (B), draw a link line
                    if (setupHover->pendingPortalA && *setupHover->pendingPortalA != pos) {
                        double ax, ay;
                        geo.cellToPixel(setupHover->pendingPortalA->x, setupHover->pendingPortalA->y, ax, ay);
                        const auto& color = portalColor(pairIndex);
                        cr->set_source_rgba(color.r, color.g, color.b, 0.4);
                        cr->set_line_width(2.0);
                        std::vector<double> dashes = {5.0, 5.0};
                        cr->set_dash(dashes, 0.0);
                        cr->move_to(ax, ay);
                        cr->line_to(px, py);
                        cr->stroke();
                        cr->set_dash(std::vector<double>{}, 0.0); // Reset
                    }
                }
                break;

            case HoverSetupInfo::Tool::Eraser:
                if (board.topology().hasWall(pos) || board.topology().hasPortal(pos)) {
                    cr->set_source_rgba(0.9, 0.2, 0.2, 0.5); // Red overlay
                    cr->rectangle(px - geo.cellSize/2, py - geo.cellSize/2, geo.cellSize, geo.cellSize);
                    cr->fill();
                }
                break;

            default: break;
        }
    } else {
        // Normal Play Mode Hover
        if (board.isEmpty(pos.x, pos.y) && !board.topology().hasWall(pos) && !board.topology().hasPortal(pos)) {
            cr->set_source_rgba(0.8, 0.8, 0.8, 0.5);
            cr->rectangle(px - geo.cellSize/2, py - geo.cellSize/2, geo.cellSize, geo.cellSize);
            cr->fill();
        }
    }

    cr->restore();
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
// Analysis Overlays
// =============================================================================

void BoardRenderer::drawAnalysisOverlays(const Cairo::RefPtr<Cairo::Context>& cr,
                                         const BoardGeometry& geo,
                                         model::Color nextColor,
                                         const model::AnalysisInfo& info,
                                         const std::optional<model::AnalysisMove>& hoverPV) {
    cr->save();

    // 1. Draw N-Best scores
    cr->select_font_face("Sans",
                         Cairo::ToyFontFace::Slant::NORMAL,
                         Cairo::ToyFontFace::Weight::NORMAL);
    cr->set_font_size(geo.cellSize * 0.28);
    
    for (const auto& mv : info.nBest) {
        if (!mv.coord.isValid(geo.boardSize) || mv.score == 0) continue;
        
        double px, py;
        geo.cellToPixel(mv.coord.x, mv.coord.y, px, py);
        
        // Draw score box in bottom right corner of the cell
        std::string scoreTxt = std::to_string(mv.score);
        Cairo::TextExtents te;
        cr->get_text_extents(scoreTxt, te);
        
        double boxH = te.height + 4;
        double boxW = te.width + 6;
        double boxX = px + geo.cellSize * 0.5 - boxW - 2;
        double boxY = py + geo.cellSize * 0.5 - boxH - 2;
        
        cr->set_source_rgba(0.2, 0.2, 0.2, 0.7);
        cr->rectangle(boxX, boxY, boxW, boxH);
        cr->fill();
        
        cr->set_source_rgb(1.0, 1.0, 1.0);
        cr->move_to(boxX + 3, boxY + boxH - 3);
        cr->show_text(scoreTxt);
    }
    
    // 2. Draw phantom PV line if hovered
    if (hoverPV) {
        model::Color currentColor = nextColor;
        int stepNum = 1;
        
        for (const auto& cell : hoverPV->pv) {
            if (!cell.isValid(geo.boardSize)) break;
            
            double px, py;
            geo.cellToPixel(cell.x, cell.y, px, py);
            
            // Draw phantom stone
            cr->set_source_rgba(currentColor == model::Color::Black ? 0.0 : 1.0,
                                currentColor == model::Color::Black ? 0.0 : 1.0,
                                currentColor == model::Color::Black ? 0.0 : 1.0,
                                0.4);
            cr->arc(px, py, geo.cellSize * 0.42, 0, 2 * M_PI);
            cr->fill();
            
            // Draw sequence number inside stone
            cr->set_source_rgb(currentColor == model::Color::Black ? 1.0 : 0.0,
                               currentColor == model::Color::Black ? 1.0 : 0.0,
                               currentColor == model::Color::Black ? 1.0 : 0.0);
            
            cr->select_font_face("Sans",
                                 Cairo::ToyFontFace::Slant::NORMAL,
                                 Cairo::ToyFontFace::Weight::BOLD);
            cr->set_font_size(geo.cellSize * 0.4);
                                 
            std::string numTxt = std::to_string(stepNum++);
            Cairo::TextExtents te;
            cr->get_text_extents(numTxt, te);
            cr->move_to(px - te.width / 2.0 - te.x_bearing,
                        py - te.height / 2.0 - te.y_bearing);
            cr->show_text(numTxt);
            
            // Toggle color
            currentColor = (currentColor == model::Color::Black) ? model::Color::White : model::Color::Black;
        }
    }
    
    cr->restore();
}

// =============================================================================
// Full Board Draw — Orchestrator
// =============================================================================

void BoardRenderer::drawBoard(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo,
                               const model::Board& board,
                               std::optional<util::Coord> lastMove,
                               std::optional<util::Coord> hoverCell,
                               std::optional<HoverSetupInfo> setupHover) {
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
    if (hoverCell && board.inBounds(hoverCell->x, hoverCell->y)) {
        drawHoverHighlight(cr, geo, *hoverCell, board, setupHover);
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
