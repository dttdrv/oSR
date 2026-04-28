#include "demo/wind_tunnel/sequence_metrics.h"

#include "demo/dx12_wind_tunnel/display_upscale.h"
#include "demo/wind_tunnel/synthetic_frame.h"
#include "demo/wind_tunnel/temporal_resolve.h"

#include <cmath>
#include <algorithm>
#include <numeric>

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

double MeanEdgeEnergy(const std::vector<uint32_t>& image, core::Dimensions size) noexcept {
    const size_t expected = static_cast<size_t>(size.width) * size.height;
    if (image.size() != expected || size.width < 2 || size.height < 2) {
        return 0.0;
    }
    double sum = 0.0;
    uint64_t samples = 0;
    for (uint32_t y = 0; y + 1 < size.height; ++y) {
        for (uint32_t x = 0; x + 1 < size.width; ++x) {
            const size_t idx = static_cast<size_t>(y) * size.width + x;
            const float center = LumaFromRgba8(image[idx]);
            sum += std::abs(center - LumaFromRgba8(image[idx + 1]));
            sum += std::abs(center - LumaFromRgba8(image[idx + size.width]));
            samples += 2;
        }
    }
    return samples == 0 ? 0.0 : sum / static_cast<double>(samples);
}

std::vector<double> EdgeEnergyMap(const std::vector<uint32_t>& image, core::Dimensions size) {
    const size_t expected = static_cast<size_t>(size.width) * size.height;
    std::vector<double> edges(expected, 0.0);
    if (image.size() != expected || size.width < 2 || size.height < 2) {
        return edges;
    }
    for (uint32_t y = 0; y + 1 < size.height; ++y) {
        for (uint32_t x = 0; x + 1 < size.width; ++x) {
            const size_t idx = static_cast<size_t>(y) * size.width + x;
            const float center = LumaFromRgba8(image[idx]);
            edges[idx] = std::abs(center - LumaFromRgba8(image[idx + 1])) +
                         std::abs(center - LumaFromRgba8(image[idx + size.width]));
        }
    }
    return edges;
}

double ThinFeatureContrastRatio(const std::vector<uint32_t>& spatial,
                                const std::vector<uint32_t>& temporal,
                                core::Dimensions size) {
    if (spatial.size() != temporal.size() || spatial.empty() || size.width < 2 || size.height < 2) {
        return 0.0;
    }
    const auto spatial_edges = EdgeEnergyMap(spatial, size);
    const auto temporal_edges = EdgeEnergyMap(temporal, size);
    std::vector<size_t> indices(spatial_edges.size());
    std::iota(indices.begin(), indices.end(), size_t {0});
    std::sort(indices.begin(), indices.end(), [&](size_t lhs, size_t rhs) {
        return spatial_edges[lhs] > spatial_edges[rhs];
    });
    const size_t roi_count = std::min(std::max(indices.size() / 20, size_t {64}), indices.size());
    double spatial_sum = 0.0;
    double temporal_sum = 0.0;
    for (size_t i = 0; i < roi_count; ++i) {
        const size_t idx = indices[i];
        spatial_sum += spatial_edges[idx];
        temporal_sum += temporal_edges[idx];
    }
    return spatial_sum <= 0.0 ? 0.0 : temporal_sum / spatial_sum;
}

