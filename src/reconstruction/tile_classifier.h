#pragma once

#include "reconstruction/temporal_oracle.h"

#include <cstdint>
#include <vector>

namespace osr::reconstruction {

enum class TileClass {
    Invalid,
    Stable,
    ShimmerRisk,
    MotionRisk,
    ReactiveRisk,
    DisocclusionRisk,
    Reset
};

struct TileClassifierSettings {
    uint32_t tile_width = 8;
    uint32_t tile_height = 8;
    float stable_min_average_trust = 0.72f;
    float shimmer_variance_threshold = 0.00025f;
    float motion_risk_threshold_pixels = 24.0f;
    float reactive_risk_threshold = 0.35f;
    float disocclusion_ratio_threshold = 0.08f;
};

struct TileStats {
    TileClass classification = TileClass::Stable;
    float average_trust = 0.0f;
    float min_trust = 1.0f;
    float max_motion_pixels = 0.0f;
    float max_reactive = 0.0f;
    float luma_variance = 0.0f;
    float disocclusion_ratio = 0.0f;
    bool has_reset = false;
};

[[nodiscard]] const char* ToString(TileClass classification) noexcept;
[[nodiscard]] uint32_t MinimumSamplesForRatio(float ratio, uint32_t sample_count) noexcept;
[[nodiscard]] TileStats ClassifyTile(const std::vector<TemporalOracleSample>& samples,
                                     const TrustFieldSettings& trust_settings,
                                     const TileClassifierSettings& tile_settings);

} // namespace osr::reconstruction
