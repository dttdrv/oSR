#include "reconstruction/feature_locks.h"

#include <algorithm>
#include <cmath>

namespace osr::reconstruction {

namespace {

float SaturateLocal(float value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

float Decay(float value, float rate) noexcept {
    return SaturateLocal(value * (1.0f - SaturateLocal(rate)));
}

} // namespace

FeatureLockState UpdateFeatureLock(const FeatureLockInputs& inputs,
                                   const FeatureLockSettings& settings) noexcept {
    if (inputs.reset_history) {
        return {};
    }

    const bool hard_unlock = inputs.disoccluded ||
                             inputs.reactive_value >= settings.reactive_unlock_threshold;
    if (hard_unlock) {
        const float strength = Decay(inputs.previous_strength, settings.hard_unlock_decay_rate);
        return {
            strength,
            strength > 0.001f ? inputs.previous_age : 0u,
        };
    }

    const bool stable_feature = inputs.edge_strength >= settings.min_edge_strength &&
                                inputs.history_trust >= settings.min_history_trust &&
                                std::fabs(inputs.luma_delta) <= settings.max_luma_delta &&
                                inputs.luma_variance <= settings.max_luma_variance &&
                                std::fabs(inputs.motion_pixels) <= settings.max_motion_pixels;
    if (!stable_feature) {
        const float strength = Decay(inputs.previous_strength, settings.decay_rate);
        return {
            strength,
            strength > 0.001f ? inputs.previous_age : 0u,
        };
    }

    const float rate = inputs.previous_strength > 0.0f
        ? settings.stable_reinforce_rate
        : settings.acquire_rate;
    const float strength = SaturateLocal(inputs.previous_strength +
                                         (1.0f - inputs.previous_strength) * SaturateLocal(rate));
    return {
        strength,
        std::min(inputs.previous_age + 1u, settings.max_age),
    };
}

float FeatureLockSharpeningScale(const FeatureLockState& state, float base_scale) noexcept {
    return SaturateLocal(base_scale) * (0.5f + 0.5f * SaturateLocal(state.strength));
}

} // namespace osr::reconstruction
