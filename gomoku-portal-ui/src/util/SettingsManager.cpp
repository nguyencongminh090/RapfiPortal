#include "SettingsManager.hpp"
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <giomm/file.h>
#include <iostream>

namespace util {

std::string SettingsManager::getSettingsPath() const {
    std::string configDir = Glib::get_user_config_dir() + "/gomoku-portal-ui";
    auto dir = Gio::File::create_for_path(configDir);
    if (!dir->query_exists()) {
        try {
            dir->make_directory();
        } catch (const Glib::Error&) {}
    }
    return configDir + "/settings.ini";
}

void SettingsManager::load() {
    std::string path = getSettingsPath();
    try {
        if (!Glib::file_test(path, Glib::FileTest::EXISTS)) return;
        keyFile_->load_from_file(path);
        
        if (keyFile_->has_key("UI", "preferDarkTheme")) {
            preferDarkTheme_ = keyFile_->get_boolean("UI", "preferDarkTheme");
        }
        if (keyFile_->has_key("Game", "lastBoardSize")) {
            lastBoardSize_ = keyFile_->get_integer("Game", "lastBoardSize");
            if (lastBoardSize_ < 5 || lastBoardSize_ > 22) lastBoardSize_ = 15;
        }
        if (keyFile_->has_key("Game", "playDistN")) {
            playDistN_ = keyFile_->get_integer("Game", "playDistN");
            if (playDistN_ < 1) playDistN_ = 1;
        }
        if (keyFile_->has_key("Game", "playDistSelfOnly")) {
            playDistSelfOnly_ = keyFile_->get_boolean("Game", "playDistSelfOnly");
        }
        if (keyFile_->has_key("Window", "width")) {
            windowWidth_ = keyFile_->get_integer("Window", "width");
        }
        if (keyFile_->has_key("Window", "height")) {
            windowHeight_ = keyFile_->get_integer("Window", "height");
        }
        if (keyFile_->has_key("Window", "maximized")) {
            windowMaximized_ = keyFile_->get_boolean("Window", "maximized");
        }

        if (keyFile_->has_key("Engine", "turnTime")) {
            engineTurnTime_ = keyFile_->get_integer("Engine", "turnTime");
        }
        if (keyFile_->has_key("Engine", "matchTime")) {
            engineMatchTime_ = keyFile_->get_integer("Engine", "matchTime");
        }
        if (keyFile_->has_key("Engine", "maxMemory")) {
            engineMaxMemory_ = keyFile_->get_integer("Engine", "maxMemory");
        }
        if (keyFile_->has_key("Engine", "nBest")) {
            engineNBest_ = keyFile_->get_integer("Engine", "nBest");
        }

        if (keyFile_->has_key("UI", "showPVOverlay")) {
            showPVOverlay_ = keyFile_->get_boolean("UI", "showPVOverlay");
        }
        if (keyFile_->has_key("UI", "showWinrateHeatmap")) {
            showWinrateHeatmap_ = keyFile_->get_boolean("UI", "showWinrateHeatmap");
        }
        if (keyFile_->has_key("UI", "showMoveNumbers")) {
            showMoveNumbers_ = keyFile_->get_boolean("UI", "showMoveNumbers");
        }

        if (keyFile_->has_key("Engine", "rule")) {
            engineRule_ = keyFile_->get_integer("Engine", "rule");
        }
        if (keyFile_->has_key("Engine", "threadNum")) {
            engineThreadNum_ = keyFile_->get_integer("Engine", "threadNum");
        }
        if (keyFile_->has_key("Engine", "pondering")) {
            enginePondering_ = keyFile_->get_boolean("Engine", "pondering");
        }
        if (keyFile_->has_key("Engine", "maxDepth")) {
            engineMaxDepth_ = keyFile_->get_integer("Engine", "maxDepth");
        }
        if (keyFile_->has_key("Engine", "maxNodes")) {
            engineMaxNodes_ = std::stoull(keyFile_->get_string("Engine", "maxNodes"));
        }
        
    } catch (const Glib::Error& e) {
        std::cerr << "Settings error: " << e.what() << "\n";
    }
}

void SettingsManager::save() {
    std::string path = getSettingsPath();
    try {
        keyFile_->set_boolean("UI", "preferDarkTheme", preferDarkTheme_);
        keyFile_->set_integer("Game", "lastBoardSize", lastBoardSize_);
        keyFile_->set_integer("Game", "playDistN", playDistN_);
        keyFile_->set_boolean("Game", "playDistSelfOnly", playDistSelfOnly_);
        keyFile_->set_integer("Window", "width", windowWidth_);
        keyFile_->set_integer("Window", "height", windowHeight_);
        keyFile_->set_boolean("Window", "maximized", windowMaximized_);
        
        keyFile_->set_integer("Engine", "turnTime", engineTurnTime_);
        keyFile_->set_integer("Engine", "matchTime", engineMatchTime_);
        keyFile_->set_integer("Engine", "maxMemory", engineMaxMemory_);
        keyFile_->set_integer("Engine", "nBest", engineNBest_);

        keyFile_->set_boolean("UI", "showPVOverlay", showPVOverlay_);
        keyFile_->set_boolean("UI", "showWinrateHeatmap", showWinrateHeatmap_);
        keyFile_->set_boolean("UI", "showMoveNumbers", showMoveNumbers_);

        keyFile_->set_integer("Engine", "rule", engineRule_);
        keyFile_->set_integer("Engine", "threadNum", engineThreadNum_);
        keyFile_->set_boolean("Engine", "pondering", enginePondering_);
        keyFile_->set_integer("Engine", "maxDepth", engineMaxDepth_);
        keyFile_->set_string("Engine", "maxNodes", std::to_string(engineMaxNodes_));
        
        keyFile_->save_to_file(path);
    } catch (const Glib::Error& e) {
        std::cerr << "Settings save error: " << e.what() << "\n";
    }
}

} // namespace util
