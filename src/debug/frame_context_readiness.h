#pragma once

#include "core/frame_context.h"

#include <string>
#include <vector>

namespace osr::debug {

enum class ReadinessSeverity {
    Info,
    Warning,
    Error
};

struct ReadinessItem {
    ReadinessSeverity severity = ReadinessSeverity::Info;
    std::string code;
    std::string message;
};

struct ReadinessReport {
    bool ready = false;
    std::vector<ReadinessItem> items;
};

[[nodiscard]] ReadinessReport EvaluateSrHarnessReadiness(const core::FrameContext& frame);
[[nodiscard]] std::string SummarizeReadiness(const ReadinessReport& report);
[[nodiscard]] const char* ToString(ReadinessSeverity severity) noexcept;

} // namespace osr::debug
