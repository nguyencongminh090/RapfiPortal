/*
 *  Portal Gomoku UI — Board Canvas
 *  GTK4 DrawingArea widget that renders the board and handles mouse input.
 *  Delegates all rendering to BoardRenderer.
 */

#pragma once

#include "BoardRenderer.hpp"
#include "../../model/Board.hpp"

#include <gtkmm/drawingarea.h>
#include <gtkmm/gestureclick.h>
#include <gtkmm/eventcontrollermotion.h>
#include <sigc++/sigc++.h>
#include <optional>

namespace ui::board {

/// GTK4 DrawingArea that renders the board and captures user clicks.
///
/// Signals:
///   signalCellClicked(x, y)  — left-click on a board cell
///   signalCellRightClicked(x, y) — right-click on a board cell
///
/// The canvas does NOT modify the board — it only reads and renders it.
/// All mutations go through controller signals.
class BoardCanvas : public Gtk::DrawingArea {
public:
    explicit BoardCanvas(model::Board& board);

    /// Force a full redraw.
    void refresh();

    /// Enable/disable move number display.
    void setShowMoveNumbers(bool show);

    /// Get current board geometry (for external coord calculations).
    [[nodiscard]] const BoardGeometry& geometry() const { return geo_; }

    // =========================================================================
    // Signals
    // =========================================================================

    /// Left-click on a valid board cell.
    sigc::signal<void(int, int)> signalCellClicked;

    /// Right-click on a valid board cell.
    sigc::signal<void(int, int)> signalCellRightClicked;

private:
    model::Board& board_;
    BoardGeometry geo_{};
    std::optional<util::Coord> hoverCell_;
    bool showMoveNumbers_ = false;

    // GTK4 draw callback
    void draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

    // Input handlers
    void on_click(int n_press, double x, double y);
    void on_right_click(int n_press, double x, double y);
    void on_motion(double x, double y);
    void on_leave();
};

}  // namespace ui::board
