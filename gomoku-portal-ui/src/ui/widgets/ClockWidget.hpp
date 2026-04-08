/*
 *  Portal Gomoku UI — Clock Widget
 *  A simple timer display for a player.
 */

#pragma once

#include <gtkmm/label.h>

namespace ui::widgets {

class ClockWidget : public Gtk::Label {
public:
    ClockWidget();

    /// Set time in milliseconds and update display
    void setTimeMs(int ms);

private:
    int timeMs_ = 0;
};

} // namespace ui::widgets
