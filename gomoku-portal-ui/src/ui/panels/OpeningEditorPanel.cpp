#include "OpeningEditorPanel.hpp"
#include "../../model/OBFManager.hpp"
#include <gtkmm/window.h>
#include <giomm/file.h>
#include <giomm/liststore.h>

namespace ui::panels {

OpeningEditorPanel::OpeningEditorPanel(controller::GameController& gameCtrl)
    : Gtk::Box(Gtk::Orientation::VERTICAL, 5)
    , gameCtrl_(gameCtrl)
{
    setupLayout();
}

void OpeningEditorPanel::setupLayout() {
    set_margin(10);

    lblInfo_.set_wrap(true);
    lblInfo_.set_halign(Gtk::Align::START);

    btnSelectFile_.signal_clicked().connect(sigc::mem_fun(*this, &OpeningEditorPanel::onSelectFile));
    btnAppend_.signal_clicked().connect(sigc::mem_fun(*this, &OpeningEditorPanel::onAppend));
    btnAppend_.set_sensitive(false);

    append(btnSelectFile_);
    append(lblInfo_);
    append(btnAppend_);
    append(lblStatus_);
}

void OpeningEditorPanel::onSelectFile() {
    auto* window = dynamic_cast<Gtk::Window*>(get_root());
    if (!window) return;

    auto dialog = Gtk::FileDialog::create();
    dialog->set_title("Select or Create OBF File");

    // Filtering requires Gio::ListStore which has different template signatures across giomm versions.
    // Omitted for maximum compatibility.


    dialog->save(*window, [this, dialog](Glib::RefPtr<Gio::AsyncResult>& result) {
        try {
            auto file = dialog->save_finish(result);
            if (file) {
                currentObfPath_ = file->get_path();
                if (currentObfPath_.length() < 4 || currentObfPath_.substr(currentObfPath_.length() - 4) != ".obf") {
                    currentObfPath_ += ".obf";
                }
                updateFileInfo();
            }
        } catch (const Glib::Error&) {}
    });
}

void OpeningEditorPanel::updateFileInfo() {
    if (currentObfPath_.empty()) {
        lblInfo_.set_text("No file selected.");
        btnAppend_.set_sensitive(false);
        return;
    }
    
    auto openings = model::OBFManager::readOpenings(currentObfPath_);
    lblInfo_.set_text("File: " + currentObfPath_ + "\nOpenings count: " + std::to_string(openings.size()));
    btnAppend_.set_sensitive(true);
    lblStatus_.set_text("");
}

void OpeningEditorPanel::onAppend() {
    if (currentObfPath_.empty()) return;

    auto record = model::GameRecord::fromBoard(gameCtrl_.board());
    
    if (model::OBFManager::appendOpening(currentObfPath_, record)) {
        lblStatus_.set_text("Successfully appended to OBF!");
        updateFileInfo(); // refresh count
    } else {
        lblStatus_.set_markup("<span foreground='red'>Failed to append data.</span>");
    }
}

} // namespace ui::panels
