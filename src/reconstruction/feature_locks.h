#pragma once

#include <cstdint>

namespace osr::reconstruction {

struct FeatureLockSettings {
    float min_edge_strength = 0.18f;
    float min_history_trust = 0.70f;
    float max_luma_delta = 0.045f;
    float max_luma_variance = 0.0008f;
    float max_motion_pixels = 1.5f;
    float reactive_unlock_threshold = 0.20f;
    float acquire_rate = 0.22f;
    float stable_reinforce_rate = 0.12f;
    float decay_rate = 0.35f;
    float hard_unlock_decay_rate = 0.85f;
    uint32_t max_age = 32;
};

struct FeatureLockState {
    float strength = 0.0f;
    uint32_t age = 0;
};

struct FeatureLockInputs {
    float previous_strength = 0.0f;
    uint32_t previous_age = 0;
    float edge_strength = 0.0f;
    float history_trust = 0.0f;
    float luma_delta = 1.0f;
    float luma_variance = 1.0f;
    float motion_pixels = 0.0f;
    float reactive_value = 0.0f;
    bool disoccluded = false;
    bool reset_history = false;
};

[[nodiscard]] FeatureLockState UpdateFeatureLock(const FeatureLockInputs& inputs,
                                                 const FeatureLockSettings& settings) noexcept;
[[nodiscard]] float FeatureLockSharpeningScale(const FeatureLockState& state,
                                               float base_scale) noexcept;

} // namespace osr::reconstruction
