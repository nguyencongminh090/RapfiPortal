#include "SPSAController.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <glibmm/main.h>

namespace controller {

static bool checkWin(const model::Board& board, util::Coord pos, model::Color c) {
    if (pos == util::Coord::none() || board.cellAt(pos) != model::colorToCell(c)) return false;
    int dx[] = {1,0,1,1}; int dy[] = {0,1,1,-1};
    model::Cell tc = model::colorToCell(c);
    for (int d = 0; d < 4; ++d) {
        int count = 1; util::Coord curr = pos;
        for (int i = 0; i < 5; ++i) {
            curr = util::Coord(curr.x+dx[d], curr.y+dy[d]);
            if (!board.inBounds(curr.x,curr.y)) break;
            auto cell = board.cellAt(curr);
            if (cell == model::Cell::Wall) break;
            if (cell == model::Cell::PortalA || cell == model::Cell::PortalB) {
                auto p = board.portalPartner(curr.x,curr.y); if (!p) break; curr = *p; continue;
            }
            if (cell == tc) count++; else break;
        }
        curr = pos;
        for (int i = 0; i < 5; ++i) {
            curr = util::Coord(curr.x-dx[d], curr.y-dy[d]);
            if (!board.inBounds(curr.x,curr.y)) break;
            auto cell = board.cellAt(curr);
            if (cell == model::Cell::Wall) break;
            if (cell == model::Cell::PortalA || cell == model::Cell::PortalB) {
                auto p = board.portalPartner(curr.x,curr.y); if (!p) break; curr = *p; continue;
            }
            if (cell == tc) count++; else break;
        }
        if (count >= 5) return true;
    }
    return false;
}

SPSAController::SPSAController() : rng_(std::random_device{}()) {}

SPSAController::~SPSAController() {
    stop();
    destroySlots();
}

int SPSAController::activeSlotsCount() const {
    int c = 0;
    for (auto& s : slots_) if (s->busy) c++;
    return c;
}

void SPSAController::initSlots(int count) {
    destroySlots();
    slots_.clear();
    slots_.reserve(count);
    for (int i = 0; i < count; ++i) {
        slots_.push_back(std::make_unique<GameSlot>());
        slots_[i]->id = i;
        slots_[i]->connections.push_back(
            slots_[i]->enginePlus.signalEngineMove.connect(
                [this, i](int x, int y) { onSlotMove(i, true, x, y); }));
        slots_[i]->connections.push_back(
            slots_[i]->enginePlus.signalError.connect(
                [this, i](const std::string& e) { onSlotError(i, true, e); }));
        slots_[i]->connections.push_back(
            slots_[i]->engineMinus.signalEngineMove.connect(
                [this, i](int x, int y) { onSlotMove(i, false, x, y); }));
        slots_[i]->connections.push_back(
            slots_[i]->engineMinus.signalError.connect(
                [this, i](const std::string& e) { onSlotError(i, false, e); }));
        slots_[i]->connections.push_back(
            slots_[i]->enginePlus.signalMessage.connect(
                [this, i](const std::string& msg) { onSlotMessage(i, true, msg); }));
        slots_[i]->connections.push_back(
            slots_[i]->engineMinus.signalMessage.connect(
                [this, i](const std::string& msg) { onSlotMessage(i, false, msg); }));
    }
}

void SPSAController::destroySlots() {
    for (auto& s : slots_) {
        s->enginePlus.disconnect();
        s->engineMinus.disconnect();
        for (auto& c : s->connections) c.disconnect();
        s->connections.clear();
    }
    slots_.clear();
}

bool SPSAController::connectSlotEngines(GameSlot& slot, const std::string& plusPath, const std::string& minusPath) {
    engine::EngineConfig cfg;
    cfg.executablePath = plusPath;
    cfg.timeoutTurn = config_.turnTimeMs;
    cfg.timeoutMatch = config_.matchTimeMs;
    cfg.maxMemoryBytes = config_.maxMemory;
    cfg.threadNum = config_.threads;
    cfg.boardSize = config_.boardSize;
    cfg.rule = config_.rule;
    if (!slot.enginePlus.connect(cfg)) return false;
    cfg.executablePath = minusPath;
    if (!slot.engineMinus.connect(cfg)) { slot.enginePlus.disconnect(); return false; }
    return true;
}

void SPSAController::start(const SPSAConfig& config) {
    config_ = config;
    stopRequested_ = false;
    int conc = std::clamp(config_.concurrency, 1, MAX_SLOTS);
    config_.concurrency = conc;

    openings_ = model::OBFManager::readOpenings(config_.obfPath);
    if (openings_.empty()) {
        log("No openings found. Using empty board.");
        model::GameRecord rec; rec.boardSize = config_.boardSize;
        openings_.push_back(rec);
    }

    if (!config_.paramsConfigPath.empty()) {
        loadParamsConfig(config_.paramsConfigPath);
    } else {
        initDefaultParams();
    }

    if (!config_.statePath.empty() && loadState(config_.statePath)) {
        log("Resumed SPSA from iteration " + std::to_string(iteration_)
            + " with " + std::to_string(params_.size()) + " params.");
    } else {
        iteration_ = 0; history_.clear();
        log("Starting fresh SPSA with " + std::to_string(params_.size()) + " params.");
    }

    initSlots(conc);
    log("Concurrency: " + std::to_string(conc) + " slot(s)");

    state_ = SPSAState::Playing;
    signalStateChanged.emit();
    signalParamsUpdated.emit();

    generatePerturbation();
    startIterationMatches();
}

void SPSAController::stop() {
    if (state_ == SPSAState::Idle) return;
    stopRequested_ = true;
    stopRequested_ = true;
    for (auto& s : slots_) { s->enginePlus.disconnect(); s->engineMinus.disconnect(); s->busy = false; }
    while (!pendingGames_.empty()) pendingGames_.pop();
    while (!pendingSlotStarts_.empty()) pendingSlotStarts_.pop();
    state_ = SPSAState::Finished;
    signalStateChanged.emit();
    log("SPSA tuning stopped.");
}

void SPSAController::pollEngines() {
    for (auto& s : slots_) {
        if (!s->busy) continue;
        
        s->enginePlus.pollOutput();
        s->engineMinus.pollOutput();
        
        if (!s->enginePlus.isAlive() || !s->engineMinus.isAlive()) {
            bool pDead = !s->enginePlus.isAlive();
            bool mDead = !s->engineMinus.isAlive();
            log("Watchdog: Slot " + std::to_string(s->id) + " engine died silently! Plus=" 
                + (pDead?"DEAD":"OK") + " Minus=" + (mDead?"DEAD":"OK"));
            endSlotGame(s->id, pDead ? 2 : 1);
        }
    }
}

double SPSAController::a_k(double base_a) const { return base_a / std::pow(config_.A + iteration_ + 1.0, config_.alpha); }
double SPSAController::c_k(double base_c) const { return base_c / std::pow(iteration_ + 1.0, config_.gamma); }

void SPSAController::initDefaultParams() {
    params_.clear();
    params_.push_back({"WALL_DEAD_POCKET_PENALTY",     -60.0, -200.0,   0.0, -60.0, 0.0, 0.0});
    params_.push_back({"WALL_CORRIDOR_FOUR_BONUS",     450.0,  200.0, 800.0, 450.0, 0.0, 0.0});
    params_.push_back({"WALL_ISOLATED_THREAT_PENALTY", -30.0, -100.0,   0.0, -30.0, 0.0, 0.0});
    params_.push_back({"WALL_ADJACENCY_MOVE_BONUS",     80.0,   50.0, 200.0,  80.0, 0.0, 0.0});
    params_.push_back({"PORTAL_ADJACENCY_MOVE_BONUS",   60.0,   40.0, 150.0,  60.0, 0.0, 0.0});
    params_.push_back({"WALL_FIRST_MOVE_BONUS",        250.0,  100.0, 400.0, 250.0, 0.0, 0.0});
}

void SPSAController::loadParamsConfig(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        log("Failed to open params config: " + path);
        initDefaultParams();
        return;
    }
    params_.clear();
    std::string line;
    // skip header if it exists
    if (std::getline(file, line) && line.find(',') != std::string::npos) {
        if (line.find("Name") == std::string::npos && line.find("name") == std::string::npos) {
            file.clear();
            file.seekg(0);
        }
    }
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string name, initStr, minStr, maxStr, aStr, cStr;
        if (std::getline(ss, name, ',') &&
            std::getline(ss, initStr, ',') &&
            std::getline(ss, minStr, ',') &&
            std::getline(ss, maxStr, ',')) {
            
            std::getline(ss, aStr, ',');
            std::getline(ss, cStr, ',');
            
            try {
                double initial = std::stod(initStr);
                double min = std::stod(minStr);
                double max = std::stod(maxStr);
                double a = aStr.empty() ? 0.0 : std::stod(aStr);
                double c = cStr.empty() ? 0.0 : std::stod(cStr);
                params_.push_back({name, initial, min, max, initial, a, c});
            } catch(...) {
                log("Error parsing param line: " + line);
            }
        }
    }
}

