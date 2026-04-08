/*
 *  Portal Gomoku UI — Board Renderer
 *  Draws the board grid, pieces (X/O), walls (bricks), and portals (rings).
 *  Pure rendering utility — no widget state. Called from BoardCanvas::draw.
 *
 *  Visual style: Paper/notebook grid with X and O pieces.
 */

#pragma once

#include "../../model/Board.hpp"
#include "../../model/Cell.hpp"
#include "../../util/Coord.hpp"

#include <cairomm/context.h>
#include <optional>
#include <vector>

namespace ui::board {

/// Board geometry constants computed from the widget allocation.
/// Grid paradigm: N+1 lines create N cells. Pieces go inside cells.
struct BoardGeometry {
    double cellSize;        ///< Pixel size of one grid cell
    double originX;         ///< X pixel of the top-left grid line
    double originY;         ///< Y pixel of the top-left grid line
    int    boardSize;       ///< N for NxN

    /// Convert cell coordinates to pixel center (center of the cell).
    void cellToPixel(int x, int y, double& px, double& py) const {
        px = originX + (x + 0.5) * cellSize;
        py = originY + (y + 0.5) * cellSize;
    }

    /// Convert pixel position to cell index. Returns none if out of bounds.
    util::Coord pixelToCell(double px, double py) const {
        int x = static_cast<int>((px - originX) / cellSize);
        int y = static_cast<int>((py - originY) / cellSize);
        if (x < 0 || x >= boardSize || y < 0 || y >= boardSize)
            return util::Coord::none();
        return {x, y};
    }

    /// Compute geometry from widget dimensions and board size.
    static BoardGeometry compute(int widgetWidth, int widgetHeight, int boardSize);
};

/// Predefined portal pair colors (up to 8 pairs).
struct PortalColor {
    double r, g, b;
};

/// Pure rendering functions for the board.
/// All methods are static — no state, no widget dependency.
class BoardRenderer {
public:
    // =========================================================================
    // Full Board Draw
    // =========================================================================

    /// Draw the complete board: background, grid, coords, obstacles, pieces.
    static void drawBoard(const Cairo::RefPtr<Cairo::Context>& cr,
                          const BoardGeometry& geo,
                          const model::Board& board,
                          std::optional<util::Coord> lastMove = {},
                          std::optional<util::Coord> hoverCell = {});

    // =========================================================================
    // Individual Element Renderers
    // =========================================================================

    /// Draw the paper-style background and grid lines.
    static void drawBackground(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo);

    /// Draw coordinate labels (A-O top, 1-15 left).
    static void drawCoordinates(const Cairo::RefPtr<Cairo::Context>& cr,
                                const BoardGeometry& geo);

    /// Draw star points (hoshi) if applicable.
    static void drawStarPoints(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo);

    /// Draw last-move highlight (yellow background).
    static void drawLastMoveHighlight(const Cairo::RefPtr<Cairo::Context>& cr,
                                      const BoardGeometry& geo,
                                      util::Coord pos);

    /// Draw hover indicator (subtle highlight on the cell under the cursor).
    static void drawHoverHighlight(const Cairo::RefPtr<Cairo::Context>& cr,
                                   const BoardGeometry& geo,
                                   util::Coord pos);

    /// Draw a Black piece (bold X cross).
    static void drawBlackPiece(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo,
                               int x, int y);

    /// Draw a White piece (O circle outline).
    static void drawWhitePiece(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo,
                               int x, int y);

    /// Draw a wall cell (brick pattern).
    static void drawWall(const Cairo::RefPtr<Cairo::Context>& cr,
                         const BoardGeometry& geo,
                         int x, int y);

    /// Draw a portal endpoint (colored ring with center dot).
    /// @param pairIndex  Index into the portal color palette (0-based).
    static void drawPortal(const Cairo::RefPtr<Cairo::Context>& cr,
                           const BoardGeometry& geo,
                           int x, int y,
                           int pairIndex);

    /// Draw move number on a piece.
    static void drawMoveNumber(const Cairo::RefPtr<Cairo::Context>& cr,
                               const BoardGeometry& geo,
                               int x, int y,
                               int moveNum,
                               bool isBlack);

private:
    /// Portal pair color palette.
    static const PortalColor& portalColor(int pairIndex);
};

}  // namespace ui::board
