#pragma once

#include "core/frame_context.h"

#include <string_view>

namespace osr::core {

enum class QualityMode {
    Native,
    UltraQualityPlus,
    UltraQuality,
    Quality,
    Balanced,
    Performance,
    UltraPerformance,
    Custom
};

struct ResolutionPlan {
    QualityMode mode = QualityMode::Quality;
    Dimensions display_size = {};
    Dimensions render_size = {};
    float render_scale = 1.0f / 1.7f;
};

[[nodiscard]] const char* ToString(QualityMode mode) noexcept;
[[nodiscard]] float DefaultRenderScale(QualityMode mode) noexcept;
[[nodiscard]] QualityMode ParseQualityMode(std::string_view value) noexcept;
[[nodiscard]] float ClampRenderScale(float scale) noexcept;
[[nodiscard]] ResolutionPlan BuildResolutionPlan(Dimensions display_size, QualityMode mode) noexcept;
[[nodiscard]] ResolutionPlan BuildCustomResolutionPlan(Dimensions display_size, float render_scale) noexcept;

} // namespace osr::core
