#include "OBFManager.hpp"
#include <fstream>
#include <iostream>

namespace model {

bool OBFManager::appendOpening(const std::string& filepath, const GameRecord& record) {
    std::fstream file(filepath, std::ios::in | std::ios::out | std::ios::binary);

    if (!file.is_open()) {
        // File doesn't exist, create it with header
        file.clear(); // Clear error state if any
        file.open(filepath, std::ios::out | std::ios::binary);
        if (!file.is_open()) return false;
        
        file.write(reinterpret_cast<const char*>(&MAGIC), sizeof(MAGIC));
        file.write(reinterpret_cast<const char*>(&VERSION), sizeof(VERSION));
    } else {
        // File exists, verify magic and version
        uint32_t magic;
        file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
        if (magic != MAGIC) return false;
        
        uint32_t version;
        file.read(reinterpret_cast<char*>(&version), sizeof(version));
        if (version != VERSION) return false;
        
        // Seek to end to append
        file.seekp(0, std::ios::end);
    }
    
    // Write out the record metadata
    uint8_t boardSize = static_cast<uint8_t>(record.boardSize);
    uint16_t moveCount = static_cast<uint16_t>(record.moves.size());
    uint16_t wallCount = static_cast<uint16_t>(record.topology.walls().size());
    uint16_t portalCount = static_cast<uint16_t>(record.topology.portals().size());
    
    file.write(reinterpret_cast<const char*>(&boardSize), sizeof(boardSize));
    file.write(reinterpret_cast<const char*>(&moveCount), sizeof(moveCount));
    file.write(reinterpret_cast<const char*>(&wallCount), sizeof(wallCount));
    file.write(reinterpret_cast<const char*>(&portalCount), sizeof(portalCount));
    
    // Write Moves
    for (const auto& m : record.moves) {
        int8_t x = static_cast<int8_t>(m.coord.x);
        int8_t y = static_cast<int8_t>(m.coord.y);
        uint8_t color = (m.color == Color::Black) ? 1 : 2; // Black=1, White=2
        file.write(reinterpret_cast<const char*>(&x), sizeof(x));
        file.write(reinterpret_cast<const char*>(&y), sizeof(y));
        file.write(reinterpret_cast<const char*>(&color), sizeof(color));
    }
    
    // Write Walls
    for (const auto& w : record.topology.walls()) {
        int8_t x = static_cast<int8_t>(w.x);
        int8_t y = static_cast<int8_t>(w.y);
        file.write(reinterpret_cast<const char*>(&x), sizeof(x));
        file.write(reinterpret_cast<const char*>(&y), sizeof(y));
    }
    
    // Write Portals
    for (const auto& p : record.topology.portals()) {
        int8_t ax = static_cast<int8_t>(p.a.x);
        int8_t ay = static_cast<int8_t>(p.a.y);
        int8_t bx = static_cast<int8_t>(p.b.x);
        int8_t by = static_cast<int8_t>(p.b.y);
        file.write(reinterpret_cast<const char*>(&ax), sizeof(ax));
        file.write(reinterpret_cast<const char*>(&ay), sizeof(ay));
        file.write(reinterpret_cast<const char*>(&bx), sizeof(bx));
        file.write(reinterpret_cast<const char*>(&by), sizeof(by));
    }

    return true;
}

std::vector<GameRecord> OBFManager::readOpenings(const std::string& filepath) {
    std::vector<GameRecord> openings;
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return openings;

    uint32_t magic = 0;
    file.read(reinterpret_cast<char*>(&magic), sizeof(magic));
    if (magic != MAGIC) return openings;
    
    uint32_t version = 0;
    file.read(reinterpret_cast<char*>(&version), sizeof(version));
    if (version != VERSION) return openings;

    while (file.peek() != EOF) {
        GameRecord record;
        record.rule = 0; 
        
        uint8_t boardSize = 0;
        uint16_t moveCount = 0, wallCount = 0, portalCount = 0;
        
        file.read(reinterpret_cast<char*>(&boardSize), sizeof(boardSize));
        if (file.gcount() == 0) break; // EOF check

        file.read(reinterpret_cast<char*>(&moveCount), sizeof(moveCount));
        file.read(reinterpret_cast<char*>(&wallCount), sizeof(wallCount));
        file.read(reinterpret_cast<char*>(&portalCount), sizeof(portalCount));
        
        record.boardSize = boardSize;

        for (uint16_t i = 0; i < moveCount; ++i) {
            int8_t x, y;
            uint8_t color;
            file.read(reinterpret_cast<char*>(&x), sizeof(x));
            file.read(reinterpret_cast<char*>(&y), sizeof(y));
            file.read(reinterpret_cast<char*>(&color), sizeof(color));
            
            Move m;
            m.coord = util::Coord(x, y);
            m.color = (color == 1) ? Color::Black : Color::White;
            m.ply = static_cast<int>(i);
            record.moves.push_back(m);
        }

        for (uint16_t i = 0; i < wallCount; ++i) {
            int8_t x, y;
            file.read(reinterpret_cast<char*>(&x), sizeof(x));
            file.read(reinterpret_cast<char*>(&y), sizeof(y));
            record.topology.addWall(util::Coord(x, y));
        }

        for (uint16_t i = 0; i < portalCount; ++i) {
            int8_t ax, ay, bx, by;
            file.read(reinterpret_cast<char*>(&ax), sizeof(ax));
            file.read(reinterpret_cast<char*>(&ay), sizeof(ay));
            file.read(reinterpret_cast<char*>(&bx), sizeof(bx));
            file.read(reinterpret_cast<char*>(&by), sizeof(by));
            record.topology.addPortal(util::Coord(ax, ay), util::Coord(bx, by));
        }

        openings.push_back(record);
    }

    return openings;
}

} // namespace model
