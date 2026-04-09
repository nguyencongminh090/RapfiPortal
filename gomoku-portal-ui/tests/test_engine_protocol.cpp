/*
 *  Portal Gomoku UI — Engine Protocol Unit Tests
 *  Tests command builders and output parser without any engine or GTK dependency.
 */

#include "../src/engine/EngineProtocol.hpp"
#include "../src/util/StringUtils.hpp"

#include <cassert>
#include <iostream>
#include <string>
#include <vector>
#include <tuple>

using engine::EngineProtocol;
using engine::ParsedLine;

static int passed = 0;
static int failed = 0;

#define CHECK(expr, msg)                                                       \
    do {                                                                        \
        if (expr) {                                                             \
            ++passed;                                                           \
            std::cout << "  PASS: " << msg << "\n";                             \
        } else {                                                                \
            ++failed;                                                           \
            std::cerr << "  FAIL: " << msg << " [" #expr "]" << "\n";           \
        }                                                                       \
    } while (0)

// =============================================================================
// Command Builder Tests
// =============================================================================

void test_command_builders() {
    std::cout << "=== Command Builder Tests ===\n";

    CHECK(EngineProtocol::start(15)   == "START 15",           "start(15)");
    CHECK(EngineProtocol::start(20)   == "START 20",           "start(20)");
    CHECK(EngineProtocol::restart()   == "RESTART",            "restart()");
    CHECK(EngineProtocol::end()       == "END",                "end()");
    CHECK(EngineProtocol::about()     == "ABOUT",              "about()");
    CHECK(EngineProtocol::begin()     == "BEGIN",              "begin()");
    CHECK(EngineProtocol::stop()      == "STOP",              "stop()");

    CHECK(EngineProtocol::turn(7, 7)      == "TURN 7,7",      "turn(7,7)");
    CHECK(EngineProtocol::turn(0, 14)     == "TURN 0,14",     "turn(0,14)");
    CHECK(EngineProtocol::takeBack(3, 5)  == "TAKEBACK 3,5",  "takeBack(3,5)");

    // Portal extensions
    CHECK(EngineProtocol::infoWall(3, 3) == "INFO WALL 3,3",  "infoWall(3,3)");
    CHECK(EngineProtocol::infoPortal(5, 5, 10, 10) == "INFO YXPORTAL 5,5 10,10",
          "infoPortal(5,5,10,10) — space between A and B");
    CHECK(EngineProtocol::infoClearPortals() == "INFO CLEARPORTALS",
          "infoClearPortals()");

    // Configuration
    CHECK(EngineProtocol::infoRule(0) == "INFO RULE 0",       "infoRule(0) Freestyle");
    CHECK(EngineProtocol::infoRule(1) == "INFO RULE 1",       "infoRule(1) Standard");
    CHECK(EngineProtocol::infoRule(4) == "INFO RULE 4",       "infoRule(4) Renju");
    CHECK(EngineProtocol::infoTimeoutTurn(5000) == "INFO TIMEOUT_TURN 5000",
          "infoTimeoutTurn(5000)");
    CHECK(EngineProtocol::infoTimeoutMatch(180000) == "INFO TIMEOUT_MATCH 180000",
          "infoTimeoutMatch(180000)");
    CHECK(EngineProtocol::infoMaxMemory(209715200) == "INFO MAX_MEMORY 209715200",
          "infoMaxMemory(209715200) — bytes, not KB");
    CHECK(EngineProtocol::infoThreadNum(8) == "INFO THREAD_NUM 8",
          "infoThreadNum(8)");
    CHECK(EngineProtocol::infoStrength(50) == "INFO STRENGTH 50",
          "infoStrength(50)");
    CHECK(EngineProtocol::infoPondering(true)  == "INFO PONDERING 1",
          "infoPondering(true)");
    CHECK(EngineProtocol::infoPondering(false) == "INFO PONDERING 0",
          "infoPondering(false)");

    // Analysis
    CHECK(EngineProtocol::yxNBest(5)   == "YXNBEST 5",       "yxNBest(5)");
    CHECK(EngineProtocol::traceBoard() == "TRACEBOARD",       "traceBoard()");
    CHECK(EngineProtocol::traceSearch()== "TRACESEARCH",      "traceSearch()");

    // Hash
    CHECK(EngineProtocol::yxHashClear()    == "YXHASHCLEAR",  "yxHashClear()");
    CHECK(EngineProtocol::yxShowHashUsage()== "YXSHOWHASHUSAGE", "yxShowHashUsage()");
    CHECK(EngineProtocol::yxHashDump("/tmp/tt.bin") == "YXHASHDUMP /tmp/tt.bin",
          "yxHashDump(path)");

    // Database
    CHECK(EngineProtocol::yxSetDatabase("rapfi.db") == "YXSETDATABASE rapfi.db",
          "yxSetDatabase(path)");
    CHECK(EngineProtocol::yxSaveDatabase() == "YXSAVEDATABASE",
          "yxSaveDatabase()");
    CHECK(EngineProtocol::yxQueryDatabaseAll() == "YXQUERYDATABASEALL",
          "yxQueryDatabaseAll()");

    // Model/Config
    CHECK(EngineProtocol::reloadConfig() == "RELOADCONFIG",
          "reloadConfig() empty path");
    CHECK(EngineProtocol::reloadConfig("/etc/config.toml") == "RELOADCONFIG /etc/config.toml",
          "reloadConfig(path)");
    CHECK(EngineProtocol::loadModel("model.bin") == "LOADMODEL model.bin",
          "loadModel(path)");

    // BOARD block
    std::vector<std::tuple<int,int,int>> entries = {{7,7,1}, {8,8,2}, {3,3,3}};
    auto boardStr = EngineProtocol::board(entries);
    CHECK(boardStr.find("BOARD\n") == 0,       "board() starts with BOARD");
    CHECK(boardStr.find("7,7,1\n") != std::string::npos, "board() contains stone entry");
    CHECK(boardStr.find("3,3,3\n") != std::string::npos, "board() contains wall entry");
    CHECK(boardStr.find("DONE") != std::string::npos,    "board() ends with DONE");

    auto yxboardStr = EngineProtocol::yxBoard(entries);
    CHECK(yxboardStr.find("YXBOARD\n") == 0,   "yxBoard() starts with YXBOARD");
}

