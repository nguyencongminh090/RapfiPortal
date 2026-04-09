/*
 *  Portal Gomoku UI — Main Window Implementation
 */

#include "MainWindow.hpp"
#include <gtkmm/separator.h>
#include <gtkmm/separator.h>
#include <gtkmm/settings.h>
#include <gdk/gdkkeysyms.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/eventcontrollerkey.h>
#include "../util/SettingsManager.hpp"
#include "../model/GameRecord.hpp"
#include <iostream>
#include <fstream>

namespace ui {

// =============================================================================
// Constructor / Destructor
// =============================================================================

MainWindow::MainWindow(controller::GameController& gameCtrl)
    : gameCtrl_(gameCtrl)
    , setupCtrl_(gameCtrl)
    , analysisCtrl_(gameCtrl)
    , boardCanvas_(gameCtrl.board())
    , dbPanel_()
    , analysisPanel_(analysisCtrl_)
{
    set_title("Portal Gomoku Engine UI");
    
    // Load settings
    auto& settings = util::SettingsManager::instance();
    settings.load();
    if (settings.windowWidth() > 0 && settings.windowHeight() > 0) {
        set_default_size(settings.windowWidth(), settings.windowHeight());
    } else {
        set_default_size(1000, 700);
    }
    
    if (settings.windowMaximized()) {
        maximize();
    }
    
    // Apply theme
    auto gtkSettings = Gtk::Settings::get_default();
    if (gtkSettings) {
        gtkSettings->property_gtk_application_prefer_dark_theme() = settings.preferDarkTheme();
    }

    setupLayout();
    setupToolbar();
    setupSignals();
    setupShortcuts();
    connections_.push_back(
        signal_close_request().connect([this]() -> bool {
            auto& settings = util::SettingsManager::instance();
            settings.setWindowWidth(get_width());
            settings.setWindowHeight(get_height());
            settings.setWindowMaximized(is_maximized());
            if (auto gtkSettings = Gtk::Settings::get_default()) {
                settings.setPreferDarkTheme(gtkSettings->property_gtk_application_prefer_dark_theme().get_value());
            }
            settings.setLastBoardSize(spinBoardSize_.get_value_as_int());
            settings.save();
            return false; // let the window close
        }, false));

    startPollingTimer();
}

MainWindow::~MainWindow() {
    if (pollTimerConnection_) {
        pollTimerConnection_.disconnect();
    }
    
    for (auto& conn : connections_) {
        conn.disconnect();
    }
}

// =============================================================================
// Layout Setup
// =============================================================================

void MainWindow::setupLayout() {
    // Main vertical box
    set_child(mainVBox_);

    // 1. Toolbar
    toolbarBox_.set_spacing(5);
    toolbarBox_.set_margin(5);
    mainVBox_.append(toolbarBox_);

    // Separator
    mainVBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Setup Toolbar (initially hidden)
    setupToolbarBox_.set_spacing(5);
    setupToolbarBox_.set_margin(5);
    
    // Group the radio buttons
    rbPortal_.set_group(rbWall_);
    rbEraser_.set_group(rbWall_);
    rbWall_.set_active(true);
    
    setupToolbarBox_.append(rbWall_);
    setupToolbarBox_.append(rbPortal_);
    setupToolbarBox_.append(rbEraser_);
    setupToolbarBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    setupToolbarBox_.append(btnClearTopology_);
    
    // Only display if setup is active
    setupToolbarBox_.set_visible(false);
    mainVBox_.append(setupToolbarBox_);

    // 2. Center area: eval bar + board + side panel
    centerPaned_.set_position(700);
    centerPaned_.set_shrink_start_child(false);
    centerPaned_.set_shrink_end_child(false);
    centerPaned_.set_resize_start_child(true);
    centerPaned_.set_resize_end_child(true);
    centerPaned_.set_hexpand(true);
    centerPaned_.set_vexpand(true);

    // Board area: EvalBar + Canvas
    boardArea_.set_spacing(0);
    evalBar_.set_size_request(22, -1);
    boardArea_.append(evalBar_);
    boardCanvas_.set_hexpand(true);
    boardCanvas_.set_vexpand(true);
    boardArea_.append(boardCanvas_);

    centerPaned_.set_start_child(boardArea_);

    // Side panel notebook
    sideNotebook_.set_size_request(280, -1);
    sideNotebook_.append_page(logPanel_, "Log");
    sideNotebook_.append_page(analysisPanel_, "Analysis");
    sideNotebook_.append_page(dbPanel_, "Database");
    sideNotebook_.append_page(engineSettingsPanel_, "Settings");
    
    // Set Log as default page
    sideNotebook_.set_current_page(0);

    centerPaned_.set_end_child(sideNotebook_);

    mainVBox_.append(centerPaned_);

    // 3. Status bar
    mainVBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));
    statusBar_.set_spacing(8);
    statusBar_.set_margin(4);
    statusIndicator_.set_size_request(12, 12);
    statusIndicator_.set_valign(Gtk::Align::CENTER);
    statusBar_.append(statusIndicator_);
    engineNameLabel_.set_halign(Gtk::Align::START);
    statusBar_.append(engineNameLabel_);
    mainVBox_.append(statusBar_);
}

