/*
 *  Portal Gomoku UI — Log Panel
 *  A simple scrolling text view for engine messages and raw output.
 */

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/textview.h>
#include <gtkmm/expander.h>
#include <gtkmm/button.h>
#include <glibmm/dispatcher.h>
#include <string>
#include <mutex>
#include <vector>
#include <utility>

namespace ui::panels {

class LogPanel : public Gtk::Box {
public:
    explicit LogPanel(bool dualColumn = false);
    ~LogPanel() override = default;

    /// Append a new log message (Thread-safe)
    void appendLog(const std::string& msg);
    
    /// Append a new log message with a prefix (Thread-safe)
    void appendLog(const std::string& prefix, const std::string& msg);

    /// Clear all logs
    void clearLogs();

private:
    bool dualColumn_;
    
    Gtk::Expander expander_;
    Gtk::Box headerBox_{Gtk::Orientation::HORIZONTAL};
    Gtk::Button btnClear_{"Clear Logs"};
    Gtk::Box contentBox_{Gtk::Orientation::HORIZONTAL};

    Gtk::ScrolledWindow prefixScrolledWindow_;
    Gtk::ScrolledWindow scrolledWindow_;
    Gtk::TextView prefixTextView_;
    Gtk::TextView textView_;
    Glib::RefPtr<Gtk::TextBuffer> prefixTextBuffer_;
    Glib::RefPtr<Gtk::TextBuffer> textBuffer_;

    // Thread-safe dispatching
    std::mutex queueMutex_;
    std::vector<std::pair<std::string, std::string>> pendingLogs_;
    Glib::Dispatcher dispatcher_;

    void onDispatcherEmit();
};

} // namespace ui::panels
