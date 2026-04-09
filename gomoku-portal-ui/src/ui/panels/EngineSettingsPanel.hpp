#pragma once

#include <gtkmm/box.h>
#include <gtkmm/grid.h>
#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/adjustment.h>

namespace ui::panels {

class EngineSettingsPanel : public Gtk::Box {
public:
    EngineSettingsPanel();
    ~EngineSettingsPanel() override = default;

    sigc::signal<void(int)> signalTurnTimeChanged;
    sigc::signal<void(int)> signalMatchTimeChanged;
    sigc::signal<void(int)> signalMaxMemoryChanged;
    sigc::signal<void(int)> signalNBestChanged;

private:
    Gtk::Grid grid_;

    Gtk::Label lblTurnTime_{"Turn Time (ms):"};
    Gtk::SpinButton spinTurnTime_;

    Gtk::Label lblMatchTime_{"Match Time (ms):"};
    Gtk::SpinButton spinMatchTime_;

    Gtk::Label lblMaxMemory_{"Max Memory (MB):"};
    Gtk::SpinButton spinMaxMemory_;

    Gtk::Label lblNBest_{"Multi-PV (N-Best):"};
    Gtk::SpinButton spinNBest_;

    void onTurnTimeChanged();
    void onMatchTimeChanged();
    void onMaxMemoryChanged();
    void onNBestChanged();
};

} // namespace ui::panels
