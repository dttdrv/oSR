#include "demo/wind_tunnel/sequence_metrics.h"

#include "demo/dx12_wind_tunnel/display_upscale.h"
#include "demo/wind_tunnel/synthetic_frame.h"
#include "demo/wind_tunnel/temporal_resolve.h"

#include <cmath>

namespace osr::demo::wind_tunnel {

float LumaFromRgba8(uint32_t rgba) noexcept {
    const float r = static_cast<float>((rgba >> 16) & 0xffu) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>(rgba & 0xffu) / 255.0f;
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
}

double MeanAbsoluteLumaDelta(const std::vector<uint32_t>& lhs,
                             const std::vector<uint32_t>& rhs) noexcept {
    if (lhs.size() != rhs.size() || lhs.empty()) {
        return 0.0;
    }
    double sum = 0.0;
    for (size_t i = 0; i < lhs.size(); ++i) {
        sum += std::abs(LumaFromRgba8(lhs[i]) - LumaFromRgba8(rhs[i]));
    }
    return sum / static_cast<double>(lhs.size());
}

SequenceMetricsResult RunSequenceMetrics(const SequenceMetricsSettings& settings) {
    SequenceMetricsResult result;
    if (!settings.display_size.IsValid() || settings.frame_count < 2) {
        return result;
    }

    std::vector<uint32_t> previous_spatial;
    std::vector<uint32_t> previous_temporal;
    double spatial_delta_sum = 0.0;
    double temporal_delta_sum = 0.0;
    double history_weight_sum = 0.0;
    double reactive_sum = 0.0;
    double motion_sum = 0.0;
    double reactive_history_weight_sum = 0.0;
    double motion_history_weight_sum = 0.0;
    double reprojected_sum = 0.0;
    double reproject_oob_sum = 0.0;
    uint32_t delta_count = 0;

    for (uint32_t i = 0; i < settings.frame_count; ++i) {
        SyntheticFrameSettings frame_settings;
        frame_settings.display_size = settings.display_size;
        frame_settings.render_scale = settings.render_scale;
        frame_settings.frame_id = settings.start_frame + i;
        frame_settings.reset_history = i == 0;
        auto frame = BuildSyntheticFrame(frame_settings);
        const auto spatial = dx12_wind_tunnel::UpscaleNearest(frame.color, frame.context.render_size, frame.context.display_size);

        TemporalResolveStats stats;
        std::vector<uint32_t> temporal = spatial;
        if (!previous_temporal.empty()) {
            temporal = ResolveTemporalDisplay(spatial, previous_temporal, frame, frame.context.display_size, {}, &stats);
        }

        if (!previous_spatial.empty()) {
            spatial_delta_sum += MeanAbsoluteLumaDelta(spatial, previous_spatial);
            temporal_delta_sum += MeanAbsoluteLumaDelta(temporal, previous_temporal);
            history_weight_sum += stats.history_weight_mean;
            reactive_sum += stats.reactive_suppressed_pct;
            motion_sum += stats.motion_suppressed_pct;
            reactive_history_weight_sum += stats.reactive_history_weight_mean;
            motion_history_weight_sum += stats.motion_history_weight_mean;
            reprojected_sum += stats.reprojected_history_pct;
            reproject_oob_sum += stats.reproject_out_of_bounds_pct;
            ++delta_count;
        }

        previous_spatial = spatial;
        previous_temporal = temporal;
    }

    result.frames = settings.frame_count;
    result.spatial_frame_delta_mean = delta_count == 0 ? 0.0 : spatial_delta_sum / static_cast<double>(delta_count);
    result.temporal_frame_delta_mean = delta_count == 0 ? 0.0 : temporal_delta_sum / static_cast<double>(delta_count);
    result.temporal_delta_ratio = result.spatial_frame_delta_mean <= 0.0
        ? 1.0
        : result.temporal_frame_delta_mean / result.spatial_frame_delta_mean;
    result.stability_improvement_pct = (1.0 - result.temporal_delta_ratio) * 100.0;
    result.ghost_score = delta_count == 0 ? 0.0 : motion_history_weight_sum / static_cast<double>(delta_count);
    result.reactive_trail_score = delta_count == 0 ? 0.0 : reactive_history_weight_sum / static_cast<double>(delta_count);
    result.reprojected_history_pct = delta_count == 0 ? 0.0 : reprojected_sum / static_cast<double>(delta_count);
    result.reproject_out_of_bounds_pct = delta_count == 0 ? 0.0 : reproject_oob_sum / static_cast<double>(delta_count);
    result.temporal_history_weight_mean = delta_count == 0 ? 0.0 : history_weight_sum / static_cast<double>(delta_count);
    result.temporal_reactive_suppressed_pct = delta_count == 0 ? 0.0 : reactive_sum / static_cast<double>(delta_count);
    result.temporal_motion_suppressed_pct = delta_count == 0 ? 0.0 : motion_sum / static_cast<double>(delta_count);
    return result;
}

} // namespace osr::demo::wind_tunnel