// =============================================================================
// Toolbar Setup
// =============================================================================

void MainWindow::setupToolbar() {
    toolbarBox_.append(btnNewGame_);
    
    auto adj = Gtk::Adjustment::create(util::SettingsManager::instance().lastBoardSize(), 5.0, 22.0, 1.0, 1.0, 0.0);
    spinBoardSize_.set_adjustment(adj);
    spinBoardSize_.set_numeric(true);
    spinBoardSize_.set_wrap(false);
    spinBoardSize_.set_tooltip_text("Board Size");
    toolbarBox_.append(spinBoardSize_);

    comboMode_.append("Free Play");
    comboMode_.append("Engine as White");
    comboMode_.append("Engine as Black");
    comboMode_.set_active(0);
    toolbarBox_.append(comboMode_);
    toolbarBox_.append(btnSetupMode_);
    toolbarBox_.append(btnUndo_);
    toolbarBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    toolbarBox_.append(btnConnect_);
    toolbarBox_.append(btnThink_);
    toolbarBox_.append(btnStop_);
    toolbarBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    toolbarBox_.append(btnSaveGame_);
    toolbarBox_.append(btnLoadGame_);
    toolbarBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    toolbarBox_.append(btnThemeToggle_);

    btnStop_.set_sensitive(false);  // Disabled until engine is thinking
}

// =============================================================================
// Signal Wiring
// =============================================================================

