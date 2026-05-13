/*
 *  Portal Gomoku UI — SPSA Tuning Panel Implementation
 */

#include "SPSAPanel.hpp"

#include <gtkmm/adjustment.h>
#include <gtkmm/filedialog.h>
#include <gtkmm/window.h>
#include <giomm/file.h>
#include <sstream>
#include <iomanip>
#include <cmath>

namespace ui::panels {

SPSAPanel::SPSAPanel(controller::SPSAController& spsaCtrl)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 6)
    , spsaCtrl_(spsaCtrl)
{
    setupLayout();
    setupSignals();
}

void SPSAPanel::setupLayout() {
    mainScroll_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    mainScroll_.set_child(mainBox_);
    mainScroll_.set_vexpand(true);
    append(mainScroll_);

    mainBox_.set_margin(8);

    lblTitle_.set_use_markup(true);
    mainBox_.append(lblTitle_);
    mainBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Engine selection
    lblEngine_.set_wrap(true);
    mainBox_.append(btnSelectEngine_);
    mainBox_.append(lblEngine_);

    // Baseline engine selection
    lblBaseline_.set_wrap(true);
    mainBox_.append(btnSelectBaseline_);
    mainBox_.append(lblBaseline_);

    // OBF
    lblOBF_.set_wrap(true);
    mainBox_.append(btnSelectOBF_);
    mainBox_.append(lblOBF_);

    // State file
    lblState_.set_wrap(true);
    mainBox_.append(btnSelectState_);
    mainBox_.append(lblState_);

    mainBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // SPSA hyperparameters
    auto adjA = Gtk::Adjustment::create(1.0, 0.01, 100.0, 0.1, 1.0, 0);
    spinA_.set_adjustment(adjA);
    spinA_.set_digits(2);
    mainBox_.append(lblA_);
    mainBox_.append(spinA_);

    auto adjC = Gtk::Adjustment::create(2.0, 0.1, 50.0, 0.1, 1.0, 0);
    spinC_.set_adjustment(adjC);
    spinC_.set_digits(2);
    mainBox_.append(lblC_);
    mainBox_.append(spinC_);

    auto adjStab = Gtk::Adjustment::create(10.0, 0.0, 1000.0, 1.0, 10.0, 0);
    spinStab_.set_adjustment(adjStab);
    spinStab_.set_digits(1);
    mainBox_.append(lblStab_);
    mainBox_.append(spinStab_);

    auto adjGames = Gtk::Adjustment::create(20, 2, 200, 2, 10, 0);
    spinGames_.set_adjustment(adjGames);
    spinGames_.set_numeric(true);
    mainBox_.append(lblGames_);
    mainBox_.append(spinGames_);

    auto adjTurn = Gtk::Adjustment::create(5000, 500, 60000, 500, 1000, 0);
    spinTurnTime_.set_adjustment(adjTurn);
    spinTurnTime_.set_numeric(true);
    mainBox_.append(lblTurnTime_);
    mainBox_.append(spinTurnTime_);

    auto adjThreads = Gtk::Adjustment::create(1, 0, 64, 1, 4, 0);
    spinThreads_.set_adjustment(adjThreads);
    spinThreads_.set_numeric(true);
    mainBox_.append(lblThreads_);
    mainBox_.append(spinThreads_);

    auto adjMem = Gtk::Adjustment::create(350, 64, 16384, 64, 512, 0);
    spinMemory_.set_adjustment(adjMem);
    spinMemory_.set_numeric(true);
    mainBox_.append(lblMemory_);
    mainBox_.append(spinMemory_);

    auto adjMatchTime = Gtk::Adjustment::create(0, 0, 600000, 1000, 5000, 0);
    spinMatchTime_.set_adjustment(adjMatchTime);
    spinMatchTime_.set_numeric(true);
    mainBox_.append(lblMatchTime_);
    mainBox_.append(spinMatchTime_);

    auto adjConc = Gtk::Adjustment::create(1, 1, 8, 1, 1, 0);
    spinConcurrency_.set_adjustment(adjConc);
    spinConcurrency_.set_numeric(true);
    mainBox_.append(lblConcurrency_);
    mainBox_.append(spinConcurrency_);

    mainBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Validation settings
    auto adjValInt = Gtk::Adjustment::create(5, 0, 100, 1, 5, 0);
    spinValInterval_.set_adjustment(adjValInt);
    spinValInterval_.set_numeric(true);
    mainBox_.append(lblValInterval_);
    mainBox_.append(spinValInterval_);

    auto adjValG = Gtk::Adjustment::create(20, 2, 200, 2, 10, 0);
    spinValGames_.set_adjustment(adjValG);
    spinValGames_.set_numeric(true);
    mainBox_.append(lblValGames_);
    mainBox_.append(spinValGames_);

    mainBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Controls
    auto* btnBox = Gtk::make_managed<Gtk::Box>(Gtk::Orientation::HORIZONTAL, 4);
    btnBox->append(btnStart_);
    btnBox->append(btnStop_);
    mainBox_.append(*btnBox);
    btnStop_.set_sensitive(false);

    // Status
    lblStatus_.set_halign(Gtk::Align::START);
    lblIteration_.set_halign(Gtk::Align::START);
    lblProgress_.set_halign(Gtk::Align::START);
    mainBox_.append(lblStatus_);
    mainBox_.append(lblIteration_);
    mainBox_.append(lblProgress_);
    progressBar_.set_show_text(true);
    mainBox_.append(progressBar_);
    btnSpectate_.set_active(false);
    mainBox_.append(btnSpectate_);

    mainBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Parameter display
    lblParamTitle_.set_use_markup(true);
    mainBox_.append(lblParamTitle_);
    lblParams_.set_use_markup(true);
    lblParams_.set_halign(Gtk::Align::START);
    lblParams_.set_wrap(true);
    mainBox_.append(lblParams_);

    mainBox_.append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Validation results
    lblValTitle_.set_use_markup(true);
    mainBox_.append(lblValTitle_);
    lblValResults_.set_use_markup(true);
    lblValResults_.set_halign(Gtk::Align::START);
    lblValResults_.set_wrap(true);
    lblValResults_.set_markup("<i>No validation yet</i>");
    mainBox_.append(lblValResults_);

    append(*Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL));

    // Log
    lblLogTitle_.set_use_markup(true);
    append(lblLogTitle_);

    logView_.set_editable(false);
    logView_.set_cursor_visible(false);
    logView_.set_wrap_mode(Gtk::WrapMode::WORD_CHAR);
    logView_.add_css_class("monospace");
    logScroll_.set_child(logView_);
    logScroll_.set_min_content_height(150);
    logScroll_.set_vexpand(true);
    logScroll_.set_policy(Gtk::PolicyType::AUTOMATIC, Gtk::PolicyType::AUTOMATIC);
    append(logScroll_);
}

