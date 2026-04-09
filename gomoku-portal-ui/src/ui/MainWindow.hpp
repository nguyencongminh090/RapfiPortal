/*
 *  Portal Gomoku UI — Main Window
 *  Assembles all UI panels into the application layout.
 *
 *  Layout:
 *    ┌──────────────────────────────────────────┐
 *    │  Toolbar (New, Undo, Connect, Think)     │
 *    ├────┬──────────────────────┬──────────────┤
 *    │Eval│                      │  Side Panel  │
 *    │Bar │   BoardCanvas        │  (Notebook)  │
 *    │    │                      │  - Log       │
 *    │    │                      │  - Database  │
 *    ├────┴──────────────────────┴──────────────┤
 *    │  Status Bar [indicator] [engine name]    │
 *    └──────────────────────────────────────────┘
 */

#pragma once

#include "../controller/GameController.hpp"
#include "../controller/SetupController.hpp"
#include "../controller/AnalysisController.hpp"
#include "../ui/board/BoardCanvas.hpp"
#include "../ui/widgets/EvalBar.hpp"
#include "../ui/widgets/StatusIndicator.hpp"
#include "../ui/widgets/ClockWidget.hpp"
#include "../ui/panels/LogPanel.hpp"
#include "../ui/panels/DatabasePanel.hpp"
#include "../ui/panels/AnalysisPanel.hpp"
#include "../ui/panels/EngineSettingsPanel.hpp"

#include <gtkmm/applicationwindow.h>
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/headerbar.h>
#include <gtkmm/label.h>
#include <gtkmm/notebook.h>
#include <gtkmm/paned.h>
#include <gtkmm/separator.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/togglebutton.h>
#include <glibmm/main.h>

namespace ui {

class MainWindow : public Gtk::ApplicationWindow {
public:
    explicit MainWindow(controller::GameController& gameCtrl);
    ~MainWindow() override;

private:
    controller::GameController& gameCtrl_;

    // =========================================================================
    // Layout Containers
    // =========================================================================
    Gtk::Box       mainVBox_{Gtk::Orientation::VERTICAL};
    Gtk::Box       toolbarBox_{Gtk::Orientation::HORIZONTAL};
    Gtk::Box       setupToolbarBox_{Gtk::Orientation::HORIZONTAL};
    Gtk::Paned     mainPaned_{Gtk::Orientation::HORIZONTAL};
    Gtk::Paned     centerPaned_{Gtk::Orientation::HORIZONTAL};
    Gtk::Box       boardArea_{Gtk::Orientation::HORIZONTAL};
    Gtk::Box       statusBar_{Gtk::Orientation::HORIZONTAL};

    // =========================================================================
    // Toolbar Buttons & Controls
    // =========================================================================
    Gtk::Button btnNewGame_{"New Game"};
    Gtk::SpinButton spinBoardSize_;
    Gtk::ComboBoxText comboMode_;
    Gtk::ToggleButton btnSetupMode_{"Setup Mode"};
    Gtk::Button btnUndo_{"Undo"};
    Gtk::Button btnConnect_{"Connect"};
    Gtk::Button btnThink_{"Think"};
    Gtk::Button btnStop_{"Stop"};
    Gtk::Button btnSaveGame_{"Save"};
    Gtk::Button btnLoadGame_{"Load"};
    Gtk::ToggleButton btnThemeToggle_{"🌙"};

    // =========================================================================
    // Setup Controls
    // =========================================================================
    Gtk::CheckButton rbWall_{"Wall"};
    Gtk::CheckButton rbPortal_{"Portal Pair"};
    Gtk::CheckButton rbEraser_{"Eraser"};
    Gtk::Button btnClearTopology_{"Clear All Topology"};

    controller::SetupController setupCtrl_;
    controller::AnalysisController analysisCtrl_;

    // =========================================================================
    // Board & Widgets
    // =========================================================================
    board::BoardCanvas   boardCanvas_;
    widgets::EvalBar     evalBar_;
    widgets::ClockWidget clockWidget_;
    widgets::StatusIndicator statusIndicator_;
    Gtk::Label           engineNameLabel_{"No engine"};

    // =========================================================================
    // Side Panel
    // =========================================================================
    Gtk::Notebook       sideNotebook_;
    panels::LogPanel    logPanel_;
    panels::DatabasePanel dbPanel_;
    panels::AnalysisPanel analysisPanel_;
    panels::EngineSettingsPanel engineSettingsPanel_;
    panels::LogPanel protocolPanel_;

    // =========================================================================
    // Timer for engine polling
    // =========================================================================
    sigc::connection pollTimerConnection_;

    // =========================================================================
    // Signal connections
    // =========================================================================
    std::vector<sigc::connection> connections_;

    // =========================================================================
    // Setup
    // =========================================================================
    void setupLayout();
    void setupToolbar();
    void setupSignals();
    void setupShortcuts();
    void startPollingTimer();

    // =========================================================================
    // Handlers
    // =========================================================================
    void onNewGame();
    void onModeChanged();
    void onUndo();
    void onConnect();
    void onThink();
    void onStop();
    
    void onSaveGame();
    void onLoadGame();
    void onThemeToggle();
    
    void onSetupToggled();
    void onSetupToolChanged();
    void onClearTopology();
    
    bool onPollTimer();

    // GameController signal handlers
    void onBoardChanged();
    void onEngineStateChanged(engine::EngineState state);
    void onEngineMessage(const std::string& msg);
    void onEngineName(const std::string& name);
    void onRawComm(bool isSend, const std::string& msg);
};

}  // namespace ui