void MainWindow::setupSignals() {
    // Toolbar buttons
    btnNewGame_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onNewGame));
    comboMode_.signal_changed().connect(sigc::mem_fun(*this, &MainWindow::onModeChanged));
    btnUndo_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onUndo));
    btnConnect_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onConnect));
    btnThink_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onThink));
    btnStop_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onStop));
    btnSaveGame_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onSaveGame));
    btnLoadGame_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onLoadGame));
    btnThemeToggle_.signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onThemeToggle));

    // Board canvas clicks → game controller or setup controller

    // Connect Analysis components
    analysisCtrl_.connectSignals();
    connections_.push_back(
        analysisCtrl_.signalAnalysisUpdated.connect([this]() {
            // Can update UI based on general analysis stats
            evalBar_.setScore(analysisCtrl_.info().score);
            
            // Pass the current nBest to the board for drawing scores
            boardCanvas_.setAnalysisInfo(analysisCtrl_.info());
            
            // Re-render board for PV overlays, etc. (handled in BoardCanvas later)
            boardCanvas_.refresh();
        }));
        
    connections_.push_back(
        analysisPanel_.signalVariationHovered.connect([this]() {
            boardCanvas_.setAnalysisHover(analysisPanel_.hoveredVariation());
        }));

    // Connect EngineSettingsPanel
    connections_.push_back(
        engineSettingsPanel_.signalTurnTimeChanged.connect([this](int ms) {
            gameCtrl_.setTurnTime(ms);
        }));
    connections_.push_back(
        engineSettingsPanel_.signalMatchTimeChanged.connect([this](int ms) {
            gameCtrl_.setMatchTime(ms);
        }));
    connections_.push_back(
        engineSettingsPanel_.signalMaxMemoryChanged.connect([this](int mb) {
            gameCtrl_.setMaxMemory(static_cast<int64_t>(mb) * 1024 * 1024);
        }));
    connections_.push_back(
        engineSettingsPanel_.signalNBestChanged.connect([this](int n) {
            gameCtrl_.setNBest(n);
        }));

    // Initialize GameController with current settings defaults
    auto& settings = util::SettingsManager::instance();
    gameCtrl_.setTurnTime(settings.engineTurnTime());
    gameCtrl_.setMatchTime(settings.engineMatchTime());
    gameCtrl_.setMaxMemory(static_cast<int64_t>(settings.engineMaxMemory()) * 1024 * 1024);
    gameCtrl_.setNBest(settings.engineNBest());

    connections_.push_back(
        boardCanvas_.signalCellClicked.connect([this](int x, int y) {
            if (setupCtrl_.isActive()) {
                setupCtrl_.onCellClicked(x, y);
            } else {
                gameCtrl_.onCellClicked(x, y);
            }
        }));

    connections_.push_back(
        boardCanvas_.signalScrollUp.connect([this]() {
            if (!setupCtrl_.isActive()) gameCtrl_.undoMove();
        }));

    connections_.push_back(
        boardCanvas_.signalScrollDown.connect([this]() {
            if (!setupCtrl_.isActive()) gameCtrl_.redoMove();
        }));

    // Setup mode signals
    btnSetupMode_.signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onSetupToggled));
    rbWall_.signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onSetupToolChanged));
    rbPortal_.signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onSetupToolChanged));
    rbEraser_.signal_toggled().connect(sigc::mem_fun(*this, &MainWindow::onSetupToolChanged));
    btnClearTopology_.signal_clicked().connect(sigc::mem_fun(*this, &MainWindow::onClearTopology));

    // Game controller signals → UI updates
    connections_.push_back(
        gameCtrl_.signalBoardChanged.connect(
            sigc::mem_fun(*this, &MainWindow::onBoardChanged)));
    connections_.push_back(
        gameCtrl_.signalEngineStateChanged.connect(
            sigc::mem_fun(*this, &MainWindow::onEngineStateChanged)));
    connections_.push_back(
        gameCtrl_.signalEngineMessage.connect(
            sigc::mem_fun(*this, &MainWindow::onEngineMessage)));
    connections_.push_back(
        gameCtrl_.signalEngineName.connect(
            sigc::mem_fun(*this, &MainWindow::onEngineName)));
}

// =============================================================================
// Engine Polling Timer
// =============================================================================

void MainWindow::startPollingTimer() {
    // Poll engine output every 50ms
    pollTimerConnection_ = Glib::signal_timeout().connect(
        sigc::mem_fun(*this, &MainWindow::onPollTimer), 50);
}

bool MainWindow::onPollTimer() {
    gameCtrl_.pollEngine();
    return true;  // Keep the timer running
}

// =============================================================================
// Toolbar Handlers
// =============================================================================

void MainWindow::onNewGame() {
    int requestedSize = spinBoardSize_.get_value_as_int();
    gameCtrl_.newGame(requestedSize);
    logPanel_.appendLog("--- New Game (" + std::to_string(requestedSize) + "x" + std::to_string(requestedSize) + ") ---");
}

void MainWindow::onModeChanged() {
    int active = comboMode_.get_active_row_number();
    if (active == 0) {
        gameCtrl_.setGameMode(controller::GameMode::FreePlay);
    } else if (active == 1) {
        gameCtrl_.setGameMode(controller::GameMode::HumanVsEngine, controller::HumanSide::Black);
    } else if (active == 2) {
        gameCtrl_.setGameMode(controller::GameMode::HumanVsEngine, controller::HumanSide::White);
    }
}

