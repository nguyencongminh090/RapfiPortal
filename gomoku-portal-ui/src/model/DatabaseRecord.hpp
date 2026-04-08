/*
 *  Portal Gomoku UI — Database Record
 *  Value object for database query results from engine.
 */

#pragma once

#include "../util/Coord.hpp"

#include <string>

namespace model {

/// Represents a single database query result entry.
struct DatabaseRecord {
    util::Coord coord;          ///< Move position this record refers to
    int         value   = 0;    ///< Evaluation value
    int         depth   = 0;    ///< Search depth
    int         type    = 0;    ///< Record type mask
    std::string label;          ///< Board label / comment text
    std::string rawText;        ///< Raw DATABASE line from engine
};

}  // namespace model
