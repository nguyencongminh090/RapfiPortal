#include "EngineSettingsPanel.hpp"
#include "../../util/SettingsManager.hpp"

namespace ui::panels {

EngineSettingsPanel::EngineSettingsPanel() : Gtk::Box(Gtk::Orientation::VERTICAL) {
    set_margin(8);
    set_spacing(12);

    grid_.set_column_spacing(12);
    grid_.set_row_spacing(8);

    auto& settings = util::SettingsManager::instance();

    // 1. Rule
    comboRule_.append("0", "Freestyle (Rule 0)");
    comboRule_.append("1", "Standard (Rule 1)");
    comboRule_.append("4", "Renju (Rule 4)");
    comboRule_.append("5", "Swap1 (Rule 5)");
    comboRule_.append("6", "Swap2 (Rule 6)");
    
    // Select current rule
    int currentRule = settings.engineRule();
    comboRule_.set_active_id(std::to_string(currentRule));
    comboRule_.signal_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onRuleChanged));

    grid_.attach(lblRule_, 0, 0);
    grid_.attach(comboRule_, 1, 0);

    // 2. Turn Time
    spinTurnTime_.set_adjustment(Gtk::Adjustment::create(settings.engineTurnTime(), 0.0, 3600000.0, 100.0));
    spinTurnTime_.set_numeric(true);
    spinTurnTime_.signal_value_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onTurnTimeChanged));
    
    grid_.attach(lblTurnTime_, 0, 1);
    grid_.attach(spinTurnTime_, 1, 1);

    // 3. Match Time
    spinMatchTime_.set_adjustment(Gtk::Adjustment::create(settings.engineMatchTime(), 0.0, 36000000.0, 1000.0));
    spinMatchTime_.set_numeric(true);
    spinMatchTime_.signal_value_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onMatchTimeChanged));
    
    grid_.attach(lblMatchTime_, 0, 2);
    grid_.attach(spinMatchTime_, 1, 2);

    // 4. Max Memory
    spinMaxMemory_.set_adjustment(Gtk::Adjustment::create(settings.engineMaxMemory(), 16.0, 65536.0, 16.0));
    spinMaxMemory_.set_numeric(true);
    spinMaxMemory_.signal_value_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onMaxMemoryChanged));

    grid_.attach(lblMaxMemory_, 0, 3);
    grid_.attach(spinMaxMemory_, 1, 3);

    // 5. Threads
    spinThreads_.set_adjustment(Gtk::Adjustment::create(settings.engineThreadNum(), 1.0, 256.0, 1.0));
    spinThreads_.set_numeric(true);
    spinThreads_.signal_value_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onThreadsChanged));

    grid_.attach(lblThreads_, 0, 4);
    grid_.attach(spinThreads_, 1, 4);

    // 6. Max Depth
    spinMaxDepth_.set_adjustment(Gtk::Adjustment::create(settings.engineMaxDepth(), 1.0, 100.0, 1.0));
    spinMaxDepth_.set_numeric(true);
    spinMaxDepth_.signal_value_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onMaxDepthChanged));

    grid_.attach(lblMaxDepth_, 0, 5);
    grid_.attach(spinMaxDepth_, 1, 5);

    // 7. Multi-PV
    spinNBest_.set_adjustment(Gtk::Adjustment::create(settings.engineNBest(), 1.0, 20.0, 1.0));
    spinNBest_.set_numeric(true);
    spinNBest_.signal_value_changed().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onNBestChanged));

    grid_.attach(lblNBest_, 0, 6);
    grid_.attach(spinNBest_, 1, 6);

    // 8. Pondering
    checkPondering_.set_active(settings.enginePondering());
    checkPondering_.signal_toggled().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onPonderingChanged));
    grid_.attach(checkPondering_, 1, 7);

    append(grid_);
    
    // 9. Actions
    Gtk::Separator* sep = Gtk::make_managed<Gtk::Separator>(Gtk::Orientation::HORIZONTAL);
    sep->set_margin_top(12);
    sep->set_margin_bottom(12);
    append(*sep);

    append(lblActions_);
    actionBox_.set_spacing(6);
    actionBox_.set_margin_top(6);
    actionBox_.append(btnClearHash_);
    actionBox_.append(btnReloadConfig_);
    actionBox_.append(btnHashUsage_);
    append(actionBox_);

    btnClearHash_.signal_clicked().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onClearHash));
    btnReloadConfig_.signal_clicked().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onReloadConfig));
    btnHashUsage_.signal_clicked().connect(sigc::mem_fun(*this, &EngineSettingsPanel::onHashUsage));
}

void EngineSettingsPanel::onRuleChanged() {
    int val = std::stoi(comboRule_.get_active_id());
    util::SettingsManager::instance().setEngineRule(val);
    signalRuleChanged.emit(val);
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

void EngineSettingsPanel::onThreadsChanged() {
    int val = spinThreads_.get_value_as_int();
    util::SettingsManager::instance().setEngineThreadNum(val);
    signalThreadsChanged.emit(val);
}

void EngineSettingsPanel::onPonderingChanged() {
    bool val = checkPondering_.get_active();
    util::SettingsManager::instance().setEnginePondering(val);
    signalPonderingChanged.emit(val);
}

void EngineSettingsPanel::onMaxDepthChanged() {
    int val = spinMaxDepth_.get_value_as_int();
    util::SettingsManager::instance().setEngineMaxDepth(val);
    signalMaxDepthChanged.emit(val);
}

void EngineSettingsPanel::onMaxNodesChanged() {
    unsigned long long val = (unsigned long long)spinMaxNodes_.get_value();
    util::SettingsManager::instance().setEngineMaxNodes(val);
    signalMaxNodesChanged.emit(val);
}

void EngineSettingsPanel::onClearHash() { signalClearHashRequested.emit(); }
void EngineSettingsPanel::onReloadConfig() { signalReloadConfigRequested.emit(); }
void EngineSettingsPanel::onHashUsage() { signalHashUsageRequested.emit(); }

} // namespace ui::panels