void MainWindow::onUndo() {
    gameCtrl_.undoMove();
}

void MainWindow::onConnect() {
    if (gameCtrl_.engine().state() != engine::EngineState::Disconnected) {
        // Already connected — disconnect
        gameCtrl_.disconnectEngine();
        btnConnect_.set_label("Connect");
        return;
    }

    // Open file dialog to select engine binary
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Engine Binary");

    dialog->open(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                std::string path = file->get_path();
                gameCtrl_.connectEngine(path);
                btnConnect_.set_label("Disconnect");
                logPanel_.appendLog("Connecting to: " + path);
            }
        } catch (const Glib::Error& err) {
            // User cancelled — ignore
            if (std::string(err.what()).find("dismissed") == std::string::npos) {
                logPanel_.appendLog("Error: " + std::string(err.what()));
            }
        }
    });
}

void MainWindow::onThink() {
    gameCtrl_.startThinking();
}

void MainWindow::onStop() {
    gameCtrl_.stopThinking();
}

void MainWindow::onSaveGame() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Save Game");

    dialog->save(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->save_finish(result);
            if (file) {
                std::string path = file->get_path();
                std::ofstream ofs(path);
                if (ofs) {
                    ofs << gameCtrl_.board().size() << "\n";
                    for (const auto& move : gameCtrl_.board().history()) {
                        if (move.isPass()) {
                            ofs << "-1,-1\n";
                        } else {
                            ofs << move.coord.x << "," << move.coord.y << "\n";
                        }
                    }
                    logPanel_.appendLog("Game saved to: " + path);
                }
            }
        } catch (const Glib::Error&) {}
    });
}

void MainWindow::onLoadGame() {
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Load Game");

    dialog->open(*this, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                std::string path = file->get_path();
                std::ifstream ifs(path);
                if (ifs) {
                    int size = 15;
                    ifs >> size;
                    spinBoardSize_.set_value(size);

                    std::vector<std::pair<int,int>> moves;
                    std::string line;
                    while (std::getline(ifs, line)) {
                        if (line.empty()) continue;
                        auto comma = line.find(',');
                        if (comma != std::string::npos) {
                            int x = std::stoi(line.substr(0, comma));
                            int y = std::stoi(line.substr(comma + 1));
                            moves.emplace_back(x, y);
                        }
                    }

                    // BUG-004 FIX: Use GameController method — no direct board mutation.
                    gameCtrl_.loadGameFromMoves(size, moves);
                    logPanel_.appendLog("Game loaded from: " + path);
                }
            }
        } catch (const Glib::Error&) {}
    });
}

void MainWindow::onThemeToggle() {
    auto settings = Gtk::Settings::get_default();
    if (!settings) return;
    
    bool preferDark = settings->property_gtk_application_prefer_dark_theme().get_value();
    settings->property_gtk_application_prefer_dark_theme() = !preferDark;
    
    btnThemeToggle_.set_label(preferDark ? "🌙" : "☀️");
}

void MainWindow::setupShortcuts() {
    auto keyCtrl = Gtk::EventControllerKey::create();
    keyCtrl->signal_key_pressed().connect(
        [this](guint keyval, guint /*keycode*/, Gdk::ModifierType state) -> bool {
            bool ctrl = static_cast<int>(state & Gdk::ModifierType::CONTROL_MASK) != 0;
            bool shift = static_cast<int>(state & Gdk::ModifierType::SHIFT_MASK) != 0;

            if (ctrl && !shift && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)) {
                onUndo(); return true;
            }
            if (ctrl && shift && (keyval == GDK_KEY_z || keyval == GDK_KEY_Z)) {
                gameCtrl_.redoMove(); return true; 
            }
            if (ctrl && !shift && (keyval == GDK_KEY_y || keyval == GDK_KEY_Y)) {
                gameCtrl_.redoMove(); return true;
            }
            if (ctrl && !shift && (keyval == GDK_KEY_n || keyval == GDK_KEY_N)) {
                onNewGame(); return true;
            }
            if (ctrl && !shift && (keyval == GDK_KEY_s || keyval == GDK_KEY_S)) {
                onSaveGame(); return true;
            }
            if (ctrl && !shift && (keyval == GDK_KEY_o || keyval == GDK_KEY_O)) {
                onLoadGame(); return true;
            }
            return false;
        }, false);
    add_controller(keyCtrl);
}

