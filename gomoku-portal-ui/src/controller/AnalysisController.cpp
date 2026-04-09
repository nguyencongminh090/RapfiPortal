/*
 *  Portal Gomoku UI — Analysis Controller Implementation
 */

#include "AnalysisController.hpp"
#include "GameController.hpp"
#include "../util/StringUtils.hpp"

#include <iostream>

namespace controller {

AnalysisController::AnalysisController(GameController& gameCtrl)
    : gameCtrl_(gameCtrl) {
}

void AnalysisController::connectSignals() {
    gameCtrl_.signalEngineMessage.connect(
        sigc::mem_fun(*this, &AnalysisController::onEngineMessage));
        
    gameCtrl_.signalBoardChanged.connect([this]() {
        // If board resets or user moves, we should probably clear analysis
        // But for now, just let the next search overwrite it.
    });
}

void AnalysisController::onEngineMessage(const std::string& msg) {
    if (util::startsWith(msg, "depth ")) {
        parseDepthLine(msg);
    } else if (util::startsWith(msg, "Speed ") || util::startsWith(msg, "Visit ")) {
        parseSpeedLine(msg);
    } else if (msg == "REALTIME REFRESH" || util::startsWith(msg, "Searching")) {
        // Clear analysis config
        info_ = {};
        lastDepthUpdated_ = 0;
        signalAnalysisUpdated.emit();
    }
}

void AnalysisController::parseDepthLine(const std::string& line) {
    auto tokens = util::split(line, ' ');
    model::AnalysisMove mv;
    int currentDepth = 0;
    
    for (size_t i = 0; i < tokens.size(); ++i) {
        if (tokens[i] == "depth" && i + 1 < tokens.size()) {
            std::string dstr = std::string(tokens[++i]);
            size_t dash = dstr.find('-');
            if (dash != std::string::npos) {
                dstr = dstr.substr(0, dash);
            }
            try {
                currentDepth = std::stoi(dstr);
                info_.depth = currentDepth; // Update overall depth
                mv.depth = currentDepth;
            } catch (...) {}
        }
        else if (tokens[i] == "multipv" && i + 1 < tokens.size()) {
            try { mv.rank = std::stoi(std::string(tokens[++i])); } catch (...) {}
        }
        else if (tokens[i] == "ev" && i + 1 < tokens.size()) {
            try {
                mv.score = std::stoi(std::string(tokens[++i]));
                if (mv.rank <= 1) info_.score = mv.score;
            } catch (...) {}
        }
        else if (tokens[i] == "pv") {
            // all subsequent are moves
            auto pvPos = line.find("pv ");
            if (pvPos != std::string::npos) {
                mv.pvText = line.substr(pvPos + 3);
            }
            for (size_t j = i + 1; j < tokens.size(); ++j) {
                auto args = util::split(tokens[j], ',');
                if (args.size() >= 2) {
                    try {
                        mv.pv.push_back({std::stoi(std::string(args[0])), std::stoi(std::string(args[1]))});
                    } catch (...) {}
                }
            }
            if (!mv.pv.empty()) {
                mv.coord = mv.pv.front();
            }
            break; // Done parsing
        }
    }
    
    // Default single PV
    if (mv.rank == 0) mv.rank = 1;
    
    // Clear nBest if we jumped to a new depth on rank 1
    if (mv.rank == 1 && currentDepth > lastDepthUpdated_) {
        info_.nBest.clear();
        lastDepthUpdated_ = currentDepth;
    }
    
    // Insert into multiPV slots
    if (info_.nBest.size() < static_cast<size_t>(mv.rank)) {
        info_.nBest.resize(mv.rank);
    }
    // Update slot
    if (mv.rank > 0) {
        info_.nBest[mv.rank - 1] = mv;
        if (mv.rank == 1) {
            info_.bestMove = mv.pvText;
        }
    }
    
    signalAnalysisUpdated.emit();
}

void AnalysisController::parseSpeedLine(const std::string& line) {
    auto parts = util::split(line, '|');
    for (auto& p : parts) {
        auto tokens = util::split(p, ' ');
        std::vector<std::string> valid;
        for (auto& t : tokens) {
            if (!t.empty()) valid.push_back(std::string(t));
        }
        
        if (valid.size() >= 2) {
            std::string key = valid[0];
            std::string val = valid[1];
            
            if (key == "Speed") {
                info_.nps = parseHumanReadableNum(val);
            } else if (key == "Visit") {
                info_.nodes = parseHumanReadableNum(val);
            } else if (key == "Time") {
                if (!val.empty() && val.back() == 's') {
                    // e.g. "1.2s"
                    val.pop_back();
                    try {
                        info_.timeMs = static_cast<int>(std::stof(val) * 1000.0f);
                    } catch (...) {}
                } else if (!val.empty() && val.back() == 'm') {
                    // e.g. "12m" for mins (rare)
                    val.pop_back();
                    try {
                        info_.timeMs = static_cast<int>(std::stof(val) * 60000.0f);
                    } catch (...) {}
                } else {
                    // assume ms
                    try { info_.timeMs = std::stoi(val); } catch (...) {}
                }
            }
        }
    }
    signalAnalysisUpdated.emit();
}

int AnalysisController::parseHumanReadableNum(const std::string& s) const {
    if (s.empty()) return 0;
    float mult = 1.0f;
    char suffix = s.back();
    std::string num = s;
    
    if (suffix == 'k' || suffix == 'K') { mult = 1e3f; num.pop_back(); }
    else if (suffix == 'm' || suffix == 'M') { mult = 1e6f; num.pop_back(); }
    else if (suffix == 'g' || suffix == 'G') { mult = 1e9f; num.pop_back(); }
    
    try {
        return static_cast<int>(std::stof(num) * mult);
    } catch (...) {
        return 0;
    }
}

}  // namespace controller
