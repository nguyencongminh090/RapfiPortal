/*
 *  Portal Gomoku UI — Entry Point
 *  Minimal GTK4 application stub.
 */

#include <gtkmm.h>
#include <iostream>

static void on_activate(Glib::RefPtr<Gtk::Application>& app) {
    auto window = Gtk::make_managed<Gtk::ApplicationWindow>();
    window->set_title("Portal Gomoku — Analysis Tool");
    window->set_default_size(1200, 800);

    auto label = Gtk::make_managed<Gtk::Label>("MINT-P Engine UI — Phase 1 Complete");
    label->set_margin(40);
    window->set_child(*label);

    app->add_window(*window);
    window->present();
}

int main(int argc, char* argv[]) {
    auto app = Gtk::Application::create("org.portal.gomoku");
    app->signal_activate().connect([&app]() { on_activate(app); });
    return app->run(argc, argv);
}
