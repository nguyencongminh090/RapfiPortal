#include "UISettingsPanel.hpp"
#include "../../util/SettingsManager.hpp"

namespace ui::panels {

UISettingsPanel::UISettingsPanel() : Gtk::Box(Gtk::Orientation::VERTICAL) {
    set_margin(12);
    set_spacing(10);

    auto& settings = util::SettingsManager::instance();

    checkShowPV_.set_active(settings.showPVOverlay());
    checkShowPV_.signal_toggled().connect(sigc::mem_fun(*this, &UISettingsPanel::onShowPVToggled));
    
    checkShowWinrate_.set_active(settings.showWinrateHeatmap());
    checkShowWinrate_.signal_toggled().connect(sigc::mem_fun(*this, &UISettingsPanel::onShowWinrateToggled));

    checkShowMoveNumbers_.set_active(settings.showMoveNumbers());
    checkShowMoveNumbers_.signal_toggled().connect(sigc::mem_fun(*this, &UISettingsPanel::onShowMoveNumbersToggled));

    append(checkShowPV_);
    append(checkShowWinrate_);
    append(checkShowMoveNumbers_);
}

void UISettingsPanel::onShowPVToggled() {
    bool val = checkShowPV_.get_active();
    util::SettingsManager::instance().setShowPVOverlay(val);
    signalShowPVToggled.emit(val);
}

void UISettingsPanel::onShowWinrateToggled() {
    bool val = checkShowWinrate_.get_active();
    util::SettingsManager::instance().setShowWinrateHeatmap(val);
    signalShowWinrateToggled.emit(val);
}

void UISettingsPanel::onShowMoveNumbersToggled() {
    bool val = checkShowMoveNumbers_.get_active();
    util::SettingsManager::instance().setShowMoveNumbers(val);
    signalShowMoveNumbersToggled.emit(val);
}

} // namespace ui::panels
