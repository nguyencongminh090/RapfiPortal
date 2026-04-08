/*
 *  Portal Gomoku UI — Database Panel
 *  Displays database entries in a tabular view.
 */

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>

#include "../../model/DatabaseRecord.hpp"
#include <vector>

namespace ui::panels {

class DatabasePanel : public Gtk::Box {
public:
    DatabasePanel();

    /// Update the table with new records
    void setRecords(const std::vector<model::DatabaseRecord>& records);

    /// Clear the table
    void clear();

    // Setup signals for buttons here if needed, or expose them
    Gtk::Button& getQueryAllButton() { return btnQueryAll_; }
    Gtk::Button& getQueryOneButton() { return btnQueryOne_; }
    Gtk::Button& getDeleteOneButton() { return btnDeleteOne_; }

private:
    Gtk::ScrolledWindow scrolledWindow_;
    Gtk::TreeView treeView_;
    Gtk::Box buttonBox_{Gtk::Orientation::HORIZONTAL};

    Gtk::Button btnQueryAll_{"Query All"};
    Gtk::Button btnQueryOne_{"Query One"};
    Gtk::Button btnDeleteOne_{"Delete One"};

    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() {
            add(colCoord);
            add(colValue);
            add(colDepth);
            add(colLabel);
            add(colRawText);
        }

        Gtk::TreeModelColumn<Glib::ustring> colCoord;
        Gtk::TreeModelColumn<int>           colValue;
        Gtk::TreeModelColumn<int>           colDepth;
        Gtk::TreeModelColumn<Glib::ustring> colLabel;
        Gtk::TreeModelColumn<Glib::ustring> colRawText;
    };

    ModelColumns columns_;
    Glib::RefPtr<Gtk::ListStore> listStore_;
};

} // namespace ui::panels
