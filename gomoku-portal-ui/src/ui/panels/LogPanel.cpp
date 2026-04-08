/*
 *  Portal Gomoku UI — Log Panel Implementation
 */

#include "LogPanel.hpp"
#include <gtkmm/textiter.h>

namespace ui::panels {

LogPanel::LogPanel() : Gtk::Box(Gtk::Orientation::VERTICAL) {
    // Setup TextView
    textView_.set_editable(false);
    textView_.set_cursor_visible(false);
    textView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    textView_.set_monospace(true); // Better for logs

    textBuffer_ = textView_.get_buffer();

    // Setup ScrolledWindow
    scrolledWindow_.set_child(textView_);
    scrolledWindow_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledWindow_.set_expand(true);

    append(scrolledWindow_);
}

void LogPanel::appendLog(const std::string& msg) {
    if (!textBuffer_) return;

    Gtk::TextBuffer::iterator iter = textBuffer_->end();
    textBuffer_->insert(iter, msg + "\n");

    // Auto-scroll to bottom
    Gtk::TextBuffer::iterator end_iter = textBuffer_->end();
    textView_.scroll_to(textBuffer_->create_mark(end_iter), 0.0);
}

void LogPanel::clearLogs() {
    if (textBuffer_) {
        textBuffer_->set_text("");
    }
}

} // namespace ui::panels
