/*
 *  Portal Gomoku UI — Entry Point
 */

#include "ui/Application.hpp"

int main(int argc, char* argv[]) {
    auto app = ui::Application::create();
    return app->run(argc, argv);
}
