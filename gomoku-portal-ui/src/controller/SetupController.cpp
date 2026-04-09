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

    // Entering setup clears all stones
    if (gameCtrl_.board().ply() > 0) {
        // Keep the topology, just reset the game state
        gameCtrl_.board().resetKeepTopology();
    }

    active_ = true;
    pendingPortalA_ = std::nullopt;
    gameCtrl_.signalBoardChanged.emit();
    return true;
}

void SetupController::exitSetupMode() {
    active_ = false;
    pendingPortalA_ = std::nullopt;
    
    // Sync the new topology to the engine
    gameCtrl_.syncBoardToEngine();
    
    // Refresh board canvas to hide hover state
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
