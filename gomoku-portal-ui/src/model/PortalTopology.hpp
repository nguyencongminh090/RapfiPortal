/*
 *  Portal Gomoku UI — Portal Topology
 *  Stores wall positions and portal pairs for the board.
 *  Pure data — no rendering or engine dependency.
 */

#pragma once

#include "../util/Coord.hpp"

#include <algorithm>
#include <vector>

namespace model {

/// Stores the board's portal/wall topology.
/// This mirrors the engine's pendingPortals structure on the UI side.
///
/// Layout:
///   walls_   — list of wall cell coordinates
///   portals_ — list of portal pairs (A, B)
///
/// Invariants:
///   - No duplicate wall positions
///   - No duplicate portal pairs
///   - Wall and portal positions do not overlap
class PortalTopology {
public:
    PortalTopology() = default;

    // --- Walls ---

    /// Add a wall at the given position. Returns false if already exists.
    bool addWall(util::Coord pos) {
        if (hasWall(pos) || hasPortal(pos)) return false;
        walls_.push_back(pos);
        return true;
    }

    /// Remove a wall at the given position. Returns false if not found.
    bool removeWall(util::Coord pos) {
        auto it = std::find(walls_.begin(), walls_.end(), pos);
        if (it == walls_.end()) return false;
        walls_.erase(it);
        return true;
    }

    /// Check if a wall exists at the given position.
    [[nodiscard]] bool hasWall(util::Coord pos) const {
        return std::find(walls_.begin(), walls_.end(), pos) != walls_.end();
    }

    /// All wall positions.
    [[nodiscard]] const std::vector<util::Coord>& walls() const { return walls_; }

    // --- Portals ---

    /// Add a portal pair. Returns false if either position is already occupied.
    bool addPortal(util::Coord a, util::Coord b) {
        if (a == b) return false;
        if (hasWall(a) || hasWall(b)) return false;
        if (hasPortal(a) || hasPortal(b)) return false;
        portals_.push_back({a, b});
        return true;
    }

    /// Remove a portal pair containing the given position. Returns false if not found.
    bool removePortal(util::Coord pos) {
        auto it = std::find_if(portals_.begin(), portals_.end(),
            [&](const util::PortalPair& p) { return p.a == pos || p.b == pos; });
        if (it == portals_.end()) return false;
        portals_.erase(it);
        return true;
    }

    /// Check if a portal exists at the given position.
    [[nodiscard]] bool hasPortal(util::Coord pos) const {
        return findPortalPair(pos) != nullptr;
    }

    /// Get the partner of a portal at the given position.
    /// Returns Coord::none() if no portal exists at pos.
    [[nodiscard]] util::Coord portalPartner(util::Coord pos) const {
        auto* pair = findPortalPair(pos);
        if (!pair) return util::Coord::none();
        return (pair->a == pos) ? pair->b : pair->a;
    }

    /// All portal pairs.
    [[nodiscard]] const std::vector<util::PortalPair>& portals() const { return portals_; }

    // --- General ---

    /// Check if a position is occupied by any obstacle (wall or portal).
    [[nodiscard]] bool isOccupied(util::Coord pos) const {
        return hasWall(pos) || hasPortal(pos);
    }

    /// Total number of obstacles (walls + portal endpoints).
    [[nodiscard]] int obstacleCount() const {
        return static_cast<int>(walls_.size() + portals_.size() * 2);
    }

    /// Clear all walls and portals.
    void clear() {
        walls_.clear();
        portals_.clear();
    }

    /// Check if topology is empty (no walls, no portals).
    [[nodiscard]] bool empty() const {
        return walls_.empty() && portals_.empty();
    }

private:
    std::vector<util::Coord>       walls_;
    std::vector<util::PortalPair>  portals_;

    /// Find the portal pair containing a given position, or nullptr.
    [[nodiscard]] const util::PortalPair* findPortalPair(util::Coord pos) const {
        for (auto& p : portals_) {
            if (p.a == pos || p.b == pos) return &p;
        }
        return nullptr;
    }
};

}  // namespace model
