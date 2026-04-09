#pragma once

#include <string>
#include <glibmm/keyfile.h>
#include <gtkmm/window.h>

namespace util {

class SettingsManager {
public:
    static SettingsManager& instance() {
        static SettingsManager instance;
        return instance;
    }

    /// Load settings from ~/.config/gomoku-portal-ui/settings.ini
    void load();

    /// Save current settings to disk
    void save();

    // =========================================================================
    // Properties
    // =========================================================================

    bool preferDarkTheme() const { return preferDarkTheme_; }
    void setPreferDarkTheme(bool val) { preferDarkTheme_ = val; }

    int lastBoardSize() const { return lastBoardSize_; }
    void setLastBoardSize(int val) { lastBoardSize_ = val; }

    int windowWidth() const { return windowWidth_; }
    void setWindowWidth(int val) { windowWidth_ = val; }

    int windowHeight() const { return windowHeight_; }
    void setWindowHeight(int val) { windowHeight_ = val; }

    bool windowMaximized() const { return windowMaximized_; }
    void setWindowMaximized(bool val) { windowMaximized_ = val; }

    int engineTurnTime() const { return engineTurnTime_; }
    void setEngineTurnTime(int val) { engineTurnTime_ = val; }

    int engineMatchTime() const { return engineMatchTime_; }
    void setEngineMatchTime(int val) { engineMatchTime_ = val; }

    int engineMaxMemory() const { return engineMaxMemory_; }
    void setEngineMaxMemory(int val) { engineMaxMemory_ = val; }

    int engineNBest() const { return engineNBest_; }
    void setEngineNBest(int val) { engineNBest_ = val; }

private:
    SettingsManager() {
        keyFile_ = Glib::KeyFile::create();
    }
    ~SettingsManager() = default;

    std::string getSettingsPath() const;

    Glib::RefPtr<Glib::KeyFile> keyFile_;

    bool preferDarkTheme_ = false;
    int lastBoardSize_ = 15;
    int windowWidth_ = -1;
    int windowHeight_ = -1;
    bool windowMaximized_ = false;

    int engineTurnTime_ = 5000;
    int engineMatchTime_ = 300000;
    int engineMaxMemory_ = 1024;
    int engineNBest_ = 1;
};

} // namespace util
