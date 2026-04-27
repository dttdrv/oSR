#pragma once

namespace osr::reconstruction {

struct DisocclusionSettings {
    float depth_threshold = 0.01f;
    float motion_threshold_pixels = 64.0f;
};

[[nodiscard]] bool IsLikelyDisoccluded(float depth_delta, float motion_length_pixels, const DisocclusionSettings& settings);

} // namespace osr::reconstruction