void SPSAController::generatePerturbation() {
    delta_.resize(params_.size());
    std::uniform_int_distribution<int> dist(0, 1);
    for (size_t i = 0; i < params_.size(); ++i) delta_[i] = dist(rng_) * 2 - 1;
}

void SPSAController::applyPerturbedParams(GameSlot& slot) {
    for (size_t i = 0; i < params_.size(); ++i) {
        double p_c = (params_[i].c > 0) ? params_[i].c : config_.c;
        double ck = c_k(p_c);
        double pp = std::clamp(params_[i].value + ck * delta_[i], params_[i].min, params_[i].max);
        slot.enginePlus.send_raw_if_idle("INFO " + params_[i].name + " " + std::to_string(static_cast<int>(std::round(pp))));
    }
    for (size_t i = 0; i < params_.size(); ++i) {
        double p_c = (params_[i].c > 0) ? params_[i].c : config_.c;
        double ck = c_k(p_c);
        double pm = std::clamp(params_[i].value - ck * delta_[i], params_[i].min, params_[i].max);
        slot.engineMinus.send_raw_if_idle("INFO " + params_[i].name + " " + std::to_string(static_cast<int>(std::round(pm))));
    }
}

void SPSAController::applyCurrentParams(engine::EngineController& engine) {
    for (const auto& p : params_)
        engine.send_raw_if_idle("INFO " + p.name + " " + std::to_string(static_cast<int>(std::round(p.value))));
}

