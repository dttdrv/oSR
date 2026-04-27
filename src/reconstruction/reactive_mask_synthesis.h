#pragma once

#include "reconstruction/trust_field.h"

namespace osr::reconstruction {

struct ReactiveSynthesisInputs {
    float color_delta_luma = 0.0f;
    float depth_relative_delta = 0.0f;
    float motion_length_pixels = 0.0f;
    float transparency_hint = 0.0f;
};

struct ReactiveSynthesisSettings {
    float color_delta_scale = 3.0f;
    float depth_delta_scale = 8.0f;
    float motion_scale = 1.0f / 96.0f;
    float transparency_scale = 1.0f;
};

[[nodiscard]] float SynthesizeReactiveMask(const ReactiveSynthesisInputs& inputs,
                                           const ReactiveSynthesisSettings& settings) noexcept;

} // namespace osr::reconstruction

