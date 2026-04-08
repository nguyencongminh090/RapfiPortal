/*
 *  Portal Gomoku UI — Database Controller
 *  Handles sending database requests and accumulating results.
 */

#pragma once

#include "../engine/EngineController.hpp"
#include "../model/DatabaseRecord.hpp"

#include <sigc++/sigc++.h>
#include <vector>
#include <string>

namespace controller {

class DatabaseController {
public:
    explicit DatabaseController(engine::EngineController& engineCtrl);
    ~DatabaseController();

    /// Request all database entries for the current board state
    void requestDatabaseAll();

    /// Request the database entry for the exact current board state
    void requestDatabaseOne();

    /// Request a textual search query
    void requestDatabaseText();

    /// Delete exact current board state from DB
    void deleteDatabaseOne();

    /// Clear all entries from DB
    void deleteDatabaseAll();

    /// Signal emitted when a database query finishes, passing the list of records
    sigc::signal<void(const std::vector<model::DatabaseRecord>&)> signalDatabaseResultsReady;

private:
    engine::EngineController& engineCtrl_;
    std::vector<model::DatabaseRecord> currentResults_;
    bool isQuerying_ = false;

    // Connections to clean up on destruction
    std::vector<sigc::connection> connections_;

    void onDatabaseLine(const std::string& text);
    void onDatabaseDone();
    
    /// Basic parser for "DATABASE x,y v=123 d=25 type=1 label" string
    model::DatabaseRecord parseRecord(const std::string& text);
};

} // namespace controller