void SPSAController::sendStartToSlotEngine(engine::EngineController& engine, const model::GameRecord& rec) {
    engine::EngineConfig cfg;
    cfg.boardSize = rec.boardSize > 0 ? rec.boardSize : config_.boardSize;
    cfg.rule = rec.rule;
    cfg.timeoutTurn = config_.turnTimeMs;
    cfg.timeoutMatch = config_.matchTimeMs;
    cfg.maxMemoryBytes = config_.maxMemory;
    cfg.threadNum = config_.threads;
    std::vector<std::pair<int,int>> walls;
    for (const auto& w : rec.topology.walls()) walls.push_back({w.x, w.y});
    std::vector<std::tuple<int,int,int,int>> portals;
    for (const auto& p : rec.topology.portals()) portals.push_back({p.a.x, p.a.y, p.b.x, p.b.y});
    engine.startGame(cfg, walls, portals);
}

// === Match Execution ===

void SPSAController::startIterationMatches() {
    log("=== SPSA Iteration " + std::to_string(iteration_ + 1) + " ===");

    matchScorePlus_ = 0; matchScoreMinus_ = 0; matchDraws_ = 0;
    gamesCompleted_ = 0;
    gamesTotalCurrent_ = config_.gamesPerIteration;
    while (!pendingGames_.empty()) pendingGames_.pop();
    for (int i = 0; i < gamesTotalCurrent_; ++i) pendingGames_.push(i);

    while (!pendingSlotStarts_.empty()) pendingSlotStarts_.pop();
    for (auto& sp : slots_) {
        sp->busy = false;
        sp->enginePlus.disconnect();
        sp->engineMinus.disconnect();
        pendingSlotStarts_.push(sp->id);
    }
    
    // Launch the first slot start timeout
    if (!pendingSlotStarts_.empty()) {
        Glib::signal_timeout().connect(sigc::mem_fun(*this, &SPSAController::onSlotStartTimeout), 10);
    }
    signalProgressUpdated.emit();
}