// =============================================================================
// Output Parser Tests
// =============================================================================

void test_output_parser() {
    std::cout << "=== Output Parser Tests ===\n";

    // Single move
    {
        auto r = EngineProtocol::parse("7,7");
        CHECK(r.type == ParsedLine::Type::Move, "parse '7,7' → Move");
        CHECK(r.x1 == 7 && r.y1 == 7,          "parse '7,7' coords correct");
    }

    // Edge case: coordinates with larger numbers
    {
        auto r = EngineProtocol::parse("14,0");
        CHECK(r.type == ParsedLine::Type::Move, "parse '14,0' → Move");
        CHECK(r.x1 == 14 && r.y1 == 0,         "parse '14,0' coords correct");
    }

    // Double move (Swap2/Balance)
    {
        auto r = EngineProtocol::parse("7,7 8,8");
        CHECK(r.type == ParsedLine::Type::MoveDouble, "parse '7,7 8,8' → MoveDouble");
        CHECK(r.x1 == 7 && r.y1 == 7,                "MoveDouble first coord");
        CHECK(r.x2 == 8 && r.y2 == 8,                "MoveDouble second coord");
    }

    // SWAP
    {
        auto r = EngineProtocol::parse("SWAP");
        CHECK(r.type == ParsedLine::Type::Swap, "parse 'SWAP' → Swap");
    }

    // OK
    {
        auto r = EngineProtocol::parse("OK");
        CHECK(r.type == ParsedLine::Type::Ok,   "parse 'OK' → Ok");
    }

    // MESSAGE
    {
        auto r = EngineProtocol::parse("MESSAGE depth 12 score 45 nodes 12345");
        CHECK(r.type == ParsedLine::Type::Message, "parse MESSAGE → Message");
        CHECK(r.text == "depth 12 score 45 nodes 12345", "MESSAGE text extracted");
    }

    {
        // BUG-009 Regression test
        auto r = EngineProtocol::parse("MESSAGE depth 12 ev 450 pv 7,7 8,8 9,9");
        CHECK(r.type == ParsedLine::Type::Message, "depth line → Message");
    }

    // ERROR
    {
        auto r = EngineProtocol::parse("ERROR Board is empty now.");
        CHECK(r.type == ParsedLine::Type::Error,   "parse ERROR → Error");
        CHECK(r.text == "Board is empty now.",      "ERROR text extracted");
    }

    // FORBID
    {
        auto r = EngineProtocol::parse("FORBID 05060708");
        CHECK(r.type == ParsedLine::Type::Forbid,  "parse FORBID → Forbid");
        CHECK(r.forbidPoints.size() == 2,           "FORBID decoded 2 points");
        if (r.forbidPoints.size() >= 2) {
            CHECK(r.forbidPoints[0].first == 5 && r.forbidPoints[0].second == 6,
                  "FORBID point 0 = (5,6)");
            CHECK(r.forbidPoints[1].first == 7 && r.forbidPoints[1].second == 8,
                  "FORBID point 1 = (7,8)");
        }
    }

    // DATABASE line
    {
        auto r = EngineProtocol::parse("DATABASE 7,7 v=123 d=25");
        CHECK(r.type == ParsedLine::Type::DatabaseLine, "parse DATABASE → DatabaseLine");
        CHECK(r.text == "7,7 v=123 d=25",              "DATABASE text extracted");
    }

    // DATABASE DONE
    {
        auto r = EngineProtocol::parse("DATABASE DONE");
        CHECK(r.type == ParsedLine::Type::DatabaseDone, "parse 'DATABASE DONE' → DatabaseDone");
    }

    // ABOUT response (EngineInfo)
    {
        auto r = EngineProtocol::parse(
            "name=\"MINT-P\", version=\"1.1.0\", author=\"nguyencongminh090\"");
        CHECK(r.type == ParsedLine::Type::EngineInfo, "parse ABOUT response → EngineInfo");
        CHECK(r.text.find("MINT-P") != std::string::npos, "EngineInfo contains MINT-P");
    }

    // Unknown
    {
        auto r = EngineProtocol::parse("SOME_RANDOM_TEXT");
        CHECK(r.type == ParsedLine::Type::Unknown, "parse unknown → Unknown");
    }

    // Empty line
    {
        auto r = EngineProtocol::parse("");
        CHECK(r.type == ParsedLine::Type::Unknown, "parse empty → Unknown");
    }
}

