/*
 *  Portal Gomoku UI — Tournament Panel
 *  UI for configuring and running a match between two engines.
 */

#pragma once

#include "../../controller/TournamentController.hpp"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/separator.h>

namespace ui::panels {

class TournamentPanel : public Gtk::Box {
public:
    explicit TournamentPanel(controller::TournamentController& tournCtrl);
    ~TournamentPanel() override = default;

private:
    controller::TournamentController& tournCtrl_;

    Gtk::Label  lblTitle_{"<b>Engine Tournament</b>"};
    
    Gtk::Button btnSelectEngineA_{"Select Engine A..."};
    Gtk::Label  lblEngineA_{"No engine selected", Gtk::Align::START};
    
    Gtk::Button btnSelectEngineB_{"Select Engine B..."};
    Gtk::Label  lblEngineB_{"No engine selected", Gtk::Align::START};
    
    Gtk::Button btnSelectOBF_{"Select Opening File (.obf)..."};
    Gtk::Label  lblOBF_{"No OBF selected", Gtk::Align::START};

    Gtk::Label  lblMatchTime_{"Match Time (ms):"};
    Gtk::SpinButton spinMatchTime_;
    
    Gtk::Label  lblTurnTime_{"Turn Time (ms):"};
    Gtk::SpinButton spinTurnTime_;
    
    Gtk::Label  lblGames_{"Games Per Opening:"};
    Gtk::SpinButton spinGamesPerOpening_;

    Gtk::Label  lblThreads_{"Threads (0=Auto):"};
    Gtk::SpinButton spinThreads_;

    Gtk::Label  lblMemory_{"Max Memory (MB):"};
    Gtk::SpinButton spinMemory_;

    Gtk::Button btnStart_{"Start Tournament"};
    Gtk::Button btnStop_{"Stop Tournament"};

    Gtk::Label  lblProgress_{"Status: Idle"};
    Gtk::Label  lblScore_{"<big><b>Engine A: 0 | Engine B: 0 | Draws: 0</b></big>"};

    std::string pathEngineA_;
    std::string pathEngineB_;
    std::string pathOBF_;

    void setupLayout();
    void setupSignals();

    void onSelectEngineA();
    void onSelectEngineB();
    void onSelectOBF();
    void onStart();
    void onStop();

    void onStateChanged();
    void onScoreChanged();
};

} // namespace ui::panels
