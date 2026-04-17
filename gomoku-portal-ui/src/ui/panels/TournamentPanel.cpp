#include "TournamentPanel.hpp"
#include <gtkmm/filedialog.h>
#include <gtkmm/window.h>
#include <gtkmm/adjustment.h>
#include <giomm/file.h>

namespace ui::panels {

TournamentPanel::TournamentPanel(controller::TournamentController& tournCtrl)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 8)
    , tournCtrl_(tournCtrl)
{
    setupLayout();
    setupSignals();
}

void TournamentPanel::setupLayout() {
    set_margin(10);
    
    lblTitle_.set_use_markup(true);
    append(lblTitle_);
    append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));
    
    // Engine A
    lblEngineA_.set_wrap(true);
    append(btnSelectEngineA_);
    append(lblEngineA_);
    
    // Engine B
    lblEngineB_.set_wrap(true);
    append(btnSelectEngineB_);
    append(lblEngineB_);
    
    // OBF
    lblOBF_.set_wrap(true);
    append(btnSelectOBF_);
    append(lblOBF_);
    
    append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));
    
    // Settings
    auto adjMatch = Gtk::Adjustment::create(0, 0, 86400000, 1000, 1000, 0);
    spinMatchTime_.set_adjustment(adjMatch);
    spinMatchTime_.set_numeric(true);
    append(lblMatchTime_);
    append(spinMatchTime_);
    
    auto adjTurn = Gtk::Adjustment::create(5000, 100, 3600000, 100, 1000, 0);
    spinTurnTime_.set_adjustment(adjTurn);
    spinTurnTime_.set_numeric(true);
    append(lblTurnTime_);
    append(spinTurnTime_);
    
    auto adjGames = Gtk::Adjustment::create(2, 1, 1000, 1, 10, 0);
    spinGamesPerOpening_.set_adjustment(adjGames);
    spinGamesPerOpening_.set_numeric(true);
    append(lblGames_);
    append(spinGamesPerOpening_);
    
    auto adjThreads = Gtk::Adjustment::create(1, 0, 128, 1, 4, 0);
    spinThreads_.set_adjustment(adjThreads);
    spinThreads_.set_numeric(true);
    append(lblThreads_);
    append(spinThreads_);
    
    auto adjMemory = Gtk::Adjustment::create(350, 64, 32768, 64, 1024, 0);
    spinMemory_.set_adjustment(adjMemory);
    spinMemory_.set_numeric(true);
    append(lblMemory_);
    append(spinMemory_);
    
    append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));
    
    lblScore_.set_use_markup(true);
    append(lblScore_);
    append(lblProgress_);
    
    append(btnStart_);
    append(btnStop_);
    btnStop_.set_sensitive(false);
}

void TournamentPanel::setupSignals() {
    btnSelectEngineA_.signal_clicked().connect(sigc::mem_fun(*this, &TournamentPanel::onSelectEngineA));
    btnSelectEngineB_.signal_clicked().connect(sigc::mem_fun(*this, &TournamentPanel::onSelectEngineB));
    btnSelectOBF_.signal_clicked().connect(sigc::mem_fun(*this, &TournamentPanel::onSelectOBF));
    
    btnStart_.signal_clicked().connect(sigc::mem_fun(*this, &TournamentPanel::onStart));
    btnStop_.signal_clicked().connect(sigc::mem_fun(*this, &TournamentPanel::onStop));
    
    tournCtrl_.signalStateChanged.connect(sigc::mem_fun(*this, &TournamentPanel::onStateChanged));
    tournCtrl_.signalScoreChanged.connect(sigc::mem_fun(*this, &TournamentPanel::onScoreChanged));
}

void TournamentPanel::onSelectEngineA() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Engine A");
    dialog->open(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                pathEngineA_ = file->get_path();
                lblEngineA_.set_text(file->get_basename());
            }
        } catch (const Glib::Error&) {}
    });
}

void TournamentPanel::onSelectEngineB() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Engine B");
    dialog->open(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                pathEngineB_ = file->get_path();
                lblEngineB_.set_text(file->get_basename());
            }
        } catch (const Glib::Error&) {}
    });
}

void TournamentPanel::onSelectOBF() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Opening Binary File");
    
    // Filtering requires Gio::ListStore which has different template signatures across giomm versions.
    // Omitted for maximum compatibility.


    dialog->open(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                pathOBF_ = file->get_path();
                lblOBF_.set_text(file->get_basename());
            }
        } catch (const Glib::Error&) {}
    });
}

void TournamentPanel::onStart() {
    if (pathEngineA_.empty() || pathEngineB_.empty() || pathOBF_.empty()) {
        lblProgress_.set_text("Please select all files!");
        return;
    }
    
    controller::TournamentConfig cfg;
    cfg.engineA_Path = pathEngineA_;
    cfg.engineB_Path = pathEngineB_;
    cfg.obfPath = pathOBF_;
    cfg.matchTimeMs = spinMatchTime_.get_value_as_int();
    cfg.turnTimeMs = spinTurnTime_.get_value_as_int();
    cfg.gamesPerOpening = spinGamesPerOpening_.get_value_as_int();
    cfg.threads = spinThreads_.get_value_as_int();
    cfg.maxMemory = static_cast<int64_t>(spinMemory_.get_value_as_int()) * 1024 * 1024;
    
    tournCtrl_.startTournament(cfg);
}

void TournamentPanel::onStop() {
    tournCtrl_.stopTournament();
}

void TournamentPanel::onStateChanged() {
    auto state = tournCtrl_.state();
    bool playing = (state == controller::TournamentState::Playing || state == controller::TournamentState::Initializing);
    
    btnSelectEngineA_.set_sensitive(!playing);
    btnSelectEngineB_.set_sensitive(!playing);
    btnSelectOBF_.set_sensitive(!playing);
    btnStart_.set_sensitive(!playing);
    btnStop_.set_sensitive(playing);
    
    if (state == controller::TournamentState::Idle)
        lblProgress_.set_text("Status: Idle");
    else if (state == controller::TournamentState::Initializing)
        lblProgress_.set_text("Status: Connecting to Engines...");
    else if (state == controller::TournamentState::Finished)
        lblProgress_.set_text("Status: Tournament Finished");
}

void TournamentPanel::onScoreChanged() {
    int gTotal = tournCtrl_.totalGames();
    int gPlayed = tournCtrl_.gamesPlayed();
    
    lblProgress_.set_text("Status: Played " + std::to_string(gPlayed) + " / " + std::to_string(gTotal));
    
    std::string scoreStr = "<big><b>A: " + std::to_string(tournCtrl_.scoreEngineA()) + 
                           " | B: " + std::to_string(tournCtrl_.scoreEngineB()) + 
                           " | Draws: " + std::to_string(tournCtrl_.scoreDraws()) + "</b></big>";
    lblScore_.set_use_markup(true);
    lblScore_.set_markup(scoreStr);
}

} // namespace ui::panels
