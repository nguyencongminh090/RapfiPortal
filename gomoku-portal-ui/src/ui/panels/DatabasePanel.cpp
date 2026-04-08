/*
 *  Portal Gomoku UI — Database Panel Implementation
 */

#include "DatabasePanel.hpp"
#include <string>

namespace ui::panels {

DatabasePanel::DatabasePanel() : Gtk::Box(Gtk::Orientation::VERTICAL) {
    set_spacing(5);

    // Setup Toolbar/Buttons
    buttonBox_.set_spacing(5);
    buttonBox_.set_margin(5);
    buttonBox_.append(btnQueryAll_);
    buttonBox_.append(btnQueryOne_);
    buttonBox_.append(btnDeleteOne_);
    append(buttonBox_);

    // Setup ListStore and TreeView
    listStore_ = Gtk::ListStore::create(columns_);
    treeView_.set_model(listStore_);

    treeView_.append_column("Coord", columns_.colCoord);
    treeView_.append_column("Value", columns_.colValue);
    treeView_.append_column("Depth", columns_.colDepth);
    treeView_.append_column("Label", columns_.colLabel);
    treeView_.append_column("Raw Text", columns_.colRawText);

    // Make some columns resizable
    for (guint i = 0; i < treeView_.get_n_columns(); i++) {
        if (auto col = treeView_.get_column(i)) {
            col->set_resizable(true);
        }
    }

    // Setup ScrolledWindow
    scrolledWindow_.set_child(treeView_);
    scrolledWindow_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scrolledWindow_.set_expand(true);
    append(scrolledWindow_);
}

void DatabasePanel::setRecords(const std::vector<model::DatabaseRecord>& records) {
    clear();
    for (const auto& rec : records) {
        auto row = *(listStore_->append());
        
        std::string coordStr = std::to_string(rec.coord.x) + "," + std::to_string(rec.coord.y);
        if (rec.coord.x < 0 || rec.coord.y < 0) coordStr = "N/A"; // Handle missing coord
        
        row[columns_.colCoord] = coordStr;
        row[columns_.colValue] = rec.value;
        row[columns_.colDepth] = rec.depth;
        row[columns_.colLabel] = rec.label;
        row[columns_.colRawText] = rec.rawText;
    }
}

void DatabasePanel::clear() {
    if (listStore_) {
        listStore_->clear();
    }
}

} // namespace ui::panels
