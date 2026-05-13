/*
 *  Portal Gomoku UI — SPSA Tuning Panel
 *  GTK4 UI for configuring and running SPSA parameter optimization.
 *
 *  Features:
 *    - Engine and baseline engine selection
 *    - SPSA hyperparameter configuration (a, c, A)
 *    - Validation match interval config (θ vs baseline)
 *    - Live parameter value display with initial → current tracking
 *    - Validation results log with Elo estimation
 *    - Stop / Continue support (persists to disk)
 */

#pragma once

#include "../../controller/SPSAController.hpp"

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/progressbar.h>
#include <gtkmm/scrolledwindow.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/textview.h>
#include <gtkmm/togglebutton.h>

namespace ui::panels {

class SPSAPanel : public Gtk::Box {
public:
    explicit SPSAPanel(controller::SPSAController& spsaCtrl);
    ~SPSAPanel() override = default;

    void poll() { spsaCtrl_.pollEngines(); }

private:
    controller::SPSAController& spsaCtrl_;

    Gtk::ScrolledWindow mainScroll_;
    Gtk::Box            mainBox_{Gtk::Orientation::VERTICAL, 6};

    Gtk::Label  lblTitle_{"<b>SPSA Parameter Tuner</b>"};

    // Engine selection
    Gtk::Button btnSelectEngine_{"Select Tuning Engine..."};
    Gtk::Label  lblEngine_{"No engine selected", Gtk::Align::START};

    // Baseline engine selection
    Gtk::Button btnSelectBaseline_{"Select Baseline Engine..."};
    Gtk::Label  lblBaseline_{"No baseline selected", Gtk::Align::START};

    // OBF
    Gtk::Button btnSelectOBF_{"Select Opening File (.obf)..."};
    Gtk::Label  lblOBF_{"No OBF selected", Gtk::Align::START};

    // Params config
    Gtk::Button btnSelectParamsConfig_{"Select Params Config (.csv)..."};
    Gtk::Label  lblParamsConfig_{"No Params config selected", Gtk::Align::START};

    // SPSA hyperparameters
    Gtk::Label      lblStab_{"A (stabilization):"};
    Gtk::SpinButton spinStab_;
    Gtk::Label      lblGames_{"Games / Iteration:"};
    Gtk::SpinButton spinGames_;
    Gtk::Label      lblTurnTime_{"Turn Time (ms):"};
    Gtk::SpinButton spinTurnTime_;
    Gtk::Label      lblThreads_{"Threads:"};
    Gtk::SpinButton spinThreads_;
    Gtk::Label      lblMemory_{"Memory (MB):"};
    Gtk::SpinButton spinMemory_;
    Gtk::Label      lblMatchTime_{"Match Time (ms, 0=off):"};
    Gtk::SpinButton spinMatchTime_;
    Gtk::Label      lblConcurrency_{"Concurrent Games:"};
    Gtk::SpinButton spinConcurrency_;

    // Validation settings
    Gtk::Label      lblValInterval_{"Validate every N iters:"};
    Gtk::SpinButton spinValInterval_;
    Gtk::Label      lblValGames_{"Validation games:"};
    Gtk::SpinButton spinValGames_;

    // State file
    Gtk::Button btnSelectState_{"State File..."};
    Gtk::Label  lblState_{"data/spsa/spsa_state.json", Gtk::Align::START};

    // Controls
    Gtk::Button btnStart_{"▶ Start / Resume"};
    Gtk::Button btnStop_{"■ Stop"};

    // Status
    Gtk::Label  lblStatus_{"Status: Idle"};
    Gtk::Label  lblIteration_{"Iteration: 0"};
    Gtk::Label  lblProgress_{"Games: 0/0  Slots: 0 active"};
    Gtk::ProgressBar progressBar_;
    Gtk::ToggleButton btnSpectate_{"Spectate"};

    // Parameter display
    Gtk::Label  lblParamTitle_{"<b>Current Parameters</b>"};
    Gtk::Label  lblParams_{""};

    // Validation results display
    Gtk::Label  lblValTitle_{"<b>Validation Results (θ vs Baseline)</b>"};
    Gtk::Label  lblValResults_{""};

    // Log
    Gtk::Label          lblLogTitle_{"<b>Tuning Log</b>"};
    Gtk::ScrolledWindow logScroll_;
    Gtk::TextView       logView_;

    std::string pathEngine_;
    std::string pathBaseline_;
    std::string pathOBF_;
    std::string pathState_ = "data/spsa/spsa_state.json";
    std::string pathParamsConfig_;

    void setupLayout();
    void setupSignals();

    void onSelectEngine();
    void onSelectBaseline();
    void onSelectOBF();
    void onSelectState();
    void onSelectParamsConfig();
    void onStart();
    void onStop();

    void onStateChanged();
    void onLogMessage(const std::string& msg);
    void onIterationComplete();
    void onParamsUpdated();
    void onValidationComplete();
    void onProgressUpdated();

    void refreshParamDisplay();
    void refreshValidationDisplay();
};

} // namespace ui::panels