bool SPSAController::onSlotStartTimeout() {
    if (stopRequested_ || pendingSlotStarts_.empty()) return false;
    
    int slotId = pendingSlotStarts_.front();
    pendingSlotStarts_.pop();
    auto& slot = *slots_[slotId];
    
    std::string ePlusPath = (state_ == SPSAState::Validating) ? config_.enginePath : config_.enginePath;
    std::string eMinusPath = (state_ == SPSAState::Validating) ? config_.baselinePath : config_.enginePath;
    
    if (!connectSlotEngines(slot, ePlusPath, eMinusPath)) {
        log("Failed to start engines for slot " + std::to_string(slot.id));
    } else {
        if (state_ == SPSAState::Playing) applyPerturbedParams(slot);
        else applyCurrentParams(slot.enginePlus);
        dispatchToSlot(slot.id);
    }
    
    // Continue timeout loop if there are more slots to start
    return !pendingSlotStarts_.empty();
}

void SPSAController::dispatchToSlot(int slotId) {
    if (stopRequested_) return;
    auto& slot = *slots_[slotId];

    // If engines died (e.g. OOM or resource limit), attempt to reconnect them before assigning next game.
    if (!slot.enginePlus.isAlive() || !slot.engineMinus.isAlive()) {
        log("Slot " + std::to_string(slotId) + " engines died, attempting to reconnect...");
        slot.enginePlus.disconnect();
        slot.engineMinus.disconnect();
        
        std::string ePlusPath = (state_ == SPSAState::Validating) ? config_.enginePath : config_.enginePath;
        std::string eMinusPath = (state_ == SPSAState::Validating) ? config_.baselinePath : config_.enginePath;
        
        if (!connectSlotEngines(slot, ePlusPath, eMinusPath)) {
            log("Failed to restart engines for slot " + std::to_string(slotId));
            slot.busy = false;
            // Still check if we need to end iteration
            bool allIdle = true;
            for (auto& s : slots_) if (s->busy) { allIdle = false; break; }
            if (allIdle) {
                if (state_ == SPSAState::Validating) onAllValidationGamesFinished();
                else onAllGamesFinished();
            }
            return;
        }
        
        if (state_ == SPSAState::Playing) {
            applyPerturbedParams(slot);
        } else if (state_ == SPSAState::Validating) {
            applyCurrentParams(slot.enginePlus);
        }
    }

    if (pendingGames_.empty()) {
        slot.busy = false;
        // Check if all slots are idle -> iteration done
        bool allIdle = true;
        for (auto& s : slots_) if (s->busy) { allIdle = false; break; }
        if (allIdle) {
            if (state_ == SPSAState::Validating) onAllValidationGamesFinished();
            else onAllGamesFinished();
        }
        return;
    }

    slot.gameIndex = pendingGames_.front();
    pendingGames_.pop();
    slot.busy = true;
    signalProgressUpdated.emit();
    slot.openingIdx = (slot.gameIndex / 2) % static_cast<int>(openings_.size());
    slot.gameInOpening = slot.gameIndex % 2;
    slot.lastEvalWR = 50.0f;

    const auto& opening = openings_[slot.openingIdx];
    slot.board.reset(opening.boardSize > 0 ? opening.boardSize : config_.boardSize);
    slot.board.setTopology(opening.topology);
    for (const auto& m : opening.moves) {
        if (!m.isPass()) slot.board.placeStone(m.coord, m.color);
        else slot.board.pass(m.color);
    }

    sendStartToSlotEngine(slot.enginePlus, opening);
    sendStartToSlotEngine(slot.engineMinus, opening);

    bool plusIsBlack = (slot.gameInOpening % 2 == 0);
    bool openingTurnIsBlack = (opening.moves.size() % 2 == 0);
    slot.isPlusTurn = (openingTurnIsBlack == plusIsBlack);

    if (spectating_ && slotId == spectateSlot_) {
        signalBoardUpdated.emit(slot.board);
        std::string bName, wName;
        if (state_ == SPSAState::Validating) {
            bName = plusIsBlack ? "Tuned" : "Baseline";
            wName = plusIsBlack ? "Baseline" : "Tuned";
        } else {
            bName = plusIsBlack ? "theta+" : "theta-";
            wName = plusIsBlack ? "theta-" : "theta+";
        }
        signalMatchInfo.emit(bName, wName);
    }

    auto entries = opening.toBoardEntries();
    slot.plusTimeLeftMs = config_.matchTimeMs;
    slot.minusTimeLeftMs = config_.matchTimeMs;

    if (slot.isPlusTurn) {
        slot.engineMinus.loadPositionSilent(entries);
        if (config_.matchTimeMs > 0) slot.enginePlus.send_raw_if_idle("INFO time_left " + std::to_string(slot.plusTimeLeftMs));
        if (entries.empty()) { slot.enginePlus.loadPositionSilent(entries); slot.enginePlus.requestEngineMove(); }
        else slot.enginePlus.loadPosition(entries);
    } else {
        slot.enginePlus.loadPositionSilent(entries);
        if (config_.matchTimeMs > 0) slot.engineMinus.send_raw_if_idle("INFO time_left " + std::to_string(slot.minusTimeLeftMs));
        if (entries.empty()) { slot.engineMinus.loadPositionSilent(entries); slot.engineMinus.requestEngineMove(); }
        else slot.engineMinus.loadPosition(entries);
    }
    slot.turnStartTime = std::chrono::steady_clock::now();
}

