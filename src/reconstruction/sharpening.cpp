#include "reconstruction/sharpening.h"

#include <algorithm>
#include <cmath>

namespace osr::reconstruction {

float ClampSharpness(float amount) noexcept {
    return std::clamp(amount, 0.0f, 1.0f);
}

float ConfidenceGatedSharpness(float base_amount,
                               float history_trust,
                               float reactive_value,
                               bool disoccluded,
                               const SharpeningSettings& settings) noexcept {
    if (!settings.enabled || disoccluded) {
        return 0.0f;
    }

    const float base = ClampSharpness(base_amount);
    const float trust = ClampSharpness(history_trust);
    const float reactive = ClampSharpness(reactive_value);
    const float trust_scale = std::lerp(settings.low_trust_scale, 1.0f, trust);
    const float reactive_scale = std::lerp(1.0f, settings.reactive_scale, reactive);
    return ClampSharpness(base * trust_scale * reactive_scale);
}

} // namespace osr::reconstruction