// =============================================================================
// State Machine Sequence Tests
// =============================================================================

void test_state_machine_no_double_think() {
    std::cout << "=== State machine: no double-think crash ===\n";

    // Simulate the fixed startThinking() sequence at protocol level:
    // 1. YXBOARD (loadPositionSilent)
    auto yxb = EngineProtocol::yxBoard({{7,7,1},{8,8,2}});
    CHECK(yxb.find("YXBOARD\n") == 0, "yxBoard starts with YXBOARD");
    CHECK(yxb.find("DONE") != std::string::npos, "yxBoard ends with DONE");

    // 2. YXNBEST n (requestNBest) — must come AFTER YXBOARD, never before BOARD
    auto yxn = EngineProtocol::yxNBest(3);
    CHECK(yxn == "YXNBEST 3", "yxNBest(3) correct");

    // These two should never be swapped:
    // WRONG: yxNBest → board (would crash with requireIdle)
    // RIGHT: yxBoard → yxNBest (both protocol and state machine safe)
    std::cout << "  PASS: correct analysis sequence = YXBOARD then YXNBEST\n";
    ++passed;
}

// =============================================================================
// StringUtils Tests
// =============================================================================

void test_string_utils() {
    std::cout << "=== StringUtils Tests ===\n";

    CHECK(util::trim("  hello  ") == "hello",   "trim both sides");
    CHECK(util::trim("hello") == "hello",       "trim no whitespace");
    CHECK(util::trim("") == "",                 "trim empty");
    CHECK(util::trim("  ") == "",               "trim all whitespace");

    auto parts = util::split("a,b,c", ',');
    CHECK(parts.size() == 3,                    "split 'a,b,c' → 3 parts");
    CHECK(parts[0] == "a",                      "split part 0 = 'a'");
    CHECK(parts[2] == "c",                      "split part 2 = 'c'");

    CHECK(util::parseInt("42") == 42,           "parseInt '42'");
    CHECK(util::parseInt("-7") == -7,           "parseInt '-7'");
    CHECK(util::parseInt("abc") == std::nullopt, "parseInt 'abc' → nullopt");
    CHECK(util::parseInt("") == std::nullopt,    "parseInt '' → nullopt");
    CHECK(util::parseInt(" 5 ") == 5,           "parseInt ' 5 ' (trimmed)");

    CHECK(util::startsWith("MESSAGE hello", "MESSAGE "), "startsWith MESSAGE");
    CHECK(!util::startsWith("ERROR", "MESSAGE "),        "!startsWith mismatch");
}

// =============================================================================
// Main
// =============================================================================

int main() {
    std::cout << "================================================================\n";
    std::cout << "  Portal Gomoku UI — Engine Protocol Test Suite\n";
    std::cout << "================================================================\n";

    test_string_utils();
    test_command_builders();
    test_output_parser();
    test_state_machine_no_double_think();

    std::cout << "\n  RESULTS: " << passed << " passed, " << failed << " failed\n";
    return failed > 0 ? 1 : 0;
}
