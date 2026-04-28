#include "reconstruction/temporal_oracle.h"
#include "reconstruction/tile_classifier.h"

#include <iomanip>
#include <iostream>
#include <string>
#include <vector>

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

void PrintTileScenario(const std::string& name, std::vector<osr::reconstruction::TemporalOracleSample> samples) {
    const osr::reconstruction::TrustFieldSettings trust_settings;
    const osr::reconstruction::TileClassifierSettings tile_settings;
    const auto stats = osr::reconstruction::ClassifyTile(samples, trust_settings, tile_settings);
    std::cout << name
              << ",tile_class=" << osr::reconstruction::ToString(stats.classification)
              << ",avg_trust=" << std::fixed << std::setprecision(6) << stats.average_trust
              << ",min_trust=" << stats.min_trust
              << ",max_motion=" << stats.max_motion_pixels
              << ",max_reactive=" << stats.max_reactive
              << ",luma_variance=" << stats.luma_variance
              << ",disocclusion_ratio=" << stats.disocclusion_ratio
              << "\n";
}

std::vector<osr::reconstruction::TemporalOracleSample> StableTile() {
    std::vector<osr::reconstruction::TemporalOracleSample> samples;
    samples.reserve(64);
    for (int i = 0; i < 64; ++i) {
        osr::reconstruction::TemporalOracleSample sample;
        sample.current_luma = 0.5f + ((i % 2 == 0) ? 0.002f : -0.002f);
        sample.history_luma = 0.5f;
        sample.current_depth = 0.4f;
        sample.reprojected_depth = 0.4f;
        sample.previous_trust = 1.0f;
        samples.push_back(sample);
    }
    return samples;
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

    auto stable_tile = StableTile();
    PrintTileScenario("tile_stable", stable_tile);

    auto motion_tile = StableTile();
    motion_tile[7].motion_x_pixels = 64.0f;
    PrintTileScenario("tile_motion", motion_tile);

    auto reactive_tile = StableTile();
    reactive_tile[5].reactive_value = 0.75f;
    reactive_tile[5].current_luma = 0.9f;
    PrintTileScenario("tile_reactive", reactive_tile);

    return 0;
}
