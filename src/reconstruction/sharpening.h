#pragma once

namespace osr::reconstruction {

struct SharpeningSettings {
    bool enabled = false;
    float amount = 0.0f;
};

[[nodiscard]] float ClampSharpness(float amount) noexcept;

} // namespace osr::reconstruction