double TextContrast(const std::vector<uint32_t>& image, core::Dimensions size, uint64_t frame_id) noexcept {
    if (image.size() != static_cast<size_t>(size.width) * size.height || image.empty()) {
        return 0.0;
    }
    double glyph_sum = 0.0;
    double panel_sum = 0.0;
    uint64_t glyph_count = 0;
    uint64_t panel_count = 0;
    for (uint32_t y = 0; y < size.height; ++y) {
        for (uint32_t x = 0; x < size.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(size.width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(size.height);
            const auto text = EvaluateSyntheticTextCoverage(u, v, frame_id, true);
            if (!text.panel) {
                continue;
            }
            const float luma = LumaFromRgba8(image[static_cast<size_t>(y) * size.width + x]);
            if (text.glyph) {
                glyph_sum += luma;
                ++glyph_count;
            } else {
                panel_sum += luma;
                ++panel_count;
            }
        }
    }
    if (glyph_count == 0 || panel_count == 0) {
        return 0.0;
    }
    return std::abs(glyph_sum / static_cast<double>(glyph_count) -
                    panel_sum / static_cast<double>(panel_count));
}

double TextReadabilityContrastRatio(const std::vector<uint32_t>& spatial,
                                    const std::vector<uint32_t>& temporal,
                                    core::Dimensions size,
                                    uint64_t frame_id) noexcept {
    const double spatial_contrast = TextContrast(spatial, size, frame_id);
    if (spatial_contrast <= 0.0) {
        return 0.0;
    }
    return TextContrast(temporal, size, frame_id) / spatial_contrast;
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
    double color_rejected_sum = 0.0;
    double color_residual_sum = 0.0;
    double depth_rejected_sum = 0.0;
    double depth_residual_sum = 0.0;
    double sharpening_amount_sum = 0.0;
    double spatial_edge_sum = 0.0;
    double temporal_edge_sum = 0.0;
    double thin_feature_contrast_sum = 0.0;
    double text_readability_sum = 0.0;
    uint32_t delta_count = 0;
    SyntheticFrame previous_frame;
    bool has_previous_frame = false;

    for (uint32_t i = 0; i < settings.frame_count; ++i) {
        SyntheticFrameSettings frame_settings;
        frame_settings.display_size = settings.display_size;
        frame_settings.render_scale = settings.render_scale;
        frame_settings.frame_id = settings.start_frame + i;
        frame_settings.reset_history = i == 0;
        auto frame = BuildSyntheticFrame(frame_settings);
        const auto spatial = dx12_wind_tunnel::UpscaleBilinear(frame.color, frame.context.render_size, frame.context.display_size);

        TemporalResolveStats stats;
        std::vector<uint32_t> temporal = spatial;
        if (!previous_temporal.empty() && has_previous_frame) {
            temporal = ResolveTemporalDisplay(spatial, previous_temporal, frame, frame.context.display_size, {}, &stats, &previous_frame);
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
            color_rejected_sum += stats.color_rejected_pct;
            color_residual_sum += stats.color_residual_mean;
            depth_rejected_sum += stats.depth_rejected_pct;
            depth_residual_sum += stats.depth_residual_mean;
            sharpening_amount_sum += stats.sharpening_amount_mean;
            spatial_edge_sum += MeanEdgeEnergy(spatial, frame.context.display_size);
            temporal_edge_sum += MeanEdgeEnergy(temporal, frame.context.display_size);
            thin_feature_contrast_sum += ThinFeatureContrastRatio(spatial, temporal, frame.context.display_size);
            text_readability_sum += TextReadabilityContrastRatio(spatial, temporal, frame.context.display_size, frame_settings.frame_id);
            ++delta_count;
        }

        previous_spatial = spatial;
        previous_temporal = temporal;
        previous_frame = std::move(frame);
        has_previous_frame = true;
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
    result.edge_preservation = spatial_edge_sum <= 0.0 ? 0.0 : temporal_edge_sum / spatial_edge_sum;
    result.thin_feature_contrast = delta_count == 0 ? 0.0 : thin_feature_contrast_sum / static_cast<double>(delta_count);
    result.text_readability_contrast = delta_count == 0 ? 0.0 : text_readability_sum / static_cast<double>(delta_count);
    result.reprojected_history_pct = delta_count == 0 ? 0.0 : reprojected_sum / static_cast<double>(delta_count);
    result.reproject_out_of_bounds_pct = delta_count == 0 ? 0.0 : reproject_oob_sum / static_cast<double>(delta_count);
    result.color_rejected_pct = delta_count == 0 ? 0.0 : color_rejected_sum / static_cast<double>(delta_count);
    result.color_residual_mean = delta_count == 0 ? 0.0 : color_residual_sum / static_cast<double>(delta_count);
    result.depth_rejected_pct = delta_count == 0 ? 0.0 : depth_rejected_sum / static_cast<double>(delta_count);
    result.depth_residual_mean = delta_count == 0 ? 0.0 : depth_residual_sum / static_cast<double>(delta_count);
    result.sharpening_amount_mean = delta_count == 0 ? 0.0 : sharpening_amount_sum / static_cast<double>(delta_count);
    result.temporal_history_weight_mean = delta_count == 0 ? 0.0 : history_weight_sum / static_cast<double>(delta_count);
    result.temporal_reactive_suppressed_pct = delta_count == 0 ? 0.0 : reactive_sum / static_cast<double>(delta_count);
    result.temporal_motion_suppressed_pct = delta_count == 0 ? 0.0 : motion_sum / static_cast<double>(delta_count);
    return result;
}

} // namespace osr::demo::wind_tunnel
