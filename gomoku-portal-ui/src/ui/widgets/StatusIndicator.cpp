/*
 *  Portal Gomoku UI — Status Indicator Implementation
 */

#include "StatusIndicator.hpp"

namespace ui::widgets {

StatusIndicator::StatusIndicator() {
    set_content_width(12);
    set_content_height(12);
    set_draw_func(sigc::mem_fun(*this, &StatusIndicator::draw_content));
}

void StatusIndicator::setState(engine::EngineState state) {
    if (state_ != state) {
        state_ = state;
        queue_draw(); // Ask GTK to redraw
    }
}

void StatusIndicator::draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height) {
    double radius = std::min(width, height) / 2.0;
    double cx = width / 2.0;
    double cy = height / 2.0;

    cr->arc(cx, cy, radius, 0.0, 2.0 * M_PI);

    // Set color based on state
    switch (state_) {
        case engine::EngineState::Disconnected:
            cr->set_source_rgb(0.5, 0.5, 0.5); // Gray
            break;
        case engine::EngineState::Idle:
            cr->set_source_rgb(0.2, 0.8, 0.2); // Green
            break;
        case engine::EngineState::Thinking:
            cr->set_source_rgb(0.9, 0.6, 0.1); // Orange
            break;
        case engine::EngineState::Stopping:
            cr->set_source_rgb(0.8, 0.2, 0.2); // Red
            break;
    }
    
    cr->fill();
}

} // namespace ui::widgets
