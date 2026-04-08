/*
 *  Portal Gomoku UI — Engine Controller Implementation
 */

#include "EngineController.hpp"

#include <iostream>
#include <stdexcept>

namespace engine {

EngineController::EngineController() {
    // Set the line callback on the process
    process_.setLineCallback([this](const std::string& line) {
        onLineReceived(line);
    });
}

EngineController::~EngineController() {
    if (state_ != EngineState::Disconnected) {
        disconnect();
    }
}

// =============================================================================
// Lifecycle
// =============================================================================

bool EngineController::connect(const EngineConfig& config) {
    if (state_ != EngineState::Disconnected) {
        disconnect();
    }

    currentConfig_ = config;

    if (!process_.launch(config.executablePath, config.workingDir)) {
        return false;
    }

    setState(EngineState::Idle);

    // Initialize GUI mode — must be first command
    send(EngineProtocol::yxShowInfo());

    return true;
}

void EngineController::disconnect() {
    if (state_ == EngineState::Disconnected) return;

    // Try graceful shutdown
    if (process_.isAlive()) {
        try { send(EngineProtocol::end()); } catch (...) {}
    }

    process_.kill();
    setState(EngineState::Disconnected);
}

// =============================================================================
// Game Commands
// =============================================================================

void EngineController::startGame(
    const EngineConfig& config,
    const std::vector<std::pair<int,int>>& walls,
    const std::vector<std::tuple<int,int,int,int>>& portals)
{
    requireIdle("startGame");

    currentConfig_ = config;

    send(EngineProtocol::start(config.boardSize));
    // Engine replies OK via restart() internally — we'll get it in pollOutput

    send(EngineProtocol::infoClearPortals());

    // Add walls
    for (auto [x, y] : walls)
        send(EngineProtocol::infoWall(x, y));

    // Add portals
    for (auto [ax, ay, bx, by] : portals)
        send(EngineProtocol::infoPortal(ax, ay, bx, by));

    // Apply rule and config
    send(EngineProtocol::infoRule(config.rule));
    applyConfig(config);
}

void EngineController::restartGame() {
    requireIdle("restartGame");
    send(EngineProtocol::restart());
}

void EngineController::makeMove(int x, int y) {
    requireIdle("makeMove");
    send(EngineProtocol::turn(x, y));
    setState(EngineState::Thinking);
}

void EngineController::requestEngineMove() {
    requireIdle("requestEngineMove");
    send(EngineProtocol::begin());
    setState(EngineState::Thinking);
}

void EngineController::takeBack(int x, int y) {
    requireIdle("takeBack");
    send(EngineProtocol::takeBack(x, y));
}

void EngineController::loadPosition(
    const std::vector<std::tuple<int,int,int>>& entries)
{
    requireIdle("loadPosition");
    // BOARD block — each line sent individually
    send("BOARD");
    for (auto& [x, y, color] : entries)
        send(std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(color));
    send("DONE");
    setState(EngineState::Thinking);  // BOARD starts thinking
}

void EngineController::loadPositionSilent(
    const std::vector<std::tuple<int,int,int>>& entries)
{
    requireIdle("loadPositionSilent");
    send("YXBOARD");
    for (auto& [x, y, color] : entries)
        send(std::to_string(x) + "," + std::to_string(y) + "," + std::to_string(color));
    send("DONE");
    // YXBOARD does NOT start thinking — stay IDLE
}

// =============================================================================
// Portal Setup
// =============================================================================

void EngineController::addWall(int x, int y) {
    requireIdle("addWall");
    send(EngineProtocol::infoWall(x, y));
}

void EngineController::addPortal(int ax, int ay, int bx, int by) {
    requireIdle("addPortal");
    send(EngineProtocol::infoPortal(ax, ay, bx, by));
}

void EngineController::clearPortals() {
    requireIdle("clearPortals");
    send(EngineProtocol::infoClearPortals());
}

// =============================================================================
// Configuration
// =============================================================================

void EngineController::applyConfig(const EngineConfig& config) {
    send(EngineProtocol::infoTimeoutTurn(config.timeoutTurn));
    send(EngineProtocol::infoTimeoutMatch(config.timeoutMatch));
    send(EngineProtocol::infoMaxMemory(config.maxMemoryBytes));
    if (config.threadNum > 0)
        send(EngineProtocol::infoThreadNum(config.threadNum));
    send(EngineProtocol::infoStrength(config.strength));
    send(EngineProtocol::infoMaxDepth(config.maxDepth));
    send(EngineProtocol::infoPondering(config.pondering));
    send(EngineProtocol::infoShowDetail(config.showDetail));
}

// =============================================================================
// Analysis
// =============================================================================

void EngineController::requestNBest(int n) {
    requireIdle("requestNBest");
    send(EngineProtocol::yxNBest(n));
    setState(EngineState::Thinking);
}

void EngineController::stopThinking() {
    if (state_ != EngineState::Thinking) return;  // Silently ignore if not thinking
    send(EngineProtocol::stop());
    setState(EngineState::Stopping);
}

void EngineController::traceBoard() {
    send(EngineProtocol::traceBoard());
}

void EngineController::traceSearch() {
    send(EngineProtocol::traceSearch());
}

// =============================================================================
// Hash & Database
// =============================================================================

void EngineController::hashClear()                       { send(EngineProtocol::yxHashClear()); }
void EngineController::hashDump(const std::string& path) { send(EngineProtocol::yxHashDump(path)); }
void EngineController::hashLoad(const std::string& path) { send(EngineProtocol::yxHashLoad(path)); }
void EngineController::setDatabase(const std::string& p) { send(EngineProtocol::yxSetDatabase(p)); }
void EngineController::saveDatabase()                    { send(EngineProtocol::yxSaveDatabase()); }
void EngineController::queryDatabaseAll()                { send(EngineProtocol::yxQueryDatabaseAll()); }
void EngineController::queryDatabaseOne()                { send(EngineProtocol::yxQueryDatabaseOne()); }
void EngineController::queryDatabaseText()               { send(EngineProtocol::yxQueryDatabaseText()); }
void EngineController::deleteDatabaseOne()               { send(EngineProtocol::yxDeleteDatabaseOne()); }
void EngineController::deleteDatabaseAll()               { send(EngineProtocol::yxDeleteDatabaseAll()); }

// =============================================================================
// Polling & Dispatch
// =============================================================================

int EngineController::pollOutput() {
    return process_.drainOutput();
}

void EngineController::onLineReceived(const std::string& line) {
    auto parsed = EngineProtocol::parse(line);

    switch (parsed.type) {
    case ParsedLine::Type::Move:
        if (state_ == EngineState::Thinking || state_ == EngineState::Stopping)
            setState(EngineState::Idle);
        signalEngineMove.emit(parsed.x1, parsed.y1);
        break;

    case ParsedLine::Type::MoveDouble:
        if (state_ == EngineState::Thinking || state_ == EngineState::Stopping)
            setState(EngineState::Idle);
        signalEngineMove2.emit(parsed.x1, parsed.y1, parsed.x2, parsed.y2);
        break;

    case ParsedLine::Type::Swap:
        if (state_ == EngineState::Thinking || state_ == EngineState::Stopping)
            setState(EngineState::Idle);
        signalSwap.emit();
        break;

    case ParsedLine::Type::Ok:
        signalOk.emit();
        break;

    case ParsedLine::Type::Message:
        signalMessage.emit(parsed.text);
        break;

    case ParsedLine::Type::Error:
        signalError.emit(parsed.text);
        break;

    case ParsedLine::Type::Forbid:
        signalForbid.emit(parsed.forbidPoints);
        break;

    case ParsedLine::Type::DatabaseLine:
        signalDatabaseLine.emit(parsed.text);
        break;

    case ParsedLine::Type::DatabaseDone:
        signalDatabaseDone.emit();
        break;

    case ParsedLine::Type::EngineInfo:
        signalAboutInfo.emit(parsed.text);
        break;

    case ParsedLine::Type::Unknown:
        // Log unrecognized lines as messages
        signalMessage.emit("[Unknown] " + parsed.text);
        break;
    }
}

// =============================================================================
// State Management
// =============================================================================

void EngineController::setState(EngineState newState) {
    if (state_ != newState) {
        state_ = newState;
        signalStateChanged.emit(newState);
    }
}

void EngineController::requireState(EngineState expected, const char* action) const {
    if (state_ != expected) {
        throw std::runtime_error(
            std::string("[EngineController] ") + action
            + " requires state " + toString(expected)
            + " but current state is " + toString(state_));
    }
}

void EngineController::requireIdle(const char* action) const {
    requireState(EngineState::Idle, action);
}

void EngineController::send(const std::string& cmd) {
    if (state_ == EngineState::Disconnected) {
        throw std::runtime_error("[EngineController] Cannot send command while disconnected");
    }
    process_.sendLine(cmd);
}

}  // namespace engine
