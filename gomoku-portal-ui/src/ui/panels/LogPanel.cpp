/*
 *  Portal Gomoku UI — Log Panel Implementation
 */

#include "LogPanel.hpp"
#include <gtkmm/textiter.h>

namespace ui::panels {

LogPanel::LogPanel(bool dualColumn) : Gtk::Box(Gtk::Orientation::HORIZONTAL), dualColumn_(dualColumn) {
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

    if (dualColumn_) {
        prefixTextView_.set_editable(false);
        prefixTextView_.set_cursor_visible(false);
        prefixTextView_.set_wrap_mode(Gtk::WrapMode::NONE);
        prefixTextView_.set_monospace(true);
        prefixTextView_.add_css_class("dim-label");

        prefixTextBuffer_ = prefixTextView_.get_buffer();

        prefixScrolledWindow_.set_child(prefixTextView_);
        prefixScrolledWindow_.set_policy(Gtk::PolicyType::NEVER, Gtk::PolicyType::EXTERNAL);
        prefixScrolledWindow_.set_vadjustment(scrolledWindow_.get_vadjustment());
        
        prefixScrolledWindow_.set_size_request(85, -1);
        
        append(prefixScrolledWindow_);
    }

    append(scrolledWindow_);
}

void LogPanel::appendLog(const std::string& msg) {
    appendLog("", msg);
}

void LogPanel::appendLog(const std::string& prefix, const std::string& msg) {
    if (!textBuffer_) return;

    if (dualColumn_ && prefixTextBuffer_) {
        auto pIter = prefixTextBuffer_->end();
        prefixTextBuffer_->insert(pIter, prefix + "\n");
    }

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
    if (prefixTextBuffer_) {
        prefixTextBuffer_->set_text("");
    }
}

} // namespace ui::panels
