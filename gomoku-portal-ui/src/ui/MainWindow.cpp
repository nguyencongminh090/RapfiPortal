/*
 *  Portal Gomoku UI — Main Window Implementation
 */

#include "MainWindow.hpp"
#include <gtkmm/filedialog.h>
#include <iostream>

namespace ui {

// =============================================================================
// Constructor / Destructor
// =============================================================================

MainWindow::MainWindow(controller::GameController& gameCtrl)
    : gameCtrl_(gameCtrl)
    , boardCanvas_(gameCtrl.board())
{
    set_title("Portal Gomoku — MINT-P Analysis Tool");
    set_default_size(1100, 750);

    setupLayout();
    setupToolbar();
    setupSignals();
    startPollingTimer();
}

MainWindow::~MainWindow() {
    pollTimerConnection_.disconnect();
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
    sideNotebook_.append_page(databasePanel_, "Database");

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

    comboMode_.append("Free Play");
    comboMode_.append("Engine as White");
    comboMode_.append("Engine as Black");
    comboMode_.set_active(0);
    toolbarBox_.append(comboMode_);

    toolbarBox_.append(btnUndo_);
    toolbarBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::VERTICAL));
    toolbarBox_.append(btnConnect_);
    toolbarBox_.append(btnThink_);
    toolbarBox_.append(btnStop_);

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

    // Board canvas clicks → game controller
    connections_.push_back(
        boardCanvas_.signalCellClicked.connect(
            sigc::mem_fun(gameCtrl_, &controller::GameController::onCellClicked)));

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
    gameCtrl_.newGame(gameCtrl_.board().size());
    logPanel_.appendLog("--- New Game ---");
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

// =============================================================================
// GameController Signal Handlers
// =============================================================================

void MainWindow::onBoardChanged() {
    boardCanvas_.refresh();
}

void MainWindow::onEngineStateChanged(engine::EngineState state) {
    statusIndicator_.setState(state);

    // Enable/disable buttons based on state
    bool idle = (state == engine::EngineState::Idle);
    bool thinking = (state == engine::EngineState::Thinking);
    bool disconnected = (state == engine::EngineState::Disconnected);

    btnThink_.set_sensitive(idle);
    btnStop_.set_sensitive(thinking);
    btnNewGame_.set_sensitive(!thinking);
    btnUndo_.set_sensitive(!thinking);

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
