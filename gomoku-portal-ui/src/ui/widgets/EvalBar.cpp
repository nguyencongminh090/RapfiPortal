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
    cr->set_source_rgb(0.9, 0.9, 0.9);
    cr->rectangle(0, 0, width, height);
    cr->fill();

    // Winrate via sigmoid: W = 1 / (1 + exp(-score / 200))
    //   score    0 →  50%  (equal)
    //   score  100 →  62%  (slight advantage)
    //   score  200 →  73%  (clear advantage)
    //   score  500 →  92%  (winning)
    //   score 1000 →  99%+ (decisive)
    double winrate = 1.0 / (1.0 + std::exp(-static_cast<double>(score_) / 200.0));
    winrate = std::clamp(winrate, 0.02, 0.98); // Never fully empty/full

    int fill_height = static_cast<int>(height * winrate);

    // Black's advantage area (drawn from bottom up)
    cr->set_source_rgb(0.15, 0.15, 0.15);
    cr->rectangle(0, height - fill_height, width, fill_height);
    cr->fill();

    // Draw center line
    cr->set_source_rgba(0.4, 0.4, 0.4, 0.6);
    cr->set_line_width(1.0);
    cr->move_to(0, height / 2.0);
    cr->line_to(width, height / 2.0);
    cr->stroke();
}

} // namespace ui::widgets
