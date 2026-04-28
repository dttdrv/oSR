#include "core/quality_mode.h"

#include <algorithm>
#include <cmath>

namespace osr::core {

const char* ToString(QualityMode mode) noexcept {
    switch (mode) {
    case QualityMode::Native: return "Native";
    case QualityMode::UltraQuality: return "UltraQuality";
    case QualityMode::Quality: return "Quality";
    case QualityMode::Balanced: return "Balanced";
    case QualityMode::Performance: return "Performance";
    case QualityMode::UltraPerformance: return "UltraPerformance";
    case QualityMode::Custom: return "Custom";
    default: return "Unknown";
    }
}

float DefaultRenderScale(QualityMode mode) noexcept {
    switch (mode) {
    case QualityMode::Native: return 1.0f;
    case QualityMode::UltraQuality: return 0.77f;
    case QualityMode::Quality: return 0.66f;
    case QualityMode::Balanced: return 0.58f;
    case QualityMode::Performance: return 0.5f;
    case QualityMode::UltraPerformance: return 1.0f / 3.0f;
    case QualityMode::Custom:
    default: return 0.66f;
    }
}

QualityMode ParseQualityMode(std::string_view value) noexcept {
    if (value == "native" || value == "Native") return QualityMode::Native;
    if (value == "ultra_quality" || value == "UltraQuality") return QualityMode::UltraQuality;
    if (value == "quality" || value == "Quality") return QualityMode::Quality;
    if (value == "balanced" || value == "Balanced") return QualityMode::Balanced;
    if (value == "performance" || value == "Performance") return QualityMode::Performance;
    if (value == "ultra_performance" || value == "UltraPerformance") return QualityMode::UltraPerformance;
    if (value == "custom" || value == "Custom") return QualityMode::Custom;
    return QualityMode::Quality;
}

float ClampRenderScale(float scale) noexcept {
    return std::clamp(scale, 1.0f / 3.0f, 1.0f);
}

ResolutionPlan BuildCustomResolutionPlan(Dimensions display_size, float render_scale) noexcept {
    const float scale = ClampRenderScale(render_scale);
    return {
        QualityMode::Custom,
        display_size,
        {
            static_cast<uint32_t>(std::max(1.0f, std::round(static_cast<float>(display_size.width) * scale))),
            static_cast<uint32_t>(std::max(1.0f, std::round(static_cast<float>(display_size.height) * scale)))
        },
        scale
    };
}

ResolutionPlan BuildResolutionPlan(Dimensions display_size, QualityMode mode) noexcept {
    auto plan = BuildCustomResolutionPlan(display_size, DefaultRenderScale(mode));
    plan.mode = mode;
    return plan;
}

} // namespace osr::core
