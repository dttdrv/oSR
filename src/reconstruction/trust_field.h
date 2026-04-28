#pragma once

#include <algorithm>

namespace osr::reconstruction {

struct TrustFieldSettings {
    float max_history_weight = 0.86f;
    float min_history_weight = 0.04f;
    float depth_consistency_threshold = 0.02f;
    float motion_consistency_threshold_pixels = 64.0f;
    float color_consistency_threshold = 0.12f;
    float reactive_penalty = 0.65f;
    float disocclusion_penalty = 0.85f;
    float trust_decay_rate = 0.08f;
    float trust_recovery_floor = 0.90f;
};

struct TrustFactors {
    float previous_trust = 0.0f;
    float depth_relative_delta = 1.0f;
    float motion_length_pixels = 0.0f;
    float color_delta_luma = 1.0f;
    float reactive_value = 0.0f;
    bool disoccluded = false;
    bool reset_history = false;
};

struct TrustFieldResult {
    float history_trust = 0.0f;
    float accumulation_weight = 0.0f;
    float rejection_confidence = 1.0f;
    float evidence_trust = 0.0f;
};

[[nodiscard]] float Saturate(float value) noexcept;
[[nodiscard]] float DepthTrust(float relative_delta, const TrustFieldSettings& settings) noexcept;
[[nodiscard]] float MotionTrust(float motion_length_pixels, const TrustFieldSettings& settings) noexcept;
[[nodiscard]] float ColorTrust(float color_delta_luma, const TrustFieldSettings& settings) noexcept;
[[nodiscard]] TrustFieldResult ComputeTrustField(const TrustFactors& factors, const TrustFieldSettings& settings) noexcept;

} // namespace osr::reconstruction
