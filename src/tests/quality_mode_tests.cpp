#include "core/quality_mode.h"

#include <cmath>
#include <iostream>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(1);
    }
}

bool Near(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

} // namespace

int main() {
    using namespace osr::core;

    const Dimensions display {1920, 1200};
    const auto quality = BuildResolutionPlan(display, QualityMode::Quality);
    Require(Near(quality.render_scale, 0.66f), "Quality mode should use 66 percent scale.");
    Require(quality.render_size.width == 1267, "Quality mode should round 1920 wide output from 66 percent.");
    Require(quality.render_size.height == 792, "Quality mode should use 66 percent height.");

    const auto balanced = BuildResolutionPlan(display, QualityMode::Balanced);
    Require(Near(balanced.render_scale, 0.58f), "Balanced mode should use 58 percent scale.");
    Require(balanced.render_size.width == 1114, "Balanced mode width should round from 1920 * 0.58.");
    Require(balanced.render_size.height == 696, "Balanced mode height should round from 1200 * 0.58.");

    const auto performance = BuildResolutionPlan(display, QualityMode::Performance);
    Require(Near(performance.render_scale, 0.5f), "Performance mode should use 50 percent scale.");
    Require(performance.render_size.width == 960, "Performance mode should use 50 percent width.");
    Require(performance.render_size.height == 600, "Performance mode should use 50 percent height.");

    const auto ultra_performance = BuildResolutionPlan(display, QualityMode::UltraPerformance);
    Require(ultra_performance.render_size.width == 640, "UltraPerformance mode should use one-third width.");
    Require(ultra_performance.render_size.height == 400, "UltraPerformance mode should use one-third height.");

    const auto custom_low = BuildCustomResolutionPlan(display, 0.1f);
    Require(Near(custom_low.render_scale, 1.0f / 3.0f), "Custom scale should clamp low.");

    const auto custom_high = BuildCustomResolutionPlan(display, 2.0f);
    Require(custom_high.render_scale == 1.0f, "Custom scale should clamp high.");
    Require(custom_high.render_size.width == 1920, "Native custom width should equal display width.");

    Require(ParseQualityMode("balanced") == QualityMode::Balanced, "Lowercase mode parsing failed.");
    Require(ParseQualityMode("Quality") == QualityMode::Quality, "Title-case mode parsing failed.");
    Require(ParseQualityMode("not_a_mode") == QualityMode::Quality, "Unknown mode should default to Quality.");
    Require(std::string_view(ToString(QualityMode::UltraQuality)) == "UltraQuality", "ToString coverage failed.");

    return 0;
}
