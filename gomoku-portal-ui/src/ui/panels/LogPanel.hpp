/*
 *  Portal Gomoku UI — Log Panel
 *  A simple scrolling text view for engine messages and raw output.
 */

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <string>

namespace ui::panels {

class LogPanel : public Gtk::Box {
public:
    LogPanel();

    /// Append a new log message
    void appendLog(const std::string& msg);

    /// Clear all logs
    void clearLogs();

private:
    Gtk::ScrolledWindow scrolledWindow_;
    Gtk::TextView textView_;
    Glib::RefPtr<Gtk::TextBuffer> textBuffer_;
};

} // namespace ui::panels
