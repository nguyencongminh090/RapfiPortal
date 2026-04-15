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

    int playDistN() const { return playDistN_; }
    void setPlayDistN(int val) { playDistN_ = val; }

    bool playDistSelfOnly() const { return playDistSelfOnly_; }
    void setPlayDistSelfOnly(bool val) { playDistSelfOnly_ = val; }

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

    bool showPVOverlay() const { return showPVOverlay_; }
    void setShowPVOverlay(bool val) { showPVOverlay_ = val; }

    bool showWinrateHeatmap() const { return showWinrateHeatmap_; }
    void setShowWinrateHeatmap(bool val) { showWinrateHeatmap_ = val; }

    bool showMoveNumbers() const { return showMoveNumbers_; }
    void setShowMoveNumbers(bool val) { showMoveNumbers_ = val; }

    int engineRule() const { return engineRule_; }
    void setEngineRule(int val) { engineRule_ = val; }

    int engineThreadNum() const { return engineThreadNum_; }
    void setEngineThreadNum(int val) { engineThreadNum_ = val; }

    bool enginePondering() const { return enginePondering_; }
    void setEnginePondering(bool val) { enginePondering_ = val; }

    int engineMaxDepth() const { return engineMaxDepth_; }
    void setEngineMaxDepth(int val) { engineMaxDepth_ = val; }

    unsigned long long engineMaxNodes() const { return engineMaxNodes_; }
    void setEngineMaxNodes(unsigned long long val) { engineMaxNodes_ = val; }

private:
    SettingsManager() {
        keyFile_ = Glib::KeyFile::create();
    }
    ~SettingsManager() = default;

    std::string getSettingsPath() const;

    Glib::RefPtr<Glib::KeyFile> keyFile_;

    bool preferDarkTheme_ = false;
    int lastBoardSize_ = 15;
    int playDistN_ = 5;
    bool playDistSelfOnly_ = false;
    int windowWidth_ = -1;
    int windowHeight_ = -1;
    bool windowMaximized_ = false;

    int engineTurnTime_ = 5000;
    int engineMatchTime_ = 300000;
    int engineMaxMemory_ = 1024;
    int engineNBest_ = 1;

    bool showPVOverlay_ = true;
    bool showWinrateHeatmap_ = true;
    bool showMoveNumbers_ = false;

    int engineRule_ = 0; // Freestyle
    int engineThreadNum_ = 1;
    bool enginePondering_ = false;
    int engineMaxDepth_ = 99;
    unsigned long long engineMaxNodes_ = 0; // Unlimited
};

} // namespace util
