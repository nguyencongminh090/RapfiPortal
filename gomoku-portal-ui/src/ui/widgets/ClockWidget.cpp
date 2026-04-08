/*
 *  Portal Gomoku UI — Clock Widget Implementation
 */

#include "ClockWidget.hpp"
#include <iomanip>
#include <sstream>

namespace ui::widgets {

ClockWidget::ClockWidget() {
    // Basic setup, can add CSS classes here later
    set_halign(Gtk::Align::CENTER);
    add_css_class("clock-widget"); // Hook for CSS styling
    setTimeMs(0);
}

void ClockWidget::setTimeMs(int ms) {
    timeMs_ = ms;

    int total_seconds = ms / 1000;
    int minutes = total_seconds / 60;
    int seconds = total_seconds % 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(2) << minutes << ":"
        << std::setfill('0') << std::setw(2) << seconds;
        
    // Optionally show tenths if time is running low, but for simplicity, we just show mm:ss here
    // oss << "." << deciseconds; 

    set_text(oss.str());
}

} // namespace ui::widgets
