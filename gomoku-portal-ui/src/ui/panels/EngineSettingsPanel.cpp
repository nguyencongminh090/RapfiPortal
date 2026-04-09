#include "EngineSettingsPanel.hpp"
#include "../../util/SettingsManager.hpp"

namespace ui::panels {

EngineSettingsPanel::EngineSettingsPanel() : Gtk::Box(Gtk::Orientation::VERTICAL) {
    set_margin(8);
    set_spacing(12);

    grid_.set_column_spacing(12);
    grid_.set_row_spacing(8);

    auto& settings = util::SettingsManager::instance();

    // Turn Time
    spinTurnTime_.set_adjustment(Gtk::Adjustment::create(settings.engineTurnTime(), 1.0, 3600000.0, 100.0));
    spinTurnTime_.set_numeric(true);
    spinTurnTime_.signal_value_changed().connect(
        sigc::mem_fun(*this, &EngineSettingsPanel::onTurnTimeChanged));
    
    grid_.attach(lblTurnTime_, 0, 0);
    grid_.attach(spinTurnTime_, 1, 0);

    // Match Time
    spinMatchTime_.set_adjustment(Gtk::Adjustment::create(settings.engineMatchTime(), 1.0, 36000000.0, 1000.0));
    spinMatchTime_.set_numeric(true);
    spinMatchTime_.signal_value_changed().connect(
        sigc::mem_fun(*this, &EngineSettingsPanel::onMatchTimeChanged));
    
    grid_.attach(lblMatchTime_, 0, 1);
    grid_.attach(spinMatchTime_, 1, 1);

    // Max Memory
    spinMaxMemory_.set_adjustment(Gtk::Adjustment::create(settings.engineMaxMemory(), 16.0, 65536.0, 16.0));
    spinMaxMemory_.set_numeric(true);
    spinMaxMemory_.signal_value_changed().connect(
        sigc::mem_fun(*this, &EngineSettingsPanel::onMaxMemoryChanged));

    grid_.attach(lblMaxMemory_, 0, 2);
    grid_.attach(spinMaxMemory_, 1, 2);

    // N-Best
    spinNBest_.set_adjustment(Gtk::Adjustment::create(settings.engineNBest(), 1.0, 20.0, 1.0));
    spinNBest_.set_numeric(true);
    spinNBest_.signal_value_changed().connect(
        sigc::mem_fun(*this, &EngineSettingsPanel::onNBestChanged));

    grid_.attach(lblNBest_, 0, 3);
    grid_.attach(spinNBest_, 1, 3);

    append(grid_);
}

void EngineSettingsPanel::onTurnTimeChanged() {
    int val = spinTurnTime_.get_value_as_int();
    util::SettingsManager::instance().setEngineTurnTime(val);
    signalTurnTimeChanged.emit(val);
}

void EngineSettingsPanel::onMatchTimeChanged() {
    int val = spinMatchTime_.get_value_as_int();
    util::SettingsManager::instance().setEngineMatchTime(val);
    signalMatchTimeChanged.emit(val);
}

void EngineSettingsPanel::onMaxMemoryChanged() {
    int val = spinMaxMemory_.get_value_as_int();
    util::SettingsManager::instance().setEngineMaxMemory(val);
    signalMaxMemoryChanged.emit(val);
}

void EngineSettingsPanel::onNBestChanged() {
    int val = spinNBest_.get_value_as_int();
    util::SettingsManager::instance().setEngineNBest(val);
    signalNBestChanged.emit(val);
}

} // namespace ui::panels
