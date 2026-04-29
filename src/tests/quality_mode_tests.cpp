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
    const auto ultra_quality_plus = BuildResolutionPlan(display, QualityMode::UltraQualityPlus);
    Require(Near(ultra_quality_plus.render_scale, 1.0f / 1.3f), "UltraQualityPlus should match XeSS 1.3x scale.");
    Require(ultra_quality_plus.render_size.width == 1477, "UltraQualityPlus width should round 1920 / 1.3.");
    Require(ultra_quality_plus.render_size.height == 923, "UltraQualityPlus height should round 1200 / 1.3.");

    const auto ultra_quality = BuildResolutionPlan(display, QualityMode::UltraQuality);
    Require(Near(ultra_quality.render_scale, 1.0f / 1.5f), "UltraQuality should match XeSS 1.5x scale.");
    Require(ultra_quality.render_size.width == 1280, "UltraQuality width should match 1920 / 1.5.");
    Require(ultra_quality.render_size.height == 800, "UltraQuality height should match 1200 / 1.5.");

    const auto quality = BuildResolutionPlan(display, QualityMode::Quality);
    Require(Near(quality.render_scale, 1.0f / 1.7f), "Quality mode should match XeSS 1.7x scale.");
    Require(quality.render_size.width == 1129, "Quality mode should round 1920 / 1.7.");
    Require(quality.render_size.height == 706, "Quality mode should round 1200 / 1.7.");

    const auto balanced = BuildResolutionPlan(display, QualityMode::Balanced);
    Require(Near(balanced.render_scale, 0.5f), "Balanced mode should match XeSS 2.0x scale.");
    Require(balanced.render_size.width == 960, "Balanced mode width should match 1920 / 2.");
    Require(balanced.render_size.height == 600, "Balanced mode height should match 1200 / 2.");

    const auto performance = BuildResolutionPlan(display, QualityMode::Performance);
    Require(Near(performance.render_scale, 1.0f / 2.3f), "Performance mode should match XeSS 2.3x scale.");
    Require(performance.render_size.width == 835, "Performance mode width should round 1920 / 2.3.");
    Require(performance.render_size.height == 522, "Performance mode height should round 1200 / 2.3.");

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
    Require(ParseQualityMode("ultra_quality_plus") == QualityMode::UltraQualityPlus, "Ultra Quality Plus parsing failed.");
    Require(ParseQualityMode("ultra-quality-plus") == QualityMode::UltraQualityPlus, "Hyphenated Ultra Quality Plus parsing failed.");
    Require(ParseQualityMode("not_a_mode") == QualityMode::Quality, "Unknown mode should default to Quality.");
    Require(std::string_view(ToString(QualityMode::UltraQualityPlus)) == "UltraQualityPlus", "ToString coverage failed.");

    return 0;
}
