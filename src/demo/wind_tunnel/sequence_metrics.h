#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <vector>

namespace osr::demo::wind_tunnel {

struct SequenceMetricsSettings {
    core::Dimensions display_size {1280, 800};
    float render_scale = 2.0f / 3.0f;
    uint64_t start_frame = 1;
    uint32_t frame_count = 32;
};

struct SequenceMetricsResult {
    uint32_t frames = 0;
    double spatial_frame_delta_mean = 0.0;
    double temporal_frame_delta_mean = 0.0;
    double temporal_delta_ratio = 1.0;
    double stability_improvement_pct = 0.0;
    double ghost_score = 0.0;
    double reactive_trail_score = 0.0;
    double edge_preservation = 0.0;
    double reprojected_history_pct = 0.0;
    double reproject_out_of_bounds_pct = 0.0;
    double color_rejected_pct = 0.0;
    double color_residual_mean = 0.0;
    double temporal_history_weight_mean = 0.0;
    double temporal_reactive_suppressed_pct = 0.0;
    double temporal_motion_suppressed_pct = 0.0;
};

[[nodiscard]] float LumaFromRgba8(uint32_t rgba) noexcept;
[[nodiscard]] double MeanAbsoluteLumaDelta(const std::vector<uint32_t>& lhs,
                                           const std::vector<uint32_t>& rhs) noexcept;
[[nodiscard]] double MeanEdgeEnergy(const std::vector<uint32_t>& image,
                                    core::Dimensions size) noexcept;

[[nodiscard]] SequenceMetricsResult RunSequenceMetrics(const SequenceMetricsSettings& settings);

} // namespace osr::demo::wind_tunnel
