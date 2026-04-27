#include "reconstruction/reactive_mask_synthesis.h"

#include <cmath>

namespace osr::reconstruction {

float SynthesizeReactiveMask(const ReactiveSynthesisInputs& inputs,
                             const ReactiveSynthesisSettings& settings) noexcept {
    const float color_term = std::fabs(inputs.color_delta_luma) * settings.color_delta_scale;
    const float depth_term = std::fabs(inputs.depth_relative_delta) * settings.depth_delta_scale;
    const float motion_term = std::fabs(inputs.motion_length_pixels) * settings.motion_scale;
    const float transparency_term = inputs.transparency_hint * settings.transparency_scale;

    return Saturate(std::max(std::max(color_term, depth_term), std::max(motion_term, transparency_term)));
}

} // namespace osr::reconstruction

