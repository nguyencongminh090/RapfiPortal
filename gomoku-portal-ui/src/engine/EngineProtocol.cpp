/*
 *  Portal Gomoku UI — Engine Protocol Implementation
 *  Stateless command builder and output parser.
 */

#include "EngineProtocol.hpp"
#include "../util/StringUtils.hpp"

#include <sstream>
#include <algorithm>
#include <cctype>
#include "../util/Coord.hpp"

namespace engine {

// =============================================================================
// Command Builders — Session Lifecycle
// =============================================================================

std::string EngineProtocol::start(int boardSize) {
    return "START " + std::to_string(boardSize);
}

std::string EngineProtocol::restart() { return "RESTART"; }
std::string EngineProtocol::end()     { return "END"; }
std::string EngineProtocol::about()   { return "ABOUT"; }

// =============================================================================
// Command Builders — Gameplay
// =============================================================================

std::string EngineProtocol::begin() { return "BEGIN"; }

std::string EngineProtocol::turn(int x, int y) {
    return "TURN " + std::to_string(x) + "," + std::to_string(y);
}

std::string EngineProtocol::takeBack(int x, int y) {
    return "TAKEBACK " + std::to_string(x) + "," + std::to_string(y);
}

std::string EngineProtocol::stop() { return "STOP"; }

std::string EngineProtocol::buildBoardBlock(
    const char* header,
    const std::vector<std::tuple<int,int,int>>& entries)
{
    std::ostringstream ss;
    ss << header << "\n";
    for (auto& [x, y, color] : entries)
        ss << x << "," << y << "," << color << "\n";
    ss << "DONE";
    return ss.str();
}

std::string EngineProtocol::board(const std::vector<std::tuple<int,int,int>>& entries) {
    return buildBoardBlock("BOARD", entries);
}

std::string EngineProtocol::yxBoard(const std::vector<std::tuple<int,int,int>>& entries) {
    return buildBoardBlock("YXBOARD", entries);
}

// =============================================================================
// Command Builders — Portal Extensions
// =============================================================================

std::string EngineProtocol::infoWall(int x, int y) {
    return "INFO WALL " + std::to_string(x) + "," + std::to_string(y);
}

std::string EngineProtocol::infoPortal(int ax, int ay, int bx, int by) {
    // Note: space separator between A and B coordinates
    return "INFO YXPORTAL " + std::to_string(ax) + "," + std::to_string(ay)
         + " " + std::to_string(bx) + "," + std::to_string(by);
}

std::string EngineProtocol::infoClearPortals() { return "INFO CLEARPORTALS"; }

// =============================================================================
// Command Builders — Configuration
// =============================================================================

std::string EngineProtocol::infoRule(int rule) {
    return "INFO RULE " + std::to_string(rule);
}

std::string EngineProtocol::infoTimeoutTurn(int ms) {
    return "INFO TIMEOUT_TURN " + std::to_string(ms);
}

std::string EngineProtocol::infoTimeoutMatch(int ms) {
    return "INFO TIMEOUT_MATCH " + std::to_string(ms);
}

std::string EngineProtocol::infoMaxMemory(size_t bytes) {
    return "INFO MAX_MEMORY " + std::to_string(bytes);
}

std::string EngineProtocol::infoThreadNum(int n) {
    return "INFO THREAD_NUM " + std::to_string(n);
}

std::string EngineProtocol::infoStrength(int level) {
    return "INFO STRENGTH " + std::to_string(level);
}

std::string EngineProtocol::infoMaxDepth(int depth) {
    return "INFO MAX_DEPTH " + std::to_string(depth);
}

std::string EngineProtocol::infoPondering(bool enable) {
    return std::string("INFO PONDERING ") + (enable ? "1" : "0");
}


std::string EngineProtocol::infoMaxNode(unsigned long long n) {
    return "INFO MAX_NODE " + std::to_string(n);
}

// =============================================================================
// Command Builders — Analysis
// =============================================================================

std::string EngineProtocol::yxNBest(int n) {
    return "YXNBEST " + std::to_string(n);
}

std::string EngineProtocol::yxPlayDist(int n) {
    return "YXPLAYDIST " + std::to_string(n);
}

std::string EngineProtocol::yxPlaySelfDist(int n) {
    return "YXPLAYSELF " + std::to_string(n);
}

std::string EngineProtocol::traceBoard() { return "TRACEBOARD"; }
std::string EngineProtocol::traceSearch() { return "TRACESEARCH"; }

// =============================================================================
// Command Builders — Hash Table
// =============================================================================

std::string EngineProtocol::yxHashClear()      { return "YXHASHCLEAR"; }
std::string EngineProtocol::yxShowHashUsage()   { return "YXSHOWHASHUSAGE"; }

std::string EngineProtocol::yxHashDump(const std::string& path) {
    return "YXHASHDUMP " + path;
}

std::string EngineProtocol::yxHashLoad(const std::string& path) {
    return "YXHASHLOAD " + path;
}

// =============================================================================
// Command Builders — Database
// =============================================================================

std::string EngineProtocol::yxSetDatabase(const std::string& path) {
    return "YXSETDATABASE " + path;
}

std::string EngineProtocol::yxSaveDatabase()      { return "YXSAVEDATABASE"; }
std::string EngineProtocol::yxQueryDatabaseAll()   { return "YXQUERYDATABASEALL"; }
std::string EngineProtocol::yxQueryDatabaseOne()   { return "YXQUERYDATABASEONE"; }
std::string EngineProtocol::yxQueryDatabaseText()  { return "YXQUERYDATABASETEXT"; }
std::string EngineProtocol::yxDeleteDatabaseOne()  { return "YXDELETEDATABASEONE"; }
std::string EngineProtocol::yxDeleteDatabaseAll()  { return "YXDELETEDATABASEALL"; }

// =============================================================================
// Command Builders — Model / Config
// =============================================================================

std::string EngineProtocol::reloadConfig(const std::string& path) {
    return path.empty() ? "RELOADCONFIG" : ("RELOADCONFIG " + path);
}

std::string EngineProtocol::loadModel(const std::string& path) {
    return "LOADMODEL " + path;
}

// =============================================================================
// Output Parser
// =============================================================================

std::tuple<int, int, bool> EngineProtocol::parseCoordPair(const std::string& s) {
    auto c = util::Coord::parse(s);
    if (c) return {c->x, c->y, true};
    return {-1, -1, false};
}

ParsedLine EngineProtocol::parse(const std::string& line) {
    ParsedLine result;

    if (line.empty()) {
        result.type = ParsedLine::Type::Unknown;
        return result;
    }

    // "OK"
    if (line == "OK") {
        result.type = ParsedLine::Type::Ok;
        return result;
    }

    // "SWAP"
    if (line == "SWAP") {
        result.type = ParsedLine::Type::Swap;
        return result;
    }

    // "MESSAGE ..."
    if (util::startsWith(line, "MESSAGE ")) {
        result.type = ParsedLine::Type::Message;
        result.text = line.substr(8);
        return result;
    }

    // "ERROR ..."
    if (util::startsWith(line, "ERROR ")) {
        result.type = ParsedLine::Type::Error;
        result.text = line.substr(6);
        return result;
    }

    // "FORBID xxyy..."
    if (util::startsWith(line, "FORBID ")) {
        result.type = ParsedLine::Type::Forbid;
        result.text = line.substr(7);
        // Decode zero-padded 2-digit coordinate pairs
        auto& coords = result.text;
        for (size_t i = 0; i + 3 < coords.size(); i += 4) {
            auto xOpt = util::parseInt(coords.substr(i, 2));
            auto yOpt = util::parseInt(coords.substr(i + 2, 2));
            if (xOpt && yOpt)
                result.forbidPoints.emplace_back(*xOpt, *yOpt);
        }
        return result;
    }

    // "DATABASE DONE"
    if (line == "DATABASE DONE") {
        result.type = ParsedLine::Type::DatabaseDone;
        return result;
    }

    // "DATABASE ..."
    if (util::startsWith(line, "DATABASE ")) {
        result.type = ParsedLine::Type::DatabaseLine;
        result.text = line.substr(9);
        return result;
    }

    // Try to parse as coordinate(s)
    // Check for double move: "x1,y1 x2,y2"
    auto spacePos = line.find(' ');
    if (spacePos != std::string::npos) {
        auto [x1, y1, ok1] = parseCoordPair(line.substr(0, spacePos));
        auto [x2, y2, ok2] = parseCoordPair(line.substr(spacePos + 1));
        if (ok1 && ok2) {
            result.type = ParsedLine::Type::MoveDouble;
            result.x1 = x1; result.y1 = y1;
            result.x2 = x2; result.y2 = y2;
            return result;
        }
    }

    // Try single move: "x,y"
    auto [x, y, ok] = parseCoordPair(line);
    if (ok) {
        result.type = ParsedLine::Type::Move;
        result.x1 = x; result.y1 = y;
        return result;
    }

    // Check for ABOUT response (contains "name=")
    if (line.find("name=") != std::string::npos) {
        result.type = ParsedLine::Type::EngineInfo;
        result.text = line;
        return result;
    }

    // Unknown
    result.type = ParsedLine::Type::Unknown;
    result.text = line;
    return result;
}

}  // namespace engine
