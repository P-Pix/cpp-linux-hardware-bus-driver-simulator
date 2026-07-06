#include "lhbd/utils/Logger.hpp"

#include <chrono>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace lhbd::utils {

namespace {

const char* levelName(LogLevel level) {
    switch (level) {
        case LogLevel::Debug: return "DEBUG";
        case LogLevel::Info: return "INFO";
        case LogLevel::Warn: return "WARN";
        case LogLevel::Error: return "ERROR";
    }
    return "UNKNOWN";
}

std::string timestamp() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
    gmtime_r(&time, &tm);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard lock(mutex_);
    level_ = level;
}

void Logger::debug(const std::string& message) { log(LogLevel::Debug, message); }
void Logger::info(const std::string& message) { log(LogLevel::Info, message); }
void Logger::warn(const std::string& message) { log(LogLevel::Warn, message); }
void Logger::error(const std::string& message) { log(LogLevel::Error, message); }

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard lock(mutex_);
    if (static_cast<int>(level) < static_cast<int>(level_)) {
        return;
    }
    std::cerr << timestamp() << " [" << levelName(level) << "] " << message << '\n';
}

} // namespace lhbd::utils
