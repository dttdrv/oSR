#include "reconstruction/temporal_oracle.h"

#include <iomanip>
#include <iostream>
#include <string>

namespace {

void PrintScenario(const std::string& name, const osr::reconstruction::TemporalOracleSample& sample) {
    const osr::reconstruction::TrustFieldSettings settings;
    const auto result = osr::reconstruction::ResolveTemporalSample(sample, settings);
    std::cout << name
              << ",trust=" << std::fixed << std::setprecision(6) << result.trust.history_trust
              << ",evidence=" << result.trust.evidence_trust
              << ",history_weight=" << result.trust.accumulation_weight
              << ",current_weight=" << result.current_weight
              << ",resolved_luma=" << result.resolved_luma
              << "\n";
}

} // namespace

int main() {
    using osr::reconstruction::TemporalOracleSample;

    PrintScenario("stable_static", TemporalOracleSample{
        .current_luma = 0.50f,
        .history_luma = 0.505f,
        .current_depth = 0.4f,
        .reprojected_depth = 0.4f,
        .previous_trust = 1.0f
    });

    PrintScenario("reactive_particle", TemporalOracleSample{
        .current_luma = 1.0f,
        .history_luma = 0.2f,
        .current_depth = 0.4f,
        .reprojected_depth = 0.4f,
        .reactive_value = 1.0f,
        .previous_trust = 1.0f
    });

    PrintScenario("disoccluded_edge", TemporalOracleSample{
        .current_luma = 0.15f,
        .history_luma = 0.8f,
        .current_depth = 0.25f,
        .reprojected_depth = 0.7f,
        .disoccluded = true,
        .previous_trust = 1.0f
    });

    PrintScenario("scene_cut_reset", TemporalOracleSample{
        .current_luma = 0.1f,
        .history_luma = 0.9f,
        .reset_history = true,
        .previous_trust = 1.0f
    });

    return 0;
}
