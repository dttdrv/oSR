#pragma once

#include "core/frame_context.h"
#include "core/logging.h"

#include <filesystem>
#include <optional>
#include <string>

namespace osr::core {

struct RuntimeConfig {
    std::string bridge_mode = "ffx_dx12";
    LogLevel log_level = LogLevel::Info;
    bool capture_enabled = false;
    bool debug_overlay_enabled = false;
    bool trust_field_enabled = true;
    bool reactive_synthesis_enabled = true;
    float trust_decay_rate = 0.08f;
    float depth_consistency_threshold = 0.02f;
    float motion_consistency_threshold_pixels = 64.0f;
    std::optional<Float2> motion_vector_scale_override;
    std::optional<bool> depth_inverted_override;
};

[[nodiscard]] RuntimeConfig LoadConfig(const std::filesystem::path& path);
[[nodiscard]] bool ParseBool(const std::string& value);

} // namespace osr::core
