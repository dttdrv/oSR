#include "reconstruction/sharpening.h"

#include <algorithm>

namespace osr::reconstruction {

float ClampSharpness(float amount) noexcept {
    return std::clamp(amount, 0.0f, 1.0f);
}

} // namespace osr::reconstruction

