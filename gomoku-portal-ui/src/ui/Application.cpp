#include "Application.hpp"
#include "../util/SettingsManager.hpp"
#include <iostream>

namespace ui {

Application::Application()
    : Gtk::Application("org.portal.gomoku",
                       Gio::Application::Flags::DEFAULT_FLAGS) {
}

Glib::RefPtr<Application> Application::create() {
    return Glib::make_refptr_for_instance<Application>(new Application());
}

void Application::on_activate() {
    // 1. Load settings from disk
    util::SettingsManager::instance().load();

    // 2. Create the game controller (owns Board + Engine)
    if (!gameCtrl_) {
        gameCtrl_ = std::make_unique<controller::GameController>();
    }

    // Create the main window
    mainWindow_ = Gtk::make_managed<MainWindow>(*gameCtrl_);
    add_window(*mainWindow_);
    mainWindow_->present();
}

}  // namespace ui
