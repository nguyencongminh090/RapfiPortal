/*
 *  Portal Gomoku UI — EvalBar
 *  A progress-bar style widget to show engine evaluation visually.
 */

#pragma once

#include <gtkmm/drawingarea.h>
#include "../../model/Cell.hpp"

namespace ui::widgets {

class EvalBar : public Gtk::DrawingArea {
public:
    EvalBar();

    /// Set the evaluation score (centipawns).
    /// @param score       Raw score from engine (usually relative to sideToMove)
    /// @param sideToMove  The side whose turn it is
    void setScore(int score, model::Color sideToMove);

protected:
    void draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

private:
    int score_ = 0;
    model::Color sideToMove_ = model::Color::Black;
};

} // namespace ui::widgets
