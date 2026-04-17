/*
 *  Portal Gomoku UI — Opening Editor Panel
 *  Panel to manage appending the current board state to an OBF file.
 */

#pragma once

#include "../../controller/GameController.hpp"
#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/label.h>
#include <gtkmm/filedialog.h>
#include <string>

namespace ui::panels {

class OpeningEditorPanel : public Gtk::Box {
public:
    explicit OpeningEditorPanel(controller::GameController& gameCtrl);
    ~OpeningEditorPanel() override = default;

private:
    controller::GameController& gameCtrl_;
    std::string currentObfPath_;

    Gtk::Label  lblInfo_{"No OBF file selected."};
    Gtk::Button btnSelectFile_{"Select OBF File..."};
    Gtk::Button btnAppend_{"Append Current Board"};
    Gtk::Label  lblStatus_{""};

    void setupLayout();
    void onSelectFile();
    void onAppend();
    void updateFileInfo();
};

} // namespace ui::panels
