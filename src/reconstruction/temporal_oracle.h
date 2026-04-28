#pragma once

#include "reconstruction/trust_field.h"

#include <vector>

namespace osr::reconstruction {

struct TemporalOracleSample {
    float current_luma = 0.0f;
    float history_luma = 0.0f;
    float current_depth = 1.0f;
    float reprojected_depth = 1.0f;
    float motion_x_pixels = 0.0f;
    float motion_y_pixels = 0.0f;
    float reactive_value = 0.0f;
    bool disoccluded = false;
    bool reset_history = false;
    float previous_trust = 1.0f;
};

struct TemporalOracleResult {
    TrustFieldResult trust;
    float resolved_luma = 0.0f;
    float current_weight = 1.0f;
};

[[nodiscard]] float RelativeDepthDelta(float current_depth, float reprojected_depth) noexcept;
[[nodiscard]] float MotionLengthPixels(float motion_x_pixels, float motion_y_pixels) noexcept;
[[nodiscard]] TemporalOracleResult ResolveTemporalSample(const TemporalOracleSample& sample,
                                                         const TrustFieldSettings& settings) noexcept;
[[nodiscard]] float Mean(const std::vector<float>& values) noexcept;
[[nodiscard]] float Variance(const std::vector<float>& values) noexcept;

} // namespace osr::reconstruction

