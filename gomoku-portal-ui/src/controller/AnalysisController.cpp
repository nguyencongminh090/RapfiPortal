/*
 *  Portal Gomoku UI — Analysis Controller Implementation
 */

#include "AnalysisController.hpp"
#include "GameController.hpp"
#include "../util/StringUtils.hpp"

#include <iostream>

namespace {

/// Parse a single PV token into a Coord.
/// Handles formats: "7,7"  "(7,7)"  "7,7;"  "(7,7);"
/// Returns nullopt if the token cannot be parsed.
std::optional<util::Coord> parsePVCoord(std::string_view token) {
    // Strip leading whitespace and '('
    while (!token.empty() && (token.front() == ' ' || token.front() == '('))
        token.remove_prefix(1);
    // Strip trailing whitespace, ')', ';'
    while (!token.empty() && (token.back() == ' ' || token.back() == ')' || token.back() == ';'))
        token.remove_suffix(1);

    if (token.empty()) return std::nullopt;

    auto comma = token.find(',');
    if (comma == std::string_view::npos) return std::nullopt;

    auto x = util::parseInt(token.substr(0, comma));
    auto y = util::parseInt(token.substr(comma + 1));

    if (!x.has_value() || !y.has_value()) return std::nullopt;
    return util::Coord{*x, *y};
}

} // anonymous namespace

namespace controller {

AnalysisController::AnalysisController(GameController& gameCtrl)
    : gameCtrl_(gameCtrl) {
}

AnalysisController::~AnalysisController() {
    for (auto& conn : connections_)
        conn.disconnect();
}

void AnalysisController::connectSignals() {
    // BUG-011 FIX: guard against double-connection (e.g. if called after reconnect).
    if (!connections_.empty()) return;

    connections_.push_back(
        gameCtrl_.signalEngineMessage.connect(
            sigc::mem_fun(*this, &AnalysisController::onEngineMessage)));

    // Board changed: clear stale analysis so display stays consistent.
    connections_.push_back(
        gameCtrl_.signalBoardChanged.connect([this]() {
            // Let the next search overwrite info_ naturally.
            // No forced clear here to avoid flicker when analysis panel is visible.
        }));
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
            auto pvPos = line.find("pv ");
            if (pvPos != std::string::npos) {
                mv.pvText = line.substr(pvPos + 3);
            }
            // BUG-009 FIX: use robust parsePVCoord() that handles parentheses, semicolons, etc.
            for (size_t j = i + 1; j < tokens.size(); ++j) {
                auto coord = parsePVCoord(tokens[j]);
                if (coord.has_value()) {
                    mv.pv.push_back(*coord);
                } else {
                    // Log unrecognised token at DEBUG level so failures are visible
                    std::cerr << "[AnalysisController] unrecognised PV token: '"
                              << tokens[j] << "'\n";
                }
            }
            if (!mv.pv.empty()) {
                mv.coord = mv.pv.front();
            }
            break;
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
