/*
 *  Portal Gomoku UI — Status Indicator
 *  A simple colored circle widget to indicate engine state.
 */

#pragma once

#include <gtkmm/drawingarea.h>
#include "../../engine/EngineState.hpp"

namespace ui::widgets {

class StatusIndicator : public Gtk::DrawingArea {
public:
    StatusIndicator();

    void setState(engine::EngineState state);

protected:
    void draw_content(const Cairo::RefPtr<Cairo::Context>& cr, int width, int height);

private:
    engine::EngineState state_ = engine::EngineState::Disconnected;
};

} // namespace ui::widgets
