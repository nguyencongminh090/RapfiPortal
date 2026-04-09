/*
 *  Portal Gomoku UI — Setup Controller Implementation
 */

#include "SetupController.hpp"
#include "GameController.hpp"

namespace controller {

SetupController::SetupController(GameController& gameCtrl)
    : gameCtrl_(gameCtrl) {
}

bool SetupController::enterSetupMode() {
    // Cannot enter setup mode while the engine is thinking
    if (gameCtrl_.engine().state() == engine::EngineState::Thinking ||
        gameCtrl_.engine().state() == engine::EngineState::Stopping) {
        return false;
    }

    // BUG-008 FIX: Always call resetKeepTopology(), even when ply==0.
    // If the redo stack is non-empty (user undid all moves), resetKeepTopology()
    // clears it, preventing ghost moves from a previous topology being redone
    // after the topology has been modified in setup mode.
    gameCtrl_.board().resetKeepTopology();

    active_ = true;
    pendingPortalA_ = std::nullopt;
    gameCtrl_.signalBoardChanged.emit();
    return true;
}

void SetupController::exitSetupMode() {
    active_ = false;
    pendingPortalA_ = std::nullopt;

    // BUG-005 FIX: Mark topology dirty so startThinking() re-syncs on next think,
    // instead of re-syncing unconditionally on every think call.
    gameCtrl_.markTopologyDirty();

    // Sync the new topology to the engine now (one-time, not on every think).
    gameCtrl_.syncBoardToEngine();

    gameCtrl_.signalBoardChanged.emit();
}

void SetupController::setTool(SetupTool tool) {
    tool_ = tool;
    pendingPortalA_ = std::nullopt; // Reset pending state on tool switch
    gameCtrl_.signalBoardChanged.emit();
}

void SetupController::clearAllTopology() {
    if (!active_) return;
    
    gameCtrl_.board().clearTopology();
    pendingPortalA_ = std::nullopt;
    gameCtrl_.signalBoardChanged.emit();
}

void SetupController::onCellClicked(int x, int y) {
    if (!active_) return;

    model::Board& board = gameCtrl_.board();
    if (!board.inBounds(x, y)) return;

    util::Coord pos(x, y);

    // Get a mutable copy of the current topology
    model::PortalTopology newTopo = board.topology();

    switch (tool_) {
        case SetupTool::Wall:
            // Toggle wall
            if (newTopo.hasWall(pos)) {
                newTopo.removeWall(pos);
            } else {
                newTopo.addWall(pos);
            }
            break;

        case SetupTool::PortalPair:
            if (newTopo.isOccupied(pos)) {
                // If it's already occupied by a portal, we might want to just remove it
                if (newTopo.hasPortal(pos)) {
                    newTopo.removePortal(pos);
                    pendingPortalA_ = std::nullopt;
                }
            } else {
                if (!pendingPortalA_) {
                    // First click: mark A
                    pendingPortalA_ = pos;
                } else {
                    // Second click: mark B and complete pair
                    if (*pendingPortalA_ != pos) {
                        newTopo.addPortal(*pendingPortalA_, pos);
                        pendingPortalA_ = std::nullopt;
                    }
                }
            }
            break;

        case SetupTool::Eraser:
            // Remove whatever is there
            if (newTopo.hasWall(pos)) {
                newTopo.removeWall(pos);
            } else if (newTopo.hasPortal(pos)) {
                newTopo.removePortal(pos);
            }
            pendingPortalA_ = std::nullopt;
            break;
    }

    board.setTopology(newTopo);
    gameCtrl_.signalBoardChanged.emit();
}

}  // namespace controller
