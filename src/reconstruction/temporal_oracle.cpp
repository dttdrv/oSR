#include "reconstruction/temporal_oracle.h"

#include <cmath>

namespace osr::reconstruction {

float RelativeDepthDelta(float current_depth, float reprojected_depth) noexcept {
    const float denominator = std::max(std::fabs(current_depth), 0.0001f);
    return std::fabs(current_depth - reprojected_depth) / denominator;
}

float MotionLengthPixels(float motion_x_pixels, float motion_y_pixels) noexcept {
    return std::sqrt(motion_x_pixels * motion_x_pixels + motion_y_pixels * motion_y_pixels);
}

TemporalOracleResult ResolveTemporalSample(const TemporalOracleSample& sample,
                                           const TrustFieldSettings& settings) noexcept {
    TrustFactors factors;
    factors.previous_trust = sample.previous_trust;
    factors.depth_relative_delta = RelativeDepthDelta(sample.current_depth, sample.reprojected_depth);
    factors.motion_length_pixels = MotionLengthPixels(sample.motion_x_pixels, sample.motion_y_pixels);
    factors.color_delta_luma = sample.current_luma - sample.history_luma;
    factors.reactive_value = sample.reactive_value;
    factors.disoccluded = sample.disoccluded;
    factors.reset_history = sample.reset_history;

    const auto trust = ComputeTrustField(factors, settings);
    const float history_weight = trust.accumulation_weight;
    const float current_weight = 1.0f - history_weight;
    return {
        trust,
        sample.current_luma * current_weight + sample.history_luma * history_weight,
        current_weight
    };
}

float Mean(const std::vector<float>& values) noexcept {
    if (values.empty()) {
        return 0.0f;
    }
    float sum = 0.0f;
    for (const float value : values) {
        sum += value;
    }
    return sum / static_cast<float>(values.size());
}

float Variance(const std::vector<float>& values) noexcept {
    if (values.size() < 2) {
        return 0.0f;
    }
    const float mean = Mean(values);
    float sum = 0.0f;
    for (const float value : values) {
        const float delta = value - mean;
        sum += delta * delta;
    }
    return sum / static_cast<float>(values.size());
}

} // namespace osr::reconstruction