void SPSAController::onSlotMove(int slotId, bool fromPlus, int x, int y) {
    if (state_ != SPSAState::Playing && state_ != SPSAState::Validating) return;
    auto& slot = *slots_[slotId];
    if (!slot.busy) return;
    if (fromPlus != slot.isPlusTurn) return;

    if (config_.matchTimeMs > 0) {
        auto now = std::chrono::steady_clock::now();
        int elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - slot.turnStartTime).count();
        if (fromPlus) {
            slot.plusTimeLeftMs -= elapsedMs;
            if (slot.plusTimeLeftMs < 0) { endSlotGame(slotId, 2); return; }
        } else {
            slot.minusTimeLeftMs -= elapsedMs;
            if (slot.minusTimeLeftMs < 0) { endSlotGame(slotId, 1); return; }
        }
    }

    util::Coord mv(x, y);
    if (mv == util::Coord::none()) {
        slot.board.pass(slot.board.sideToMove());
    } else {
        if (!slot.board.isLegalMove(mv)) {
            endSlotGame(slotId, fromPlus ? 2 : 1);
            return;
        }
        slot.board.placeStone(mv, slot.board.sideToMove());
    }

    if (spectating_ && slotId == spectateSlot_) {
        signalMoveMade.emit(x, y, static_cast<int>(slot.board.sideToMove() == model::Color::Black ? 1 : 2));
        signalBoardUpdated.emit(slot.board);
    }

    auto curColor = slot.board.sideToMove() == model::Color::Black ? model::Color::White : model::Color::Black;
    if (checkWin(slot.board, mv, curColor)) { endSlotGame(slotId, fromPlus ? 1 : 2); return; }
    if (slot.board.isBoardFull()) { endSlotGame(slotId, 0); return; }

    int totalCells = slot.board.size() * slot.board.size();
    if (slot.board.totalStones() >= totalCells * 0.8f) {
        if (slot.lastEvalWR >= 45.0f && slot.lastEvalWR <= 55.0f) {
            log("Slot " + std::to_string(slotId) + " draw by 80% full and balanced eval (wr " + std::to_string(slot.lastEvalWR) + ").");
            endSlotGame(slotId, 0);
            return;
        }
    }

    slot.isPlusTurn = !slot.isPlusTurn;
    if (config_.matchTimeMs > 0) {
        if (slot.isPlusTurn) slot.enginePlus.send_raw_if_idle("INFO time_left " + std::to_string(slot.plusTimeLeftMs));
        else slot.engineMinus.send_raw_if_idle("INFO time_left " + std::to_string(slot.minusTimeLeftMs));
    }
    slot.turnStartTime = std::chrono::steady_clock::now();
    
    if (slot.isPlusTurn) slot.enginePlus.makeMove(x, y);
    else slot.engineMinus.makeMove(x, y);
}

void SPSAController::onSlotError(int slotId, bool fromPlus, const std::string& err) {
    log("Slot " + std::to_string(slotId) + " engine " + (fromPlus ? "+" : "-") + " error: " + err);
    endSlotGame(slotId, fromPlus ? 2 : 1);
}

