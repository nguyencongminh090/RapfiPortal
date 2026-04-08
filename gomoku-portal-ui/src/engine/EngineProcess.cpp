/*
 *  Portal Gomoku UI — Engine Process Implementation
 *  POSIX subprocess management with bidirectional pipe I/O.
 */

#include "EngineProcess.hpp"

#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

namespace engine {

EngineProcess::EngineProcess() = default;

EngineProcess::~EngineProcess() {
    kill();
}

bool EngineProcess::launch(const std::filesystem::path& execPath,
                           const std::filesystem::path& workDir) {
    // Don't launch if already running
    if (isAlive()) {
        kill();
    }

    // Create pipes: [read_end, write_end]
    int stdinPipe[2];   // parent writes to stdinPipe[1], child reads from stdinPipe[0]
    int stdoutPipe[2];  // child writes to stdoutPipe[1], parent reads from stdoutPipe[0]

    if (pipe(stdinPipe) != 0 || pipe(stdoutPipe) != 0) {
        std::cerr << "[EngineProcess] Failed to create pipes: " << strerror(errno) << "\n";
        return false;
    }

    pid_ = fork();
    if (pid_ < 0) {
        std::cerr << "[EngineProcess] fork() failed: " << strerror(errno) << "\n";
        close(stdinPipe[0]); close(stdinPipe[1]);
        close(stdoutPipe[0]); close(stdoutPipe[1]);
        return false;
    }

    if (pid_ == 0) {
        // === CHILD PROCESS ===

        // Change working directory if specified
        if (!workDir.empty()) {
            if (chdir(workDir.c_str()) != 0) {
                _exit(127);
            }
        }

        // Redirect stdin/stdout
        dup2(stdinPipe[0], STDIN_FILENO);
        dup2(stdoutPipe[1], STDOUT_FILENO);
        dup2(stdoutPipe[1], STDERR_FILENO);  // merge stderr into stdout

        // Close unused pipe ends
        close(stdinPipe[0]);
        close(stdinPipe[1]);
        close(stdoutPipe[0]);
        close(stdoutPipe[1]);

        // Execute engine
        std::string pathStr = execPath.string();
        execl(pathStr.c_str(), pathStr.c_str(), nullptr);

        // If execl returns, it failed
        _exit(127);
    }

    // === PARENT PROCESS ===

    // Close child-side pipe ends
    close(stdinPipe[0]);
    close(stdoutPipe[1]);

    stdinFd_  = stdinPipe[1];
    stdoutFd_ = stdoutPipe[0];
    running_  = true;

    // Start read thread
    readThread_ = std::thread(&EngineProcess::readThreadFunc, this);

    return true;
}

void EngineProcess::sendLine(const std::string& command) {
    if (stdinFd_ < 0) return;

    std::string line = command + "\n";
    ssize_t written = write(stdinFd_, line.c_str(), line.size());
    if (written < 0) {
        std::cerr << "[EngineProcess] write() failed: " << strerror(errno) << "\n";
    }
}

void EngineProcess::kill() {
    running_ = false;

    if (stdinFd_ >= 0) {
        close(stdinFd_);
        stdinFd_ = -1;
    }

    if (pid_ > 0) {
        ::kill(pid_, SIGTERM);

        // Wait briefly for graceful exit, then force kill
        int status;
        pid_t result = waitpid(pid_, &status, WNOHANG);
        if (result == 0) {
            // Still running after SIGTERM, wait 100ms then SIGKILL
            usleep(100'000);
            ::kill(pid_, SIGKILL);
            waitpid(pid_, &status, 0);
        }
        pid_ = -1;
    }

    if (stdoutFd_ >= 0) {
        close(stdoutFd_);
        stdoutFd_ = -1;
    }

    if (readThread_.joinable()) {
        readThread_.join();
    }
}

bool EngineProcess::isAlive() const {
    if (pid_ <= 0) return false;
    int status;
    pid_t result = waitpid(pid_, &status, WNOHANG);
    return result == 0;  // 0 means child still running
}

void EngineProcess::setLineCallback(std::function<void(const std::string&)> callback) {
    lineCallback_ = std::move(callback);
}

int EngineProcess::drainOutput() {
    std::vector<std::string> lines;
    {
        std::lock_guard<std::mutex> lock(bufferMutex_);
        lines.swap(lineBuffer_);
    }

    if (lineCallback_) {
        for (auto& line : lines)
            lineCallback_(line);
    }

    return static_cast<int>(lines.size());
}

void EngineProcess::readThreadFunc() {
    // Read stdout line-by-line using a FILE* wrapper for buffered I/O
    FILE* fp = fdopen(stdoutFd_, "r");
    if (!fp) {
        std::cerr << "[EngineProcess] fdopen() failed\n";
        running_ = false;
        return;
    }

    char buf[4096];
    while (running_ && fgets(buf, sizeof(buf), fp)) {
        std::string line(buf);

        // Strip trailing newline/carriage-return
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
            line.pop_back();

        if (!line.empty()) {
            std::lock_guard<std::mutex> lock(bufferMutex_);
            lineBuffer_.push_back(std::move(line));
        }
    }

    // Don't fclose — stdoutFd_ may be closed by kill() already.
    // fdopen transfers ownership, but we handle the fd lifecycle in kill().
    running_ = false;
}

}  // namespace engine
