/*
 *  Portal Gomoku UI — EvalBar Implementation
 */

#include "EvalBar.hpp"
#include <algorithm>
#include <cmath>

namespace ui::widgets {

EvalBar::EvalBar() {
    set_content_width(20);
    set_content_height(400); // Usually a tall vertical bar
    set_draw_func(sigc::mem_fun(*this, &EvalBar::draw_content));
}

void EvalBar::setScore(int score) {
    if (score_ != score) {
        score_ = score;
        queue_draw();
    }
}

void EvalBar::draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    // Background (White's advantage area)
    cr->set_source_rgb(0.9, 0.9, 0.9); // Light gray for white
    cr->rectangle(0, 0, width, height);
    cr->fill();

    // Map score to a percentage (0.0 = all white, 0.5 = equal, 1.0 = all black)
    // Wrap it using a nonlinear function (Sigmoid or atan) so small advantages are visible
    // but it never completely fills until a forced mate (very high score).
    
    // Simple mapping for now: clamp between -5000 and 5000
    double clamped_score = std::clamp(score_, -5000, 5000);
    double pct_black = 0.5 + (clamped_score / 10000.0); 
    
    int fill_height = static_cast<int>(height * pct_black);

    // Black's advantage area (drawn from bottom up)
    cr->set_source_rgb(0.2, 0.2, 0.2); // Dark gray for black
    cr->rectangle(0, height - fill_height, width, fill_height);
    cr->fill();

    // Draw center line
    cr->set_source_rgba(1.0, 0.0, 0.0, 0.5); // Red translucent
    cr->move_to(0, height / 2.0);
    cr->line_to(width, height / 2.0);
    cr->stroke();
}

} // namespace ui::widgets
