/*
 *  Portal Gomoku UI — Analysis Panel
 *  Displays search telemetry and multi-PV candidate lines.
 */

#pragma once

#include "../../controller/AnalysisController.hpp"

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/treeview.h>
#include <gtkmm/liststore.h>
#include <sigc++/sigc++.h>
#include <optional>

namespace ui::panels {

/// GTK side-panel displaying live analysis info.
class AnalysisPanel : public Gtk::Box {
public:
    explicit AnalysisPanel(controller::AnalysisController& analysisCtrl);

    /// Get the PV currently hovered/selected by the user (if any).
    [[nodiscard]] std::optional<model::AnalysisMove> hoveredVariation() const;

    /// Fired when the hovered/selected variation changes (to update board)
    sigc::signal<void()> signalVariationHovered;

    /// Update the board size used for move coordinate validation.
    /// Call this whenever the board size changes (e.g. from MainWindow::onBoardChanged).
    void setBoardSize(int size) { boardSize_ = size; }

private:
    controller::AnalysisController& analysisCtrl_;
    int boardSize_ = 15;  // BUG-006 FIX: updated via setBoardSize(), not hardcoded

    // UI Elements
    Gtk::Label lblSummary_;
    Gtk::ScrolledWindow scroll_;
    Gtk::TreeView treeView_;

    // Tree Model Columns
    class ModelColumns : public Gtk::TreeModel::ColumnRecord {
    public:
        ModelColumns() {
            add(colRank);
            add(colMove);
            add(colScore);
            add(colDepth);
            add(colPV);
            add(colData); // Hidden, stores the actual struct
        }

        Gtk::TreeModelColumn<int> colRank;
        Gtk::TreeModelColumn<Glib::ustring> colMove;
        Gtk::TreeModelColumn<int> colScore;
        Gtk::TreeModelColumn<int> colDepth;
        Gtk::TreeModelColumn<Glib::ustring> colPV;
        Gtk::TreeModelColumn<model::AnalysisMove> colData;
    };

    ModelColumns columns_;
    Glib::RefPtr<Gtk::ListStore> listStore_;

    // Handlers
    void onAnalysisUpdated();
    void onSelectionChanged();
};

}  // namespace ui::panels
