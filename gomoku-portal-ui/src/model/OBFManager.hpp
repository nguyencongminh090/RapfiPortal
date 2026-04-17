/*
 *  Portal Gomoku UI — Opening Binary Format (OBF) Manager
 *  Handles serialization and deserialization of .obf opening files.
 */

#pragma once

#include "GameRecord.hpp"
#include <string>
#include <vector>
#include <cstdint>

namespace model {

class OBFManager {
public:
    static constexpr uint32_t MAGIC = 0x3146424F; // "OBF1" in little-endian ASCII
    static constexpr uint32_t VERSION = 1;

    /// Read all openings from an OBF file.
    /// Returns an empty vector if the file does not exist or format is invalid.
    [[nodiscard]] static std::vector<GameRecord> readOpenings(const std::string& filepath);

    /// Append a single game record to an existing OBF file, or create a new one.
    /// Returns true on success.
    static bool appendOpening(const std::string& filepath, const GameRecord& record);
};

} // namespace model
