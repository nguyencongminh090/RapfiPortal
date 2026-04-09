/*
 *  Portal Gomoku UI — Database Controller Implementation
 */

#include "DatabaseController.hpp"
#include "../util/StringUtils.hpp"
#include <iostream>

namespace controller {

DatabaseController::DatabaseController(engine::EngineController& engineCtrl)
    : engineCtrl_(engineCtrl) {

    // Subscribe to engine database signals
    connections_.push_back(
        engineCtrl_.signalDatabaseLine.connect(
            sigc::mem_fun(*this, &DatabaseController::onDatabaseLine))
    );
    
    connections_.push_back(
        engineCtrl_.signalDatabaseDone.connect(
            sigc::mem_fun(*this, &DatabaseController::onDatabaseDone))
    );
}

DatabaseController::~DatabaseController() {
    for (auto& conn : connections_) {
        conn.disconnect();
    }
}

void DatabaseController::requestDatabaseAll() {
    isQuerying_ = true;
    currentResults_.clear();
    engineCtrl_.queryDatabaseAll();
}

void DatabaseController::requestDatabaseOne() {
    isQuerying_ = true;
    currentResults_.clear();
    engineCtrl_.queryDatabaseOne();
}

void DatabaseController::requestDatabaseText() {
    isQuerying_ = true;
    currentResults_.clear();
    engineCtrl_.queryDatabaseText();
}

void DatabaseController::deleteDatabaseOne() {
    engineCtrl_.deleteDatabaseOne();
}

void DatabaseController::deleteDatabaseAll() {
    engineCtrl_.deleteDatabaseAll();
}

model::DatabaseRecord DatabaseController::parseRecord(const std::string& text) {
    model::DatabaseRecord rec;
    rec.rawText = text;

    auto parts = util::split(text, ' ');
    if (parts.empty()) return rec;

    // Parts[0] should be x,y
    auto coords = util::split(parts[0], ',');
    if (coords.size() == 2) {
        auto x = util::parseInt(coords[0]);
        auto y = util::parseInt(coords[1]);
        // BUG-012 FIX: use has_value() explicitly.
        // "if (x && y)" looks like an integer zero-check but x/y are optional<int>;
        // x=optional(0) IS truthy, so the old code was correct — this change is
        // purely for maintainability to prevent future misreading of the guard.
        if (x.has_value() && y.has_value()) {
            rec.coord = {*x, *y};
        }
    }

    // Process other key=value pairs or pure labels
    std::string labelText = "";
    for (size_t i = 1; i < parts.size(); ++i) {
        auto p = parts[i];
        if (util::startsWith(p, "v=")) {
            if (auto v = util::parseInt(p.substr(2))) rec.value = *v;
        } else if (util::startsWith(p, "d=")) {
            if (auto d = util::parseInt(p.substr(2))) rec.depth = *d;
        } else if (util::startsWith(p, "t=")) {
            if (auto t = util::parseInt(p.substr(2))) rec.type = *t;
        } else {
            // Unrecognized part, append to label
            if (!labelText.empty()) labelText += " ";
            labelText += p;
        }
    }
    
    rec.label = labelText;
    return rec;
}

void DatabaseController::onDatabaseLine(const std::string& text) {
    if (!isQuerying_) return;
    currentResults_.push_back(parseRecord(text));
}

void DatabaseController::onDatabaseDone() {
    if (!isQuerying_) return;
    isQuerying_ = false;
    signalDatabaseResultsReady.emit(currentResults_);
}

} // namespace controller
