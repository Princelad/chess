#pragma once

#include <string>

enum class LogLevel : int { Info, Warn, Error };

void logSetFile(const std::string& path);
void logSetLevel(LogLevel level);
void logMsg(LogLevel level, const char* file, int line, const std::string& msg);

#define LOG_INFO(msg)  logMsg(LogLevel::Info,  __FILE__, __LINE__, (msg))
#define LOG_WARN(msg)  logMsg(LogLevel::Warn,  __FILE__, __LINE__, (msg))
#define LOG_ERROR(msg) logMsg(LogLevel::Error, __FILE__, __LINE__, (msg))
