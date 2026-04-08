/*
 *  Portal Gomoku UI — Engine State
 *  Lifecycle states for the engine subprocess connection.
 */

#pragma once

namespace engine {

/// Engine connection lifecycle states.
/// Transitions are enforced by EngineController.
enum class EngineState {
    Disconnected,   ///< No engine process running
    Idle,           ///< Connected, waiting for commands
    Thinking,       ///< Engine is computing (BEGIN/TURN/BOARD sent)
    Stopping        ///< STOP sent, waiting for best-so-far move
};

/// Convert EngineState to a human-readable string (for logs/UI).
[[nodiscard]] inline const char* toString(EngineState s) {
    switch (s) {
    case EngineState::Disconnected: return "Disconnected";
    case EngineState::Idle:         return "Idle";
    case EngineState::Thinking:     return "Thinking";
    case EngineState::Stopping:     return "Stopping";
    }
    return "Unknown";
}

}  // namespace engine
