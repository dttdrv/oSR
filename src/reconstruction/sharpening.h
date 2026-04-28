#pragma once

namespace osr::reconstruction {

struct SharpeningSettings {
    bool enabled = false;
    float amount = 0.0f;
    float low_trust_scale = 0.15f;
    float reactive_scale = 0.25f;
    float disocclusion_scale = 0.0f;
};

[[nodiscard]] float ClampSharpness(float amount) noexcept;
[[nodiscard]] float ConfidenceGatedSharpness(float base_amount,
                                             float history_trust,
                                             float reactive_value,
                                             bool disoccluded,
                                             const SharpeningSettings& settings) noexcept;

} // namespace osr::reconstruction
