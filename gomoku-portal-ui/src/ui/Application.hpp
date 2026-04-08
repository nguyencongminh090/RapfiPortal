/*
 *  Portal Gomoku UI — Application
 *  GTK4 Application subclass. Manages the singleton GameController and MainWindow.
 */

#pragma once

#include "../controller/GameController.hpp"
#include "MainWindow.hpp"

#include <gtkmm/application.h>
#include <memory>

namespace ui {

class Application : public Gtk::Application {
public:
    static Glib::RefPtr<Application> create();

protected:
    Application();

    void on_activate() override;

private:
    std::unique_ptr<controller::GameController> gameCtrl_;
    MainWindow* mainWindow_ = nullptr;
};

}  // namespace ui
