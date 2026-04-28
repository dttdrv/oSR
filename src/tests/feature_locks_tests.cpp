#include "reconstruction/feature_locks.h"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

} // namespace

int main() {
    osr::reconstruction::FeatureLockSettings settings;
    osr::reconstruction::FeatureLockInputs inputs;
    inputs.edge_strength = 0.35f;
    inputs.history_trust = 0.9f;
    inputs.luma_delta = 0.01f;
    inputs.luma_variance = 0.0001f;
    inputs.motion_pixels = 0.25f;

    const auto acquired = osr::reconstruction::UpdateFeatureLock(inputs, settings);
    if (acquired.strength <= 0.0f || acquired.age != 1u) {
        return Fail("stable high-trust edge should acquire a feature lock");
    }

    inputs.previous_strength = acquired.strength;
    inputs.previous_age = acquired.age;
    const auto reinforced = osr::reconstruction::UpdateFeatureLock(inputs, settings);
    if (reinforced.strength <= acquired.strength || reinforced.age != 2u) {
        return Fail("stable feature lock should reinforce and age");
    }

    inputs.previous_strength = reinforced.strength;
    inputs.previous_age = reinforced.age;
    inputs.luma_delta = 0.5f;
    const auto decayed = osr::reconstruction::UpdateFeatureLock(inputs, settings);
    if (decayed.strength >= reinforced.strength || decayed.age != reinforced.age) {
        return Fail("luma-unstable feature lock should decay without aging");
    }

    inputs.luma_delta = 0.01f;
    inputs.reactive_value = 1.0f;
    const auto reactive = osr::reconstruction::UpdateFeatureLock(inputs, settings);
    if (reactive.strength >= decayed.strength) {
        return Fail("reactive pixels should hard-unlock feature locks");
    }

    inputs.reactive_value = 0.0f;
    inputs.reset_history = true;
    const auto reset = osr::reconstruction::UpdateFeatureLock(inputs, settings);
    if (reset.strength != 0.0f || reset.age != 0u) {
        return Fail("history reset should clear feature locks");
    }

    if (osr::reconstruction::FeatureLockSharpeningScale({1.0f, 8u}, 0.6f) <=
        osr::reconstruction::FeatureLockSharpeningScale({0.0f, 0u}, 0.6f)) {
        return Fail("locked features should preserve more sharpening scale than unlocked pixels");
    }

    return 0;
}
