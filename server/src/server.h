#pragma once

#include <atomic>
#include <string>

namespace chess::server {

struct ServerConfig {
    unsigned short port = 5555;
    std::string host = "0.0.0.0";
    long long timeout = 0;
    std::size_t maxClients = 64;
    bool botsEnabled = false;
    int botDepth = 1;
    std::string botEnginePath = "stockfish";
    std::size_t maxBots = 4;
};

// Runs the server event loop on the calling thread. Blocks until stopped.
// If boundPortOut is non-null, writes the actual bound port before entering the loop
// (useful when config.port == 0 for OS-assigned port).
// Returns the bound port on success, 0 on failure.
unsigned short runServer(const ServerConfig& config, std::atomic<bool>& shutdownFlag,
                         unsigned short* boundPortOut = nullptr);

// Stops a running server (thread-safe, can be called from any thread).
void stopServer(std::atomic<bool>& shutdownFlag);

} // namespace chess::server
