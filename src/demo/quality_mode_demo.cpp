#include "core/quality_mode.h"

#include <iostream>

int main() {
    const osr::core::Dimensions display {1920, 1200};
    const osr::core::QualityMode modes[] = {
        osr::core::QualityMode::Native,
        osr::core::QualityMode::UltraQualityPlus,
        osr::core::QualityMode::UltraQuality,
        osr::core::QualityMode::Quality,
        osr::core::QualityMode::Balanced,
        osr::core::QualityMode::Performance,
        osr::core::QualityMode::UltraPerformance
    };

    for (const auto mode : modes) {
        const auto plan = osr::core::BuildResolutionPlan(display, mode);
        std::cout << osr::core::ToString(mode)
                  << ",scale=" << plan.render_scale
                  << ",render=" << plan.render_size.width << "x" << plan.render_size.height
                  << ",display=" << plan.display_size.width << "x" << plan.display_size.height
                  << "\n";
    }

    const auto slider = osr::core::BuildCustomResolutionPlan(display, 0.72f);
    std::cout << "Slider"
              << ",scale=" << slider.render_scale
              << ",render=" << slider.render_size.width << "x" << slider.render_size.height
              << ",display=" << slider.display_size.width << "x" << slider.display_size.height
              << "\n";

    return 0;
}
