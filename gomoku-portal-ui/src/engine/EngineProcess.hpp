/*
 *  Portal Gomoku UI — Engine Process
 *  Manages the engine subprocess lifecycle.
 *  Uses POSIX popen-style pipes for stdin/stdout communication.
 *
 *  Threading:  A dedicated read thread reads stdout line-by-line.
 *              Lines are buffered and dispatched to the main thread via callback.
 *  Ownership:  RAII — destructor kills the process if still alive.
 */

#pragma once

#include <atomic>
#include <filesystem>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace engine {

/// Manages the engine subprocess, providing async I/O over stdin/stdout pipes.
///
/// Usage:
///   1. Set the line callback via setLineCallback() (called on main thread drain)
///   2. Call launch() with the engine executable path
///   3. Call sendLine() to write commands
///   4. Call drainOutput() periodically from the main thread to process received lines
///   5. Call kill() or let destructor handle cleanup
///
/// The read thread pushes lines into an internal buffer.
/// The main thread calls drainOutput() to invoke the callback for each line.
class EngineProcess {
public:
    EngineProcess();
    ~EngineProcess();

    // Non-copyable, non-movable
    EngineProcess(const EngineProcess&) = delete;
    EngineProcess& operator=(const EngineProcess&) = delete;

    /// Launch engine subprocess. Returns true on success.
    /// @param execPath  Path to the engine executable (e.g., pbrain-MINT-P)
    /// @param workDir   Working directory for the subprocess (for config.toml lookup)
    bool launch(const std::filesystem::path& execPath,
                const std::filesystem::path& workDir = "");

    /// Send a command line to the engine's stdin. Appends '\n' automatically.
    void sendLine(const std::string& command);

    /// Kill the engine process immediately.
    void kill();

    /// Check if the engine process is still alive.
    [[nodiscard]] bool isAlive() const;

    /// Set the callback invoked for each line received from engine stdout.
    /// This callback is invoked from the main thread when drainOutput() is called.
    void setLineCallback(std::function<void(const std::string&)> callback);

    /// Drain buffered output lines and invoke the line callback for each.
    /// Must be called from the main thread (e.g., via Glib::signal_idle).
    /// Returns the number of lines processed.
    int drainOutput();

private:
    // Process handles
    pid_t           pid_     = -1;
    int             stdinFd_ = -1;          ///< Write end of stdin pipe
    int             stdoutFd_ = -1;         ///< Read end of stdout pipe

    // Read thread
    std::thread     readThread_;
    std::atomic<bool> running_{false};

    // Thread-safe line buffer
    std::mutex      bufferMutex_;
    std::vector<std::string> lineBuffer_;

    // Callback (invoked on main thread via drainOutput)
    std::function<void(const std::string&)> lineCallback_;

    /// Read thread entry point: blocks on stdout, pushes lines to buffer.
    void readThreadFunc();
};

}  // namespace engine
