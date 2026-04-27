#pragma once

#include "core/frame_context.h"

#include <optional>
#include <string>

namespace osr::profiles {

struct GameProfile {
    std::string name;
    std::string executable;
    std::optional<core::Float2> motion_vector_scale_override;
    std::optional<bool> depth_inverted_override;
    bool capture_enabled = false;
};

} // namespace osr::profiles