void SPSAController::onSlotMessage(int slotId, bool fromPlus, const std::string& msg) {
    auto& slot = *slots_[slotId];
    if (!slot.busy) return;
    
    float wr = -1.0f;
    if (auto pos = msg.find("(W "); pos != std::string::npos) {
        try { wr = std::stof(msg.substr(pos + 3)); } catch(...) {}
    } else if (auto pos = msg.find(" w "); pos != std::string::npos) {
        try { wr = std::stof(msg.substr(pos + 3)); } catch(...) {}
    } else if (auto pos = msg.find(" | Eval "); pos != std::string::npos) {
        try { wr = 50.0f + std::stoi(msg.substr(pos + 8)) / 10.0f; } catch(...) {}
    } else if (auto pos = msg.find(" ev "); pos != std::string::npos) {
        try { wr = 50.0f + std::stoi(msg.substr(pos + 4)) / 10.0f; } catch(...) {}
    }
    
    if (wr >= 0.0f) {
        slot.lastEvalWR = wr;
    }
}

void SPSAController::endSlotGame(int slotId, int result) {
    auto& slot = *slots_[slotId];
    if (!slot.busy) return;
    slot.busy = false;

    if (result == 1) matchScorePlus_++;
    else if (result == 2) matchScoreMinus_++;
    else matchDraws_++;
    gamesCompleted_++;

    signalProgressUpdated.emit();

    Glib::signal_idle().connect_once([this, slotId]() {
        if (stopRequested_) return;
        auto& slot = *slots_[slotId];
        slot.enginePlus.disconnect();
        slot.engineMinus.disconnect();
        std::string plusPath = config_.enginePath;
        std::string minusPath = (state_ == SPSAState::Validating) ? config_.baselinePath : config_.enginePath;
        if (!connectSlotEngines(slot, plusPath, minusPath)) {
            log("Failed to reconnect slot " + std::to_string(slotId));
            slot.busy = false;
            return;
        }
        if (state_ == SPSAState::Validating) applyCurrentParams(slot.enginePlus);
        else applyPerturbedParams(slot);
        dispatchToSlot(slotId);
    });
}

void SPSAController::onAllGamesFinished() { finalizeIteration(); }

void SPSAController::finalizeIteration() {
    double Wplus = matchScorePlus_ + 0.5 * matchDraws_;
    double Wminus = matchScoreMinus_ + 0.5 * matchDraws_;
    double total = matchScorePlus_ + matchScoreMinus_ + matchDraws_;
    double scoreDiff = (total > 0) ? (Wplus - Wminus) / total : 0.0;

    log("Iter " + std::to_string(iteration_+1) + ": +=" + std::to_string(matchScorePlus_)
        + " -=" + std::to_string(matchScoreMinus_) + " D=" + std::to_string(matchDraws_));

    for (size_t i = 0; i < params_.size(); ++i) {
        double p_a = (params_[i].a > 0) ? params_[i].a : config_.a;
        double p_c = (params_[i].c > 0) ? params_[i].c : config_.c;
        double ak = a_k(p_a);
        double ck = c_k(p_c);
        double g_hat = scoreDiff / (2.0 * ck * delta_[i]);
        double newVal = std::clamp(params_[i].value + ak * g_hat, params_[i].min, params_[i].max);
        log("  " + params_[i].name + ": " + std::to_string((int)std::round(params_[i].value))
            + " -> " + std::to_string((int)std::round(newVal)));
        params_[i].value = newVal;
    }

    history_.push_back({iteration_+1, (double)matchScorePlus_, (double)matchScoreMinus_, (double)matchDraws_});
    iteration_++;
    signalIterationComplete.emit();
    signalParamsUpdated.emit();
    saveState();

    if (stopRequested_) { state_ = SPSAState::Finished; signalStateChanged.emit(); return; }

    for (auto& s : slots_) { s->enginePlus.disconnect(); s->engineMinus.disconnect(); }

    if (shouldValidate()) {
        Glib::signal_idle().connect_once([this]() { if (!stopRequested_) startValidationMatch(); });
        return;
    }
    generatePerturbation();
    Glib::signal_idle().connect_once([this]() { if (!stopRequested_ && state_ == SPSAState::Playing) startIterationMatches(); });
}

// === Validation ===

