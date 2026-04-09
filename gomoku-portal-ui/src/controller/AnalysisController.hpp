/*
 *  Portal Gomoku UI — Analysis Controller
 *  Parses engine MESSAGE output to extract continuous search telemetry (PVs, depth, speed, etc.).
 */

#pragma once

#include "../model/AnalysisInfo.hpp"
#include <sigc++/sigc++.h>
#include <string>

namespace controller {

class GameController;

/// Observes engine messages and parses out multi-PV and speed statistics.
class AnalysisController {
public:
    explicit AnalysisController(GameController& gameCtrl);

    /// Connect to signals from GameController.
    void connectSignals();

    ~AnalysisController();

    /// Get the latest analyzed state
    [[nodiscard]] const model::AnalysisInfo& info() const { return info_; }

    /// Fired whenever new analysis info is parsed.
    sigc::signal<void()> signalAnalysisUpdated;

private:
    GameController& gameCtrl_;
    model::AnalysisInfo info_;
    int lastDepthUpdated_ = 0;

    // BUG-011 FIX: store connections for lifecycle management + double-connect guard
    std::vector<sigc::connection> connections_;

    void onEngineMessage(const std::string& msg);
    
    void parseDepthLine(const std::string& line);
    void parseSpeedLine(const std::string& line);
    
    // Utility to parse human-readable "1.2M" or "500k" into integer
    int parseHumanReadableNum(const std::string& s) const;
};

}  // namespace controller
