#pragma once

#include "core/frame_context.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <cstdint>
#include <vector>

namespace osr::demo::wind_tunnel {

struct TemporalResolveSettings {
    float max_history_weight = 0.72f;
    float reactive_penalty = 0.90f;
    float motion_rejection_pixels = 12.0f;
};

struct TemporalResolveStats {
    double history_weight_mean = 0.0;
    double history_weight_min = 0.0;
    double history_weight_max = 0.0;
    double reactive_suppressed_pct = 0.0;
    double motion_suppressed_pct = 0.0;
};

[[nodiscard]] std::vector<uint32_t> ResolveTemporalDisplay(const std::vector<uint32_t>& current_display,
                                                           const std::vector<uint32_t>& previous_history,
                                                           const SyntheticFrame& current_frame,
                                                           core::Dimensions display_size,
                                                           const TemporalResolveSettings& settings,
                                                           TemporalResolveStats* stats);

} // namespace osr::demo::wind_tunnel
