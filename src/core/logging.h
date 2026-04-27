#pragma once

#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>

namespace osr::core {

enum class LogLevel {
    Trace = 0,
    Debug = 1,
    Info = 2,
    Warning = 3,
    Error = 4,
    Off = 5
};

class Logger {
public:
    static Logger& Instance();

    void Configure(std::filesystem::path output_path, LogLevel min_level);
    void Log(LogLevel level, uint64_t frame_id, const std::string& component, const std::string& message);

private:
    Logger() = default;

    std::mutex mutex_;
    std::ofstream stream_;
    LogLevel min_level_ = LogLevel::Info;
};

[[nodiscard]] LogLevel ParseLogLevel(const std::string& value);
[[nodiscard]] const char* ToString(LogLevel level) noexcept;

} // namespace osr::core

