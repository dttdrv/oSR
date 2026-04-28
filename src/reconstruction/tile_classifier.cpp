#include "reconstruction/tile_classifier.h"

#include <algorithm>
#include <cmath>

namespace osr::reconstruction {

const char* ToString(TileClass classification) noexcept {
    switch (classification) {
    case TileClass::Invalid: return "Invalid";
    case TileClass::Stable: return "Stable";
    case TileClass::ShimmerRisk: return "ShimmerRisk";
    case TileClass::MotionRisk: return "MotionRisk";
    case TileClass::ReactiveRisk: return "ReactiveRisk";
    case TileClass::DisocclusionRisk: return "DisocclusionRisk";
    case TileClass::Reset: return "Reset";
    default: return "Unknown";
    }
}

uint32_t MinimumSamplesForRatio(float ratio, uint32_t sample_count) noexcept {
    if (sample_count == 0) {
        return 0;
    }
    return static_cast<uint32_t>(std::ceil(ratio * static_cast<float>(sample_count)));
}

TileStats ClassifyTile(const std::vector<TemporalOracleSample>& samples,
                       const TrustFieldSettings& trust_settings,
                       const TileClassifierSettings& tile_settings) {
    TileStats stats;
    if (samples.empty()) {
        stats.min_trust = 0.0f;
        stats.classification = TileClass::Invalid;
        return stats;
    }

    std::vector<float> current_luma;
    current_luma.reserve(samples.size());

    float trust_sum = 0.0f;
    uint32_t disoccluded_count = 0;
    for (const auto& sample : samples) {
        const auto result = ResolveTemporalSample(sample, trust_settings);
        trust_sum += result.trust.history_trust;
        stats.min_trust = std::min(stats.min_trust, result.trust.history_trust);
        stats.max_motion_pixels = std::max(stats.max_motion_pixels,
            MotionLengthPixels(sample.motion_x_pixels, sample.motion_y_pixels));
        stats.max_reactive = std::max(stats.max_reactive, sample.reactive_value);
        stats.has_reset = stats.has_reset || sample.reset_history;
        if (sample.disoccluded) {
            ++disoccluded_count;
        }
        current_luma.push_back(sample.current_luma);
    }

    stats.average_trust = trust_sum / static_cast<float>(samples.size());
    stats.luma_variance = Variance(current_luma);
    stats.disocclusion_ratio = static_cast<float>(disoccluded_count) / static_cast<float>(samples.size());

    if (stats.has_reset) {
        stats.classification = TileClass::Reset;
    } else if (stats.disocclusion_ratio >= tile_settings.disocclusion_ratio_threshold) {
        stats.classification = TileClass::DisocclusionRisk;
    } else if (stats.max_reactive >= tile_settings.reactive_risk_threshold) {
        stats.classification = TileClass::ReactiveRisk;
    } else if (stats.max_motion_pixels >= tile_settings.motion_risk_threshold_pixels) {
        stats.classification = TileClass::MotionRisk;
    } else if (stats.average_trust < tile_settings.stable_min_average_trust ||
               stats.luma_variance >= tile_settings.shimmer_variance_threshold) {
        stats.classification = TileClass::ShimmerRisk;
    } else {
        stats.classification = TileClass::Stable;
    }

    return stats;
}

} // namespace osr::reconstruction
