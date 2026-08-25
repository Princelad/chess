#include <atomic>
#include <csignal>
#include <cstdlib>
#include <iostream>
#include <string>

#include "log.h"
#include "server.h"

static std::atomic<bool> running{true};

static void signalHandler(int) { chess::server::stopServer(running); }

static void printUsage(const char* prog)
{
    std::cout << "Usage: " << prog << " [options]\n"
              << "  --port <N>        Listen port (default: 5555)\n"
              "  --host <addr>     Bind address (default: 0.0.0.0)\n"
              "  --max-clients <N> Max simultaneous clients (default: 64)\n"
              "  --timeout <secs>  Idle timeout in seconds (default: 0 = disabled)\n"
              "  --log-file <path> Log to file in addition to stdout\n"
              "  --log-level <L>   Min log level: info, warn, error (default: info)\n"
              "  --help            Show this help\n";
}

int main(int argc, char* argv[])
{
    chess::server::ServerConfig config;
    std::string logFile;
    LogLevel logLevel = LogLevel::Info;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help") {
            printUsage(argv[0]);
            return 0;
        } else if (arg == "--port" && i + 1 < argc) {
            int p = std::atoi(argv[++i]);
            if (p < 1 || p > 65535) {
                LOG_ERROR("Port must be 1-65535");
                return 1;
            }
            config.port = static_cast<unsigned short>(p);
        } else if (arg == "--host" && i + 1 < argc) {
            config.host = argv[++i];
        } else if (arg == "--max-clients" && i + 1 < argc) {
            int m = std::atoi(argv[++i]);
            if (m < 1) {
                LOG_ERROR("Max clients must be at least 1");
                return 1;
            }
            config.maxClients = static_cast<std::size_t>(m);
        } else if (arg == "--timeout" && i + 1 < argc) {
            config.timeout = std::atoll(argv[++i]);
            if (config.timeout < 0) {
                LOG_ERROR("Timeout must be non-negative");
                return 1;
            }
        } else if (arg == "--log-file" && i + 1 < argc) {
            logFile = argv[++i];
        } else if (arg == "--log-level" && i + 1 < argc) {
            std::string level = argv[++i];
            if (level == "warn") logLevel = LogLevel::Warn;
            else if (level == "error") logLevel = LogLevel::Error;
            else if (level != "info") {
                LOG_ERROR("Invalid log level: " + level);
                return 1;
            }
        } else {
            LOG_ERROR("Unknown option: " + arg);
            printUsage(argv[0]);
            return 1;
        }
    }

    logSetLevel(logLevel);
    if (!logFile.empty())
        logSetFile(logFile);

    std::signal(SIGINT, signalHandler);

    auto port = chess::server::runServer(config, running);
    return port == 0 ? 1 : 0;
}
