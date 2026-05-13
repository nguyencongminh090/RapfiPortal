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

void EvalBar::setScore(int score, model::Color sideToMove) {
    if (score_ != score || sideToMove_ != sideToMove) {
        score_ = score;
        sideToMove_ = sideToMove;
        queue_draw();
    }
}

void EvalBar::draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    // Background (White's advantage area)
    cr->set_source_rgb(0.9, 0.9, 0.9);
    cr->rectangle(0, 0, width, height);
    cr->fill();

    // Winrate via sigmoid: W = 1 / (1 + exp(-score / 200))
    // We calculate the winrate of the side to move first, then convert to Black's winrate.
    double rawWinrate = 1.0 / (1.0 + std::exp(-static_cast<double>(score_) / 200.0));
    
    double blackWinrate = (sideToMove_ == model::Color::Black) ? rawWinrate : (1.0 - rawWinrate);
    
    blackWinrate = std::clamp(blackWinrate, 0.02, 0.98); // Never fully empty/full
    int fill_height = static_cast<int>(height * blackWinrate);

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

    // Draw winrate percentage text
    cr->select_font_face("Sans", Cairo::ToyFontFace::Slant::NORMAL, Cairo::ToyFontFace::Weight::BOLD);
    cr->set_font_size(11);
    
    char text[16];
    if (blackWinrate > 0.5) {
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(std::round(blackWinrate * 100)));
        cr->set_source_rgb(0.9, 0.9, 0.9); // White text on black background
    } else {
        std::snprintf(text, sizeof(text), "%d%%", static_cast<int>(std::round((1.0 - blackWinrate) * 100)));
        cr->set_source_rgb(0.1, 0.1, 0.1); // Black text on white background
    }
    
    Cairo::TextExtents extents;
    cr->get_text_extents(text, extents);
    
    double textX = (width - extents.width) / 2.0 - extents.x_bearing;
    double textY;
    
    if (blackWinrate >= 0.5) {
        // Black is winning, draw near bottom
        textY = height - 10.0; 
    } else {
        // White is winning, draw near top
        textY = 10.0 + extents.height;
    }
    
    cr->move_to(textX, textY);
    cr->show_text(text);
}

} // namespace ui::widgets
