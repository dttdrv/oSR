#include "reconstruction/trust_field.h"

#include <cmath>

namespace osr::reconstruction {

float Saturate(float value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

float DepthTrust(float relative_delta, const TrustFieldSettings& settings) noexcept {
    if (settings.depth_consistency_threshold <= 0.0f) {
        return 0.0f;
    }
    return Saturate(1.0f - (relative_delta / settings.depth_consistency_threshold));
}

float MotionTrust(float motion_length_pixels, const TrustFieldSettings& settings) noexcept {
    if (settings.motion_consistency_threshold_pixels <= 0.0f) {
        return 0.0f;
    }
    return Saturate(1.0f - (std::fabs(motion_length_pixels) / settings.motion_consistency_threshold_pixels));
}

float ColorTrust(float color_delta_luma, const TrustFieldSettings& settings) noexcept {
    if (settings.color_consistency_threshold <= 0.0f) {
        return 0.0f;
    }
    return Saturate(1.0f - (std::fabs(color_delta_luma) / settings.color_consistency_threshold));
}

TrustFieldResult ComputeTrustField(const TrustFactors& factors, const TrustFieldSettings& settings) noexcept {
    if (factors.reset_history) {
        return {};
    }

    const float depth_trust = DepthTrust(factors.depth_relative_delta, settings);
    const float motion_trust = MotionTrust(factors.motion_length_pixels, settings);
    const float color_trust = ColorTrust(factors.color_delta_luma, settings);
    const float reactive_trust = Saturate(1.0f - factors.reactive_value * settings.reactive_penalty);
    const float disocclusion_trust = factors.disoccluded ? Saturate(1.0f - settings.disocclusion_penalty) : 1.0f;
    const float decayed_previous = Saturate(factors.previous_trust * (1.0f - settings.trust_decay_rate));
    const float evidence = depth_trust * motion_trust * color_trust * reactive_trust * disocclusion_trust;

    // Good current evidence should be able to rebuild trust after temporary uncertainty;
    // bad evidence must still force rejection immediately.
    const float memory = std::max(decayed_previous, Saturate(settings.trust_recovery_floor));
    const float combined = evidence * memory;
    const float accumulation = combined <= 0.00001f
        ? 0.0f
        : std::clamp(combined * settings.max_history_weight,
                     settings.min_history_weight,
                     settings.max_history_weight);

    return {
        combined,
        accumulation,
        Saturate(1.0f - combined),
        evidence
    };
}

} // namespace osr::reconstruction