bool SPSAController::shouldValidate() const {
    if (config_.baselinePath.empty() || config_.validationInterval <= 0) return false;
    return (iteration_ % config_.validationInterval == 0);
}

void SPSAController::startValidationMatch() {
    state_ = SPSAState::Validating;
    signalStateChanged.emit();
    log("=== VALIDATION after iter " + std::to_string(iteration_) + " ===");

    matchScorePlus_ = 0; matchScoreMinus_ = 0; matchDraws_ = 0;
    gamesCompleted_ = 0; gamesTotalCurrent_ = config_.validationGames;
    while (!pendingGames_.empty()) pendingGames_.pop();
    for (int i = 0; i < gamesTotalCurrent_; ++i) pendingGames_.push(i);

    while (!pendingSlotStarts_.empty()) pendingSlotStarts_.pop();
    for (auto& sp : slots_) {
        sp->busy = false;
        sp->enginePlus.disconnect();
        sp->engineMinus.disconnect();
        pendingSlotStarts_.push(sp->id);
    }
    if (!pendingSlotStarts_.empty()) {
        Glib::signal_timeout().connect(sigc::mem_fun(*this, &SPSAController::onSlotStartTimeout), 10);
    }
    signalProgressUpdated.emit();
}

void SPSAController::onAllValidationGamesFinished() { finalizeValidation(); }

void SPSAController::finalizeValidation() {
    double total = matchScorePlus_ + matchScoreMinus_ + matchDraws_;
    double wr = total > 0 ? (matchScorePlus_ + 0.5 * matchDraws_) / total * 100.0 : 50.0;
    valHistory_.push_back({iteration_, matchScorePlus_, matchScoreMinus_, matchDraws_, wr});
    log("VALIDATION: Tuned=" + std::to_string(matchScorePlus_) + " Base=" + std::to_string(matchScoreMinus_)
        + " D=" + std::to_string(matchDraws_) + " WR=" + std::to_string(wr) + "%");
    signalValidationComplete.emit();
    saveState();

    for (auto& s : slots_) { s->enginePlus.disconnect(); s->engineMinus.disconnect(); }
    state_ = SPSAState::Playing; signalStateChanged.emit();
    if (stopRequested_) { state_ = SPSAState::Finished; signalStateChanged.emit(); return; }
    generatePerturbation();
    Glib::signal_idle().connect_once([this]() { if (!stopRequested_ && state_ == SPSAState::Playing) startIterationMatches(); });
}

// === Persistence ===

void SPSAController::saveState() {
    if (config_.statePath.empty()) return;
    std::filesystem::create_directories(std::filesystem::path(config_.statePath).parent_path());
    std::ofstream out(config_.statePath);
    if (!out.is_open()) { log("Warning: Cannot write state"); return; }
    out << std::fixed << std::setprecision(6);
    out << "{\n  \"iteration\": " << iteration_ << ",\n";
    out << "  \"params\": [\n";
    for (size_t i = 0; i < params_.size(); ++i) {
        out << "    { \"name\": \"" << params_[i].name << "\", \"value\": " << params_[i].value
            << ", \"min\": " << params_[i].min << ", \"max\": " << params_[i].max
            << ", \"initial\": " << params_[i].initial 
            << ", \"a\": " << params_[i].a << ", \"c\": " << params_[i].c << " }";
        if (i+1 < params_.size()) out << ","; out << "\n";
    }
    out << "  ],\n  \"history\": [\n";
    for (size_t i = 0; i < history_.size(); ++i) {
        out << "    { \"iter\": " << history_[i].iteration << ", \"plus\": " << history_[i].scoreTheta_plus
            << ", \"minus\": " << history_[i].scoreTheta_minus << ", \"draws\": " << history_[i].draws << " }";
        if (i+1 < history_.size()) out << ","; out << "\n";
    }
    out << "  ],\n  \"validation\": [\n";
    for (size_t i = 0; i < valHistory_.size(); ++i) {
        out << "    { \"after\": " << valHistory_[i].afterIteration << ", \"tuned\": " << valHistory_[i].scoreTuned
            << ", \"baseline\": " << valHistory_[i].scoreBaseline << ", \"draws\": " << valHistory_[i].draws
            << ", \"winrate\": " << valHistory_[i].winrate << " }";
        if (i+1 < valHistory_.size()) out << ","; out << "\n";
    }
    out << "  ]\n}\n";
    log("State saved (iter=" + std::to_string(iteration_) + ")");
}

