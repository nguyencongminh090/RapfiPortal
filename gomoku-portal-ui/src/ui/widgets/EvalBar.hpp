/*
 *  Portal Gomoku UI — EvalBar
 *  A progress-bar style widget to show engine evaluation visually.
 */

#pragma once

#include <gtkmm/drawingarea.h>

namespace ui::widgets {

class EvalBar : public Gtk::DrawingArea {
public:
    EvalBar();

    /// Set the evaluation score (-10000 to +10000)
    /// Positive = Black advantage, Negative = White advantage
    void setScore(int score);

protected:
    void draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

private:
    int score_ = 0;
};

} // namespace ui::widgets
