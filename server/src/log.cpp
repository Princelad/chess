#include "log.h"

#include <atomic>
#include <chrono>
#include <ctime>
#include <fstream>
#include <iostream>
#include <mutex>
#include <sstream>

namespace {

std::atomic<LogLevel> gLevel{LogLevel::Info};
std::ofstream gLogFile;
std::mutex gMutex;

const char* levelStr(LogLevel level)
{
    switch (level) {
        case LogLevel::Info:  return "INFO";
        case LogLevel::Warn:  return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "???";
}

std::ostream& levelStream(LogLevel level)
{
    return (level == LogLevel::Error) ? std::cerr : std::cout;
}

std::string timestamp()
{
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm;
    localtime_r(&time, &tm);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm);
    return buf;
}

} // anonymous namespace

void logSetFile(const std::string& path)
{
    std::lock_guard<std::mutex> lock(gMutex);
    gLogFile.open(path, std::ios::app);
    if (!gLogFile.is_open())
        std::cerr << "[ERROR] Failed to open log file: " << path << "\n";
}

void logSetLevel(LogLevel level)
{
    gLevel.store(level, std::memory_order_relaxed);
}

void logMsg(LogLevel level, const char* file, int line, const std::string& msg)
{
    if (level < gLevel.load(std::memory_order_relaxed))
        return;

    std::lock_guard<std::mutex> lock(gMutex);

    // Strip path prefix — show only filename
    std::string filename = file;
    auto pos = filename.find_last_of('/');
    if (pos != std::string::npos)
        filename = filename.substr(pos + 1);

    std::ostringstream oss;
    oss << "[" << levelStr(level) << "] " << timestamp() << " " << msg
        << " (" << filename << ":" << line << ")\n";

    std::string formatted = oss.str();
    levelStream(level) << formatted;

    if (gLogFile.is_open())
        gLogFile << formatted;
}
