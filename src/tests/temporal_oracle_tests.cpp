#include "reconstruction/temporal_oracle.h"

#include <cmath>
#include <iostream>
#include <random>
#include <vector>

namespace {

bool Finite(float value) {
    return std::isfinite(value);
}

bool InUnitRange(float value) {
    return value >= 0.0f && value <= 1.0f;
}

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(1);
    }
}

osr::reconstruction::TemporalOracleResult Resolve(osr::reconstruction::TemporalOracleSample sample) {
    const osr::reconstruction::TrustFieldSettings settings;
    return osr::reconstruction::ResolveTemporalSample(sample, settings);
}

} // namespace

int main() {
    using osr::reconstruction::TemporalOracleSample;
    using osr::reconstruction::Variance;

    TemporalOracleSample stable;
    stable.current_luma = 0.50f;
    stable.history_luma = 0.505f;
    stable.current_depth = 0.4f;
    stable.reprojected_depth = 0.4f;
    stable.previous_trust = 1.0f;
    const auto stable_result = Resolve(stable);
    Require(stable_result.trust.history_trust > 0.80f, "Stable sample should keep high history trust.");
    Require(stable_result.trust.accumulation_weight > 0.65f, "Stable sample should use substantial history.");
    Require(std::fabs(stable_result.trust.history_trust - 0.881666f) < 0.0005f,
            "Stable canonical trust changed; update the oracle expectation intentionally.");
    Require(std::fabs(stable_result.trust.accumulation_weight - 0.758233f) < 0.0005f,
            "Stable canonical history weight changed; update the oracle expectation intentionally.");

    TemporalOracleSample reset = stable;
    reset.current_luma = 0.10f;
    reset.history_luma = 0.90f;
    reset.reset_history = true;
    const auto reset_result = Resolve(reset);
    Require(reset_result.trust.history_trust == 0.0f, "Reset must clear history trust.");
    Require(reset_result.trust.accumulation_weight == 0.0f, "Reset must clear history weight.");
    Require(reset_result.resolved_luma == reset.current_luma, "Reset must resolve to current sample.");

    TemporalOracleSample disoccluded = stable;
    disoccluded.current_luma = 0.15f;
    disoccluded.history_luma = 0.75f;
    disoccluded.current_depth = 0.25f;
    disoccluded.reprojected_depth = 0.55f;
    disoccluded.disoccluded = true;
    const auto disoccluded_result = Resolve(disoccluded);
    Require(disoccluded_result.trust.accumulation_weight < stable_result.trust.accumulation_weight * 0.20f,
            "Disocclusion should sharply reduce history weight.");
    Require(std::fabs(disoccluded_result.resolved_luma - disoccluded.current_luma) < 0.08f,
            "Disocclusion should stay close to current luma.");

    TemporalOracleSample reactive = stable;
    reactive.current_luma = 1.0f;
    reactive.history_luma = 0.25f;
    reactive.reactive_value = 1.0f;
    const auto reactive_result = Resolve(reactive);
    Require(reactive_result.trust.accumulation_weight < stable_result.trust.accumulation_weight * 0.20f,
            "Reactive sample should strongly reduce history weight.");
    Require(reactive_result.trust.evidence_trust == 0.0f,
            "Large reactive color mismatch should drive evidence to zero.");

    float previous_weight = stable_result.trust.accumulation_weight;
    for (float motion = 8.0f; motion <= 96.0f; motion += 8.0f) {
        TemporalOracleSample moving = stable;
        moving.motion_x_pixels = motion;
        const auto moving_result = Resolve(moving);
        Require(moving_result.trust.accumulation_weight <= previous_weight + 0.0001f,
                "History weight must not increase as motion grows.");
        previous_weight = moving_result.trust.accumulation_weight;
    }

    std::vector<float> noisy_current;
    std::vector<float> resolved;
    float trust = 1.0f;
    for (int i = 0; i < 16; ++i) {
        const float sample_noise = (i % 2 == 0) ? 0.025f : -0.025f;
        TemporalOracleSample shimmer = stable;
        shimmer.current_luma = 0.5f + sample_noise;
        shimmer.history_luma = 0.5f;
        shimmer.previous_trust = trust;
        const auto shimmer_result = Resolve(shimmer);
        trust = shimmer_result.trust.history_trust;
        noisy_current.push_back(shimmer.current_luma);
        resolved.push_back(shimmer_result.resolved_luma);
    }
    Require(Variance(resolved) < Variance(noisy_current) * 0.35f,
            "Stable shimmer sequence should reduce luma variance.");

    std::mt19937 rng(0x05A1C0DEu);
    std::uniform_real_distribution<float> luma(-0.25f, 1.25f);
    std::uniform_real_distribution<float> depth(0.001f, 1.0f);
    std::uniform_real_distribution<float> motion(-256.0f, 256.0f);
    std::uniform_real_distribution<float> reactive_value(0.0f, 1.0f);
    for (int i = 0; i < 2000; ++i) {
        TemporalOracleSample sample;
        sample.current_luma = luma(rng);
        sample.history_luma = luma(rng);
        sample.current_depth = depth(rng);
        sample.reprojected_depth = depth(rng);
        sample.motion_x_pixels = motion(rng);
        sample.motion_y_pixels = motion(rng);
        sample.reactive_value = reactive_value(rng);
        sample.disoccluded = (i % 7) == 0;
        sample.reset_history = (i % 113) == 0;
        sample.previous_trust = reactive_value(rng);
        const auto result = Resolve(sample);
        Require(Finite(result.trust.history_trust), "Fuzz: trust must be finite.");
        Require(Finite(result.trust.accumulation_weight), "Fuzz: history weight must be finite.");
        Require(Finite(result.resolved_luma), "Fuzz: resolved luma must be finite.");
        Require(InUnitRange(result.trust.history_trust), "Fuzz: trust must stay in [0,1].");
        Require(InUnitRange(result.trust.accumulation_weight), "Fuzz: history weight must stay in [0,1].");
        Require(InUnitRange(result.current_weight), "Fuzz: current weight must stay in [0,1].");
        if (sample.reset_history) {
            Require(result.trust.accumulation_weight == 0.0f, "Fuzz: reset must dominate all other factors.");
        }
    }

    return 0;
}
