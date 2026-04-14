#pragma once

#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/button.h>
#include <gtkmm/separator.h>
#include <sigc++/sigc++.h>

namespace ui::panels {

class EngineSettingsPanel : public Gtk::Box {
public:
    EngineSettingsPanel();
    ~EngineSettingsPanel() override = default;

    sigc::signal<void(int)> signalTurnTimeChanged;
    sigc::signal<void(int)> signalMatchTimeChanged;
    sigc::signal<void(int)> signalMaxMemoryChanged;
    sigc::signal<void(int)> signalNBestChanged;
    sigc::signal<void(int)> signalRuleChanged;
    sigc::signal<void(int)> signalThreadsChanged;
    sigc::signal<void(bool)> signalPonderingChanged;
    sigc::signal<void(int)> signalMaxDepthChanged;
    sigc::signal<void(unsigned long long)> signalMaxNodesChanged;

    sigc::signal<void()> signalClearHashRequested;
    sigc::signal<void()> signalReloadConfigRequested;
    sigc::signal<void()> signalHashUsageRequested;

private:
    Gtk::Box   scrollBox_{Gtk::Orientation::VERTICAL};
    Gtk::Grid grid_;

    Gtk::Label lblRule_{"Rule:"};
    Gtk::ComboBoxText comboRule_;

    Gtk::Label lblTurnTime_{"Turn Time (ms):"};
    Gtk::SpinButton spinTurnTime_;

    Gtk::Label lblMatchTime_{"Match Time (ms):"};
    Gtk::SpinButton spinMatchTime_;

    Gtk::Label lblMaxMemory_{"Max Memory (MB):"};
    Gtk::SpinButton spinMaxMemory_;

    Gtk::Label lblNBest_{"Multi-PV (N-Best):"};
    Gtk::SpinButton spinNBest_;

    Gtk::Label lblThreads_{"Threads:"};
    Gtk::SpinButton spinThreads_;

    Gtk::Label lblMaxDepth_{"Max Depth:"};
    Gtk::SpinButton spinMaxDepth_;

    Gtk::Label lblMaxNodes_{"Max Nodes (0=inf):"};
    Gtk::SpinButton spinMaxNodes_;

    Gtk::CheckButton checkPondering_{"Engine Pondering"};

    Gtk::Label lblActions_{"Maintenance Actions:"};
    Gtk::Box actionBox_{Gtk::Orientation::HORIZONTAL};
    Gtk::Button btnClearHash_{"Clear Hash"};
    Gtk::Button btnReloadConfig_{"Reload Config"};
    Gtk::Button btnHashUsage_{"Usage"};

    void onRuleChanged();
    void onTurnTimeChanged();
    void onMatchTimeChanged();
    void onMaxMemoryChanged();
    void onNBestChanged();
    void onThreadsChanged();
    void onPonderingChanged();
    void onMaxDepthChanged();
    void onMaxNodesChanged();

    void onClearHash();
    void onReloadConfig();
    void onHashUsage();
};

} // namespace ui::panels