void SPSAPanel::setupSignals() {
    btnSelectEngine_.signal_clicked().connect(
        sigc::mem_fun(*this, &SPSAPanel::onSelectEngine));
    btnSelectBaseline_.signal_clicked().connect(
        sigc::mem_fun(*this, &SPSAPanel::onSelectBaseline));
    btnSelectOBF_.signal_clicked().connect(
        sigc::mem_fun(*this, &SPSAPanel::onSelectOBF));
    btnSelectState_.signal_clicked().connect(
        sigc::mem_fun(*this, &SPSAPanel::onSelectState));
    btnStart_.signal_clicked().connect(
        sigc::mem_fun(*this, &SPSAPanel::onStart));
    btnStop_.signal_clicked().connect(
        sigc::mem_fun(*this, &SPSAPanel::onStop));

    spsaCtrl_.signalStateChanged.connect(
        sigc::mem_fun(*this, &SPSAPanel::onStateChanged));
    spsaCtrl_.signalLogMessage.connect(
        sigc::mem_fun(*this, &SPSAPanel::onLogMessage));
    spsaCtrl_.signalIterationComplete.connect(
        sigc::mem_fun(*this, &SPSAPanel::onIterationComplete));
    spsaCtrl_.signalParamsUpdated.connect(
        sigc::mem_fun(*this, &SPSAPanel::onParamsUpdated));
    spsaCtrl_.signalValidationComplete.connect(
        sigc::mem_fun(*this, &SPSAPanel::onValidationComplete));
    spsaCtrl_.signalProgressUpdated.connect(
        sigc::mem_fun(*this, &SPSAPanel::onProgressUpdated));

    btnSpectate_.signal_toggled().connect([this]() {
        spsaCtrl_.setSpectating(btnSpectate_.get_active());
    });
}