void MainWindow::onSetupToggled() {
    if (btnSetupMode_.get_active()) {
        if (!setupCtrl_.enterSetupMode()) {
            // Cannot enter setup mode right now (e.g., engine thinking)
            btnSetupMode_.set_active(false);
            logPanel_.appendLog("Cannot enter setup mode while the engine is thinking.");
            return;
        }
        setupToolbarBox_.set_visible(true);
        // Disable game actions
        comboMode_.set_sensitive(false);
        btnUndo_.set_sensitive(false);
        btnThink_.set_sensitive(false);
        
        logPanel_.appendLog("--- Entered Setup Mode ---");
    } else {
        setupCtrl_.exitSetupMode();
        setupToolbarBox_.set_visible(false);
        
        // Restore game actions if idle
        comboMode_.set_sensitive(true);
        bool idle = (gameCtrl_.engine().state() == engine::EngineState::Idle);
        btnThink_.set_sensitive(idle);
        btnUndo_.set_sensitive(true);
        
        logPanel_.appendLog("--- Exited Setup Mode ---");
    }
}

void MainWindow::onSetupToolChanged() {
    if (rbWall_.get_active()) {
        setupCtrl_.setTool(controller::SetupTool::Wall);
    } else if (rbPortal_.get_active()) {
        setupCtrl_.setTool(controller::SetupTool::PortalPair);
    } else if (rbEraser_.get_active()) {
        setupCtrl_.setTool(controller::SetupTool::Eraser);
    }
}

void MainWindow::onClearTopology() {
    setupCtrl_.clearAllTopology();
}

// =============================================================================
// GameController Signal Handlers
// =============================================================================

void MainWindow::onBoardChanged() {
    // BUG-006 FIX: propagate board size to analysis panel for correct coord validation.
    analysisPanel_.setBoardSize(gameCtrl_.board().size());

    if (setupCtrl_.isActive()) {
        board::HoverSetupInfo info;
        switch (setupCtrl_.tool()) {
            case controller::SetupTool::Wall: info.tool = board::HoverSetupInfo::Tool::Wall; break;
            case controller::SetupTool::PortalPair: info.tool = board::HoverSetupInfo::Tool::PortalPair; break;
            case controller::SetupTool::Eraser: info.tool = board::HoverSetupInfo::Tool::Eraser; break;
        }
        info.pendingPortalA = setupCtrl_.pendingPortalA();
        boardCanvas_.setSetupHover(info);
    } else {
        boardCanvas_.setSetupHover(std::nullopt);
    }
    boardCanvas_.refresh();
}

void MainWindow::onEngineStateChanged(engine::EngineState state) {
    statusIndicator_.setState(state);

    // Enable/disable buttons based on state
    bool idle = (state == engine::EngineState::Idle);
    bool thinking = (state == engine::EngineState::Thinking);
    bool disconnected = (state == engine::EngineState::Disconnected);

    if (!setupCtrl_.isActive()) {
        btnThink_.set_sensitive(idle);
        btnStop_.set_sensitive(thinking);
        btnNewGame_.set_sensitive(!thinking);
        btnUndo_.set_sensitive(!thinking);
        comboMode_.set_sensitive(!thinking);
    }

    if (disconnected) {
        btnConnect_.set_label("Connect");
        engineNameLabel_.set_text("No engine");
    }
}

void MainWindow::onEngineMessage(const std::string& msg) {
    logPanel_.appendLog(msg);
}

void MainWindow::onEngineName(const std::string& name) {
    engineNameLabel_.set_text(name);
}

}  // namespace ui
