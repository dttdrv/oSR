#include "reconstruction/sharpening.h"

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
    using osr::reconstruction::ClampSharpness;
    using osr::reconstruction::ConfidenceGatedSharpness;
    using osr::reconstruction::SharpeningSettings;

    Require(ClampSharpness(-1.0f) == 0.0f, "Sharpness should clamp low.");
    Require(ClampSharpness(2.0f) == 1.0f, "Sharpness should clamp high.");

    SharpeningSettings disabled;
    disabled.enabled = false;
    Require(ConfidenceGatedSharpness(0.8f, 1.0f, 0.0f, false, disabled) == 0.0f,
            "Disabled sharpening should return zero.");

    SharpeningSettings settings;
    settings.enabled = true;
    settings.low_trust_scale = 0.15f;
    settings.reactive_scale = 0.25f;

    Require(Near(ConfidenceGatedSharpness(0.8f, 1.0f, 0.0f, false, settings), 0.8f),
            "High trust non-reactive pixels should preserve base sharpening.");
    Require(Near(ConfidenceGatedSharpness(0.8f, 0.0f, 0.0f, false, settings), 0.12f),
            "Low trust pixels should strongly reduce sharpening.");
    Require(Near(ConfidenceGatedSharpness(0.8f, 1.0f, 1.0f, false, settings), 0.2f),
            "Reactive pixels should reduce sharpening.");
    Require(ConfidenceGatedSharpness(0.8f, 1.0f, 0.0f, true, settings) == 0.0f,
            "Disoccluded pixels should suppress sharpening.");

    const float low_trust = ConfidenceGatedSharpness(0.8f, 0.2f, 0.0f, false, settings);
    const float high_trust = ConfidenceGatedSharpness(0.8f, 0.8f, 0.0f, false, settings);
    Require(low_trust < high_trust, "Sharpening should increase monotonically with trust.");

    return 0;
}