// ============================================================================
// File Selection
// ============================================================================

void SPSAPanel::onSelectEngine() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Tuning Engine");
    dialog->open(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                pathEngine_ = file->get_path();
                lblEngine_.set_text(file->get_basename());
            }
        } catch (const Glib::Error&) {}
    });
}

void SPSAPanel::onSelectBaseline() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Baseline Engine (for validation)");
    dialog->open(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                pathBaseline_ = file->get_path();
                lblBaseline_.set_text(file->get_basename());
            }
        } catch (const Glib::Error&) {}
    });
}

void SPSAPanel::onSelectOBF() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select Opening Book File");
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

void SPSAPanel::onSelectState() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;
    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select State File");
    dialog->open(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->open_finish(result);
            if (file) {
                pathState_ = file->get_path();
                lblState_.set_text(file->get_basename());
            }
        } catch (const Glib::Error&) {}
    });
}

// ============================================================================
// Start / Stop
// ============================================================================

void SPSAPanel::onStart() {
    if (pathEngine_.empty() || pathOBF_.empty()) {
        onLogMessage("Please select engine and opening file first!");
        return;
    }

    controller::SPSAConfig cfg;
    cfg.enginePath = pathEngine_;
    cfg.baselinePath = pathBaseline_; // empty = no validation
    cfg.obfPath = pathOBF_;
    cfg.statePath = pathState_;
    cfg.a = spinA_.get_value();
    cfg.c = spinC_.get_value();
    cfg.A = spinStab_.get_value();
    cfg.gamesPerIteration = spinGames_.get_value_as_int();
    cfg.turnTimeMs = spinTurnTime_.get_value_as_int();
    cfg.threads = spinThreads_.get_value_as_int();
    cfg.maxMemory = static_cast<int64_t>(spinMemory_.get_value_as_int()) * 1024 * 1024;
    cfg.validationInterval = spinValInterval_.get_value_as_int();
    cfg.validationGames = spinValGames_.get_value_as_int();
    cfg.matchTimeMs = spinMatchTime_.get_value_as_int();
    cfg.concurrency = spinConcurrency_.get_value_as_int();

    spsaCtrl_.start(cfg);
}

void SPSAPanel::onStop() {
    spsaCtrl_.stop();
}

// ============================================================================
// Signal Handlers
// ============================================================================

void SPSAPanel::onStateChanged() {
    auto state = spsaCtrl_.state();
    bool busy = (state == controller::SPSAState::Playing
              || state == controller::SPSAState::Validating);

    btnSelectEngine_.set_sensitive(!busy);
    btnSelectBaseline_.set_sensitive(!busy);
    btnSelectOBF_.set_sensitive(!busy);
    btnSelectState_.set_sensitive(!busy);
    btnStart_.set_sensitive(!busy);
    btnStop_.set_sensitive(busy);

    spinA_.set_sensitive(!busy);
    spinC_.set_sensitive(!busy);
    spinStab_.set_sensitive(!busy);
    spinGames_.set_sensitive(!busy);
    spinTurnTime_.set_sensitive(!busy);
    spinThreads_.set_sensitive(!busy);
    spinMemory_.set_sensitive(!busy);
    spinMatchTime_.set_sensitive(!busy);
    spinConcurrency_.set_sensitive(!busy);
    spinValInterval_.set_sensitive(!busy);
    spinValGames_.set_sensitive(!busy);

    if (state == controller::SPSAState::Idle)
        lblStatus_.set_text("Status: Idle");
    else if (state == controller::SPSAState::Playing)
        lblStatus_.set_text("Status: SPSA Tuning...");
    else if (state == controller::SPSAState::Validating)
        lblStatus_.set_text("Status: Validation Match (θ vs Baseline)...");
    else if (state == controller::SPSAState::Finished)
        lblStatus_.set_text("Status: Stopped / Finished");
}

