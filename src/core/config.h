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
    std::optional<Float2> motion_vector_scale_override;
    std::optional<bool> depth_inverted_override;
};

[[nodiscard]] RuntimeConfig LoadConfig(const std::filesystem::path& path);
[[nodiscard]] bool ParseBool(const std::string& value);

} // namespace osr::core

