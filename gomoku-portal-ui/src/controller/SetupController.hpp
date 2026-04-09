/*
 *  Portal Gomoku UI — Setup Controller
 *  Handles the state machine for setting up the board topology (Walls, Portals).
 *  Operates on the Board model through the GameController.
 */

#pragma once

#include "../model/Board.hpp"
#include "../util/Coord.hpp"
#include <optional>

namespace controller {

class GameController;

/// Tool selected during Setup Mode.
enum class SetupTool {
    Wall,
    PortalPair,
    Eraser
};

/// Controls the interactive placement of walls and portals.
class SetupController {
public:
    explicit SetupController(GameController& gameCtrl);

    /// Enter setup mode. Clears all stones from the board.
    /// Returns false if the game could not be cleared (e.g. engine thinking).
    bool enterSetupMode();

    /// Exit setup mode. Syncs the current topology to the engine.
    void exitSetupMode();

    /// Is setup mode currently active?
    [[nodiscard]] bool isActive() const { return active_; }

    /// Select the active drawing tool.
    void setTool(SetupTool tool);
    [[nodiscard]] SetupTool tool() const { return tool_; }

    /// Handle a click on the board canvas.
    void onCellClicked(int x, int y);

    /// Clear all topology (walls and portals).
    void clearAllTopology();

    /// If placing a Portal Pair, this holds the first clicked endpoint.
    /// Used by BoardCanvas to draw the 'pending' state.
    [[nodiscard]] std::optional<util::Coord> pendingPortalA() const { return pendingPortalA_; }

private:
    GameController& gameCtrl_;
    bool active_ = false;
    SetupTool tool_ = SetupTool::Wall;
    std::optional<util::Coord> pendingPortalA_;
};

}  // namespace controller
