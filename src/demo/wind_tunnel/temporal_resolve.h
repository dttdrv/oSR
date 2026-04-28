#pragma once

#include "core/frame_context.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <cstdint>
#include <vector>

namespace osr::demo::wind_tunnel {

struct TemporalResolveSettings {
    float max_history_weight = 0.72f;
    float reactive_penalty = 0.90f;
    float motion_rejection_pixels = 3.0f;
    float color_rejection_threshold = 0.16f;
    float depth_rejection_threshold = 0.035f;
};

struct TemporalResolveStats {
    double history_weight_mean = 0.0;
    double history_weight_min = 0.0;
    double history_weight_max = 0.0;
    double reactive_suppressed_pct = 0.0;
    double motion_suppressed_pct = 0.0;
    double reactive_history_weight_mean = 0.0;
    double motion_history_weight_mean = 0.0;
    double reprojected_history_pct = 0.0;
    double reproject_out_of_bounds_pct = 0.0;
    double color_rejected_pct = 0.0;
    double color_residual_mean = 0.0;
    double depth_rejected_pct = 0.0;
    double depth_residual_mean = 0.0;
};

struct TemporalResolveDebugMaps {
    core::Dimensions display_size {};
    std::vector<float> history_weight;
    std::vector<float> color_residual;
    std::vector<float> depth_residual;
};

[[nodiscard]] std::vector<uint32_t> ResolveTemporalDisplay(const std::vector<uint32_t>& current_display,
                                                           const std::vector<uint32_t>& previous_history,
                                                           const SyntheticFrame& current_frame,
                                                           core::Dimensions display_size,
                                                           const TemporalResolveSettings& settings,
                                                           TemporalResolveStats* stats,
                                                           const SyntheticFrame* previous_frame = nullptr,
                                                           TemporalResolveDebugMaps* debug_maps = nullptr);

} // namespace osr::demo::wind_tunnel
