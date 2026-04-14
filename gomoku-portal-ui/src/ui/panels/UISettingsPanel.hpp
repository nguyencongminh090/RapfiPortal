#pragma once

#include <gtkmm/box.h>
#include <gtkmm/checkbutton.h>
#include <sigc++/sigc++.h>

namespace ui::panels {

class UISettingsPanel : public Gtk::Box {
public:
    UISettingsPanel();
    ~UISettingsPanel() override = default;

    sigc::signal<void(bool)> signalShowPVToggled;
    sigc::signal<void(bool)> signalShowWinrateToggled;
    sigc::signal<void(bool)> signalShowMoveNumbersToggled;

private:
    Gtk::Box vBox_{Gtk::Orientation::VERTICAL};

    Gtk::CheckButton checkShowPV_{"Show Principal Variation (Ghost stones)"};
    Gtk::CheckButton checkShowWinrate_{"Show Winrate Heatmap"};
    Gtk::CheckButton checkShowMoveNumbers_{"Show Move Numbers"};

    void onShowPVToggled();
    void onShowWinrateToggled();
    void onShowMoveNumbersToggled();
};

} // namespace ui::panels
