#include "reconstruction/reactive_mask_synthesis.h"
#include "reconstruction/trust_field.h"

#include <cmath>
#include <iostream>

namespace {

bool Near(float actual, float expected, float epsilon = 0.0001f) {
    return std::fabs(actual - expected) <= epsilon;
}

} // namespace

int main() {
    osr::reconstruction::TrustFieldSettings settings;

    if (!Near(osr::reconstruction::DepthTrust(0.0f, settings), 1.0f)) {
        std::cerr << "Depth trust should be full when depth delta is zero.\n";
        return 1;
    }

    if (!Near(osr::reconstruction::ColorTrust(0.0f, settings), 1.0f)) {
        std::cerr << "Color trust should be full when color delta is zero.\n";
        return 1;
    }

    if (osr::reconstruction::ColorTrust(settings.color_consistency_threshold * 2.0f, settings) != 0.0f) {
        std::cerr << "Color trust should clamp to zero beyond threshold.\n";
        return 1;
    }

    if (osr::reconstruction::MotionTrust(999.0f, settings) != 0.0f) {
        std::cerr << "Motion trust should clamp to zero for very large motion.\n";
        return 1;
    }

    osr::reconstruction::TrustFactors factors;
    factors.previous_trust = 1.0f;
    factors.depth_relative_delta = 0.0f;
    factors.motion_length_pixels = 0.0f;
    factors.color_delta_luma = 0.0f;
    factors.reactive_value = 0.0f;
    const auto stable = osr::reconstruction::ComputeTrustField(factors, settings);
    if (stable.history_trust <= 0.8f || stable.accumulation_weight <= 0.6f) {
        std::cerr << "Stable history should retain high trust.\n";
        return 1;
    }

    factors.reset_history = true;
    const auto reset = osr::reconstruction::ComputeTrustField(factors, settings);
    if (reset.history_trust != 0.0f || reset.accumulation_weight != 0.0f) {
        std::cerr << "Reset history should zero trust and accumulation.\n";
        return 1;
    }

    factors.reset_history = false;
    factors.color_delta_luma = 1.0f;
    const auto color_mismatch = osr::reconstruction::ComputeTrustField(factors, settings);
    if (color_mismatch.history_trust != 0.0f || color_mismatch.accumulation_weight != 0.0f) {
        std::cerr << "Large color mismatch should fully reject history.\n";
        return 1;
    }

    osr::reconstruction::ReactiveSynthesisInputs reactive_inputs;
    reactive_inputs.color_delta_luma = 0.3f;
    const auto reactive = osr::reconstruction::SynthesizeReactiveMask(reactive_inputs, {});
    if (reactive <= 0.0f || reactive > 1.0f) {
        std::cerr << "Synthesized reactive mask should be in (0, 1].\n";
        return 1;
    }

    return 0;
}