void SPSAPanel::onLogMessage(const std::string& msg) {
    auto buf = logView_.get_buffer();
    buf->insert(buf->end(), msg + "\n");
    auto mark = buf->create_mark(buf->end());
    logView_.scroll_to(mark);
}

void SPSAPanel::onIterationComplete() {
    lblIteration_.set_text("Iteration: " + std::to_string(spsaCtrl_.currentIteration()));
}

void SPSAPanel::onParamsUpdated() {
    refreshParamDisplay();
}

void SPSAPanel::onValidationComplete() {
    refreshValidationDisplay();
}

void SPSAPanel::refreshParamDisplay() {
    const auto& params = spsaCtrl_.params();
    if (params.empty()) {
        lblParams_.set_markup("<i>No parameters loaded</i>");
        return;
    }

    std::ostringstream oss;
    for (const auto& p : params) {
        int cur = static_cast<int>(std::round(p.value));
        int ini = static_cast<int>(std::round(p.initial));
        int diff = cur - ini;
        std::string diffStr = (diff >= 0 ? "+" : "") + std::to_string(diff);
        std::string color = (std::abs(diff) > 2) ? "#4CAF50" : "#888888";

        oss << "<span font_desc='Monospace 8'>"
            << "<b>" << p.name << "</b>: "
            << "<span color='" << color << "'>" << cur << "</span>"
            << " <span color='#888888'>(" << diffStr << ")</span>"
            << " [" << static_cast<int>(p.min) << ".." << static_cast<int>(p.max) << "]"
            << "</span>\n";
    }
    lblParams_.set_markup(oss.str());
}

void SPSAPanel::refreshValidationDisplay() {
    const auto& vh = spsaCtrl_.validationHistory();
    if (vh.empty()) {
        lblValResults_.set_markup("<i>No validation yet</i>");
        return;
    }

    std::ostringstream oss;
    oss << std::fixed << std::setprecision(1);
    for (const auto& v : vh) {
        // Color winrate: green if > 52%, red if < 48%, grey if near 50%
        std::string color = "#888888";
        if (v.winrate > 52.0) color = "#4CAF50";
        else if (v.winrate < 48.0) color = "#e24a4a";

        oss << "<span font_desc='Monospace 8'>"
            << "After iter " << v.afterIteration << ": "
            << "W=" << v.scoreTuned << " L=" << v.scoreBaseline
            << " D=" << v.draws
            << " <span color='" << color << "' weight='bold'>"
            << v.winrate << "%</span>"
            << "</span>\n";
    }
    lblValResults_.set_markup(oss.str());
}

void SPSAPanel::onProgressUpdated() {
    int done = spsaCtrl_.gamesCompleted();
    int total = spsaCtrl_.gamesTotalCurrent();
    int active = spsaCtrl_.activeSlotsCount();

    lblProgress_.set_text("Games: " + std::to_string(done) + "/" + std::to_string(total)
                          + "  Slots: " + std::to_string(active) + " active");
    if (total > 0)
        progressBar_.set_fraction(static_cast<double>(done) / total);
    else
        progressBar_.set_fraction(0.0);
    progressBar_.set_text(std::to_string(done) + "/" + std::to_string(total));
}

} // namespace ui::panels