bool SPSAController::loadState(const std::string& path) {
    std::ifstream in(path); if (!in.is_open()) return false;
    std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    auto iterPos = content.find("\"iteration\":"); if (iterPos == std::string::npos) return false;
    iteration_ = std::stoi(content.substr(iterPos + 13));
    // Only clear if we didn't already populate from CSV
    if (params_.empty()) {
        params_.clear();
    }
    auto paramsStart = content.find("\"params\":"); if (paramsStart == std::string::npos) return false;
    size_t sf = paramsStart;
    while (true) {
        auto np = content.find("\"name\":", sf); if (np == std::string::npos) break;
        auto nb = content.find(']', paramsStart+9); if (np > nb) break;
        auto ns = content.find('"', np+7)+1; auto ne = content.find('"', ns);
        std::string name = content.substr(ns, ne-ns);
        auto vp = content.find("\"value\":", ne); double value = std::stod(content.substr(vp+8));
        auto mp = content.find("\"min\":", vp); double min = std::stod(content.substr(mp+6));
        auto xp = content.find("\"max\":", mp); double max = std::stod(content.substr(xp+6));
        auto ip = content.find("\"initial\":", xp); double initial = std::stod(content.substr(ip+10));
        
        double a = 0.0, c = 0.0;
        auto ap = content.find("\"a\":", ip);
        auto nextObj = content.find("\"name\":", ip);
        if (ap != std::string::npos && (nextObj == std::string::npos || ap < nextObj)) {
            a = std::stod(content.substr(ap+4));
        }
        auto cp = content.find("\"c\":", ip);
        if (cp != std::string::npos && (nextObj == std::string::npos || cp < nextObj)) {
            c = std::stod(content.substr(cp+4));
        }

        bool found = false;
        for (auto& p : params_) {
            if (p.name == name) {
                p.value = value;
                // Min, max, initial, a, c from CSV take priority, so we don't overwrite them
                found = true;
                break;
            }
        }
        if (!found) {
            params_.push_back({name, value, min, max, initial, a, c});
        }
        sf = ip+10;
    }
    history_.clear();
    auto hs = content.find("\"history\":"); if (hs != std::string::npos) {
        size_t h = hs;
        while (true) {
            auto ip2 = content.find("\"iter\":", h); if (ip2 == std::string::npos) break;
            auto he = content.find(']', hs+10); if (ip2 > he) break;
            SPSAIterationResult r;
            r.iteration = std::stoi(content.substr(ip2+7));
            auto pp = content.find("\"plus\":", ip2); r.scoreTheta_plus = std::stod(content.substr(pp+7));
            auto mp = content.find("\"minus\":", pp); r.scoreTheta_minus = std::stod(content.substr(mp+8));
            auto dp = content.find("\"draws\":", mp); r.draws = std::stod(content.substr(dp+8));
            history_.push_back(r); h = dp+8;
        }
    }
    valHistory_.clear();
    auto vs = content.find("\"validation\":"); if (vs != std::string::npos) {
        size_t v = vs;
        while (true) {
            auto ap = content.find("\"after\":", v); if (ap == std::string::npos) break;
            auto ve = content.find(']', vs+13); if (ap > ve) break;
            SPSAValidationResult vr;
            vr.afterIteration = std::stoi(content.substr(ap+8));
            auto tp = content.find("\"tuned\":", ap); vr.scoreTuned = std::stoi(content.substr(tp+8));
            auto bp = content.find("\"baseline\":", tp); vr.scoreBaseline = std::stoi(content.substr(bp+11));
            auto dp = content.find("\"draws\":", bp); vr.draws = std::stoi(content.substr(dp+8));
            auto wp = content.find("\"winrate\":", dp); vr.winrate = std::stod(content.substr(wp+10));
            valHistory_.push_back(vr); v = wp+10;
        }
    }
    return !params_.empty();
}

void SPSAController::log(const std::string& msg) {
    std::cout << "[SPSA] " << msg << std::endl;
    signalLogMessage.emit(msg);
}

} // namespace controller
