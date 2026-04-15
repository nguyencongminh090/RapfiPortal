/*
 *  Portal Gomoku UI — Engine Protocol
 *  Stateless command builder and output parser.
 *  No GTK dependency. Fully unit-testable.
 *
 *  Reference: portal_src/protocol.md
 */

#pragma once

#include <string>
#include <vector>
#include <utility>

namespace engine {

/// Parsed result of one line of engine stdout output.
struct ParsedLine {
    enum class Type {
        Move,           ///< Single move: "7,7"
        MoveDouble,     ///< Double move: "7,7 8,8" (Swap2/Balance)
        Swap,           ///< "SWAP" response
        Ok,             ///< "OK" (RESTART / TAKEBACK acknowledgement)
        Message,        ///< "MESSAGE ..." log line
        Error,          ///< "ERROR ..." error line
        Forbid,         ///< "FORBID 0506..." forbidden points
        DatabaseLine,   ///< "DATABASE ..." query result line
        DatabaseDone,   ///< "DATABASE DONE" end marker
        EngineInfo,     ///< ABOUT response: name="...", version="..."
        Unknown         ///< Unrecognized line
    };

    Type        type = Type::Unknown;
    int         x1 = -1, y1 = -1;      ///< First move coords (Move, MoveDouble)
    int         x2 = -1, y2 = -1;      ///< Second move coords (MoveDouble only)
    std::string text;                   ///< Raw text for Message/Error/Database/EngineInfo
    std::vector<std::pair<int,int>> forbidPoints;  ///< Decoded forbidden cell list
};

/// Stateless protocol command builder and output parser.
/// All methods are pure functions — no state, no I/O, no side effects.
class EngineProtocol {
public:
    // =========================================================================
    // Command Builders — Session Lifecycle
    // =========================================================================

    /// "START {boardSize}"
    static std::string start(int boardSize);

    /// "RESTART"
    static std::string restart();

    /// "END"
    static std::string end();

    /// "ABOUT"
    static std::string about();


    // =========================================================================
    // Command Builders — Gameplay
    // =========================================================================

    /// "BEGIN" — engine plays first move
    static std::string begin();

    /// "TURN {x},{y}" — human plays at (x,y), engine thinks
    static std::string turn(int x, int y);

    /// "TAKEBACK {x},{y}" — undo one ply (coords are sent but ignored by engine)
    static std::string takeBack(int x, int y);

    /// "STOP" — interrupt engine thinking
    static std::string stop();

    /// Build a BOARD block: "BOARD\n{x},{y},{color}\n...\nDONE"
    /// entries: vector of {x, y, color} where color: 1=SELF, 2=OPPO, 3=WALL
    static std::string board(const std::vector<std::tuple<int,int,int>>& entries);

    /// Build a YXBOARD block (same format, engine does NOT auto-think)
    static std::string yxBoard(const std::vector<std::tuple<int,int,int>>& entries);

    // =========================================================================
    // Command Builders — Portal Extensions
    // =========================================================================

    /// "INFO WALL {x},{y}"
    static std::string infoWall(int x, int y);

    /// "INFO YXPORTAL {ax},{ay} {bx},{by}" — note: space between A and B
    static std::string infoPortal(int ax, int ay, int bx, int by);

    /// "INFO CLEARPORTALS"
    static std::string infoClearPortals();

    // =========================================================================
    // Command Builders — Configuration
    // =========================================================================

    /// "INFO RULE {rule}" — 0=Freestyle, 1=Standard, 2/4=Renju, 5=Swap1, 6=Swap2
    static std::string infoRule(int rule);

    /// "INFO TIMEOUT_TURN {ms}"
    static std::string infoTimeoutTurn(int ms);

    /// "INFO TIMEOUT_MATCH {ms}"
    static std::string infoTimeoutMatch(int ms);

    /// "INFO MAX_MEMORY {bytes}" — NOTE: engine expects bytes, not KB
    static std::string infoMaxMemory(size_t bytes);

    /// "INFO THREAD_NUM {n}"
    static std::string infoThreadNum(int n);

    /// "INFO STRENGTH {level}" — 0..100
    static std::string infoStrength(int level);

    /// "INFO MAX_DEPTH {depth}"
    static std::string infoMaxDepth(int depth);

    /// "INFO PONDERING {0|1}"
    static std::string infoPondering(bool enable);


    /// "INFO MAX_NODE {n}" — ULLONG_MAX for unlimited
    static std::string infoMaxNode(unsigned long long n);

    // =========================================================================
    // Command Builders — Analysis
    // =========================================================================

    /// "YXNBEST {n}" — request N-best PV lines
    static std::string yxNBest(int n);

    /// "YXPLAYDIST {n}" — engine plays a move with at least N Chebyshev distance
    static std::string yxPlayDist(int n);

    /// "YXPLAYSELF {n}" — engine plays a move with at least N Chebyshev distance from SELF stones
    static std::string yxPlaySelfDist(int n);

    /// "TRACEBOARD" — dump board patterns/scores
    static std::string traceBoard();

    /// "TRACESEARCH" — dump search state
    static std::string traceSearch();

    // =========================================================================
    // Command Builders — Hash Table
    // =========================================================================

    static std::string yxHashClear();
    static std::string yxShowHashUsage();
    static std::string yxHashDump(const std::string& path);
    static std::string yxHashLoad(const std::string& path);

    // =========================================================================
    // Command Builders — Database
    // =========================================================================

    static std::string yxSetDatabase(const std::string& path);
    static std::string yxSaveDatabase();
    static std::string yxQueryDatabaseAll();
    static std::string yxQueryDatabaseOne();
    static std::string yxQueryDatabaseText();
    static std::string yxDeleteDatabaseOne();
    static std::string yxDeleteDatabaseAll();

    // =========================================================================
    // Command Builders — Model / Config
    // =========================================================================

    /// "RELOADCONFIG {path}" — empty path = reload internal config
    static std::string reloadConfig(const std::string& path = "");

    /// "LOADMODEL {path}"
    static std::string loadModel(const std::string& path);

    // =========================================================================
    // Output Parser
    // =========================================================================

    /// Parse a single line of engine stdout output.
    /// The line should NOT contain the trailing newline.
    static ParsedLine parse(const std::string& line);

private:
    /// Build a BOARD/YXBOARD block with the given header command.
    static std::string buildBoardBlock(const char* header,
                                       const std::vector<std::tuple<int,int,int>>& entries);

    /// Try to parse "x,y" from a string. Returns {x, y, true} on success.
    static std::tuple<int, int, bool> parseCoordPair(const std::string& s);
};

}  // namespace engine
