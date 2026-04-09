/*
 *  Portal Gomoku UI — Analysis Panel Implementation
 */

#include "AnalysisPanel.hpp"


#include <iomanip>
#include <sstream>

namespace ui::panels {

AnalysisPanel::AnalysisPanel(controller::AnalysisController& analysisCtrl)
    : Gtk::Box(Gtk::Orientation::VERTICAL)
    , analysisCtrl_(analysisCtrl)
{
    set_margin(5);
    set_spacing(5);

    // 1. Summary Label
    lblSummary_.set_halign(Gtk::Align::START);
    lblSummary_.set_markup("<b>Depth:</b> 0  |  <b>Time:</b> 0.0s  |  <b>Nodes:</b> 0  |  <b>NPS:</b> 0");
    append(lblSummary_);

    // 2. TreeView setup
    listStore_ = Gtk::ListStore::create(columns_);
    treeView_.set_model(listStore_);
    
    // Setup Columns
    treeView_.append_column("Rank", columns_.colRank);
    treeView_.append_column("Move", columns_.colMove);
    treeView_.append_column("Score", columns_.colScore);
    treeView_.append_column("Depth", columns_.colDepth);
    treeView_.append_column("Principal Variation", columns_.colPV);

    // Make UI nicer
    treeView_.set_headers_visible(true);
    treeView_.set_vexpand(true);
    treeView_.set_hexpand(true);
    
    // We want highlighting when hovering or selecting
    treeView_.get_selection()->set_mode(Gtk::SelectionMode::SINGLE);
    treeView_.get_selection()->signal_changed().connect(
        sigc::mem_fun(*this, &AnalysisPanel::onSelectionChanged));

    scroll_.set_child(treeView_);
    scroll_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    scroll_.set_vexpand(true);
    append(scroll_);

    // Connect to controller
    analysisCtrl_.signalAnalysisUpdated.connect(
        sigc::mem_fun(*this, &AnalysisPanel::onAnalysisUpdated));
}

std::optional<model::AnalysisMove> AnalysisPanel::hoveredVariation() const {
    auto selection = treeView_.get_selection();
    if (auto iter = selection->get_selected()) {
        return (*iter)[columns_.colData];
    }
    return std::nullopt;
}

void AnalysisPanel::onSelectionChanged() {
    signalVariationHovered.emit();
}

void AnalysisPanel::onAnalysisUpdated() {
    const auto& info = analysisCtrl_.info();

    // Update Summary
    std::ostringstream ss;
    ss << "<b>Depth:</b> " << info.depth 
       << "  |  <b>Time:</b> " << std::fixed << std::setprecision(1) << (info.timeMs / 1000.0) << "s"
       << "  |  <b>Nodes:</b> " << info.nodes 
       << "  |  <b>NPS:</b> " << info.nps;
    lblSummary_.set_markup(ss.str());

    // Save selection explicitly if possible? For now, list drops it on clear.
    listStore_->clear();

    // Update TreeView
    for (const auto& mv : info.nBest) {
        if (mv.rank <= 0) continue; // Skip uninitialized
        
        auto row = *(listStore_->append());
        row[columns_.colRank] = mv.rank;
        
        // Convert Coord to string
        std::string moveStr = "-";
        if (mv.coord.isValid(boardSize_)) {  // BUG-006 FIX: use dynamic board size
            auto pos = mv.pvText.find(" ");
            moveStr = (pos != std::string::npos) ? mv.pvText.substr(0, pos) : mv.pvText;
        }
        
        row[columns_.colMove] = moveStr;
        row[columns_.colScore] = mv.score;
        row[columns_.colDepth] = mv.depth;
        row[columns_.colPV] = mv.pvText;
        row[columns_.colData] = mv;
    }
}

}  // namespace ui::panels
