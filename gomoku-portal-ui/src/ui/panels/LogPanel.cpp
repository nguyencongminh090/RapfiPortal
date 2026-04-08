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

    auto iter = textBuffer_->end();
    textBuffer_->insert(iter, msg + "\n");

    // Auto-scroll to bottom using a persistent mark (avoids leaking anonymous marks)
    auto endMark = textBuffer_->get_mark("scroll-end");
    if (!endMark) {
        endMark = textBuffer_->create_mark("scroll-end", textBuffer_->end());
    } else {
        textBuffer_->move_mark(endMark, textBuffer_->end());
    }
    textView_.scroll_to(endMark, 0.0);
}

void LogPanel::clearLogs() {
    if (textBuffer_) {
        textBuffer_->set_text("");
    }
}

} // namespace ui::panels
