#include "core/logging.h"

#include <chrono>
#include <iomanip>
#include <sstream>

namespace osr::core {

Logger& Logger::Instance() {
    static Logger logger;
    return logger;
}

void Logger::Configure(std::filesystem::path output_path, LogLevel min_level) {
    std::lock_guard lock(mutex_);
    min_level_ = min_level;
    stream_.close();
    stream_.open(output_path, std::ios::app);
}

void Logger::Log(LogLevel level, uint64_t frame_id, const std::string& component, const std::string& message) {
    if (level < min_level_ || min_level_ == LogLevel::Off) {
        return;
    }

    std::lock_guard lock(mutex_);
    if (!stream_.is_open()) {
        stream_.open("osr_runtime.log", std::ios::app);
    }

    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);

    std::tm local_time {};
#if defined(_WIN32)
    localtime_s(&local_time, &time);
#else
    localtime_r(&time, &local_time);
#endif

    stream_ << std::put_time(&local_time, "%Y-%m-%dT%H:%M:%S")
            << " level=" << ToString(level)
            << " frame=" << frame_id
            << " component=" << component
            << " message=\"" << message << "\"\n";
    stream_.flush();
}

LogLevel ParseLogLevel(const std::string& value) {
    if (value == "trace") return LogLevel::Trace;
    if (value == "debug") return LogLevel::Debug;
    if (value == "warning" || value == "warn") return LogLevel::Warning;
    if (value == "error") return LogLevel::Error;
    if (value == "off") return LogLevel::Off;
    return LogLevel::Info;
}

const char* ToString(LogLevel level) noexcept {
    switch (level) {
    case LogLevel::Trace: return "trace";
    case LogLevel::Debug: return "debug";
    case LogLevel::Info: return "info";
    case LogLevel::Warning: return "warning";
    case LogLevel::Error: return "error";
    case LogLevel::Off:
    default: return "off";
    }
}

} // namespace osr::core

