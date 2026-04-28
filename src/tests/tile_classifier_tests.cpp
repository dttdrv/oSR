#include "reconstruction/tile_classifier.h"

#include <iostream>
#include <string>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(1);
    }
}

std::vector<osr::reconstruction::TemporalOracleSample> MakeStableTile() {
    std::vector<osr::reconstruction::TemporalOracleSample> samples;
    samples.reserve(64);
    for (int i = 0; i < 64; ++i) {
        osr::reconstruction::TemporalOracleSample sample;
        sample.current_luma = 0.50f + ((i % 2) ? 0.002f : -0.002f);
        sample.history_luma = 0.50f;
        sample.current_depth = 0.4f;
        sample.reprojected_depth = 0.4f;
        sample.previous_trust = 1.0f;
        samples.push_back(sample);
    }
    return samples;
}

} // namespace

int main() {
    const osr::reconstruction::TrustFieldSettings trust_settings;
    const osr::reconstruction::TileClassifierSettings tile_settings;

    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::Invalid)) == "Invalid",
            "ToString must cover Invalid.");
    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::Stable)) == "Stable",
            "ToString must cover Stable.");
    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::ShimmerRisk)) == "ShimmerRisk",
            "ToString must cover ShimmerRisk.");
    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::MotionRisk)) == "MotionRisk",
            "ToString must cover MotionRisk.");
    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::ReactiveRisk)) == "ReactiveRisk",
            "ToString must cover ReactiveRisk.");
    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::DisocclusionRisk)) == "DisocclusionRisk",
            "ToString must cover DisocclusionRisk.");
    Require(std::string(osr::reconstruction::ToString(osr::reconstruction::TileClass::Reset)) == "Reset",
            "ToString must cover Reset.");

    const auto empty_stats = osr::reconstruction::ClassifyTile({}, trust_settings, tile_settings);
    Require(empty_stats.classification == osr::reconstruction::TileClass::Invalid,
            "Empty tile should classify as Invalid.");

    auto stable = MakeStableTile();
    const auto stable_stats = osr::reconstruction::ClassifyTile(stable, trust_settings, tile_settings);
    Require(stable_stats.classification == osr::reconstruction::TileClass::Stable,
            "Stable tile should classify as Stable.");
    Require(stable_stats.average_trust > 0.80f, "Stable tile should have high average trust.");

    auto motion = MakeStableTile();
    motion[7].motion_x_pixels = tile_settings.motion_risk_threshold_pixels;
    const auto motion_stats = osr::reconstruction::ClassifyTile(motion, trust_settings, tile_settings);
    Require(motion_stats.classification == osr::reconstruction::TileClass::MotionRisk,
            "Motion exactly at threshold should classify tile as MotionRisk.");

    auto reactive = MakeStableTile();
    reactive[5].reactive_value = tile_settings.reactive_risk_threshold;
    reactive[5].current_luma = 0.9f;
    const auto reactive_stats = osr::reconstruction::ClassifyTile(reactive, trust_settings, tile_settings);
    Require(reactive_stats.classification == osr::reconstruction::TileClass::ReactiveRisk,
            "Reactive input should classify tile as ReactiveRisk.");

    auto disoccluded = MakeStableTile();
    const uint32_t disocclusion_count =
        osr::reconstruction::MinimumSamplesForRatio(tile_settings.disocclusion_ratio_threshold,
                                                    static_cast<uint32_t>(disoccluded.size()));
    for (uint32_t i = 0; i < disocclusion_count; ++i) {
        disoccluded[i].disoccluded = true;
        disoccluded[i].current_depth = 0.2f;
        disoccluded[i].reprojected_depth = 0.8f;
    }
    const auto disoccluded_stats = osr::reconstruction::ClassifyTile(disoccluded, trust_settings, tile_settings);
    Require(disoccluded_stats.classification == osr::reconstruction::TileClass::DisocclusionRisk,
            "Disocclusion ratio should classify tile as DisocclusionRisk.");

    auto reset = MakeStableTile();
    reset[0].reset_history = true;
    reset[1].disoccluded = true;
    reset[2].reactive_value = 1.0f;
    reset[3].motion_x_pixels = 128.0f;
    const auto reset_stats = osr::reconstruction::ClassifyTile(reset, trust_settings, tile_settings);
    Require(reset_stats.classification == osr::reconstruction::TileClass::Reset,
            "Reset should take priority over all other tile risks.");

    auto disocclusion_priority = MakeStableTile();
    for (uint32_t i = 0; i < disocclusion_count; ++i) {
        disocclusion_priority[i].disoccluded = true;
        disocclusion_priority[i].current_depth = 0.2f;
        disocclusion_priority[i].reprojected_depth = 0.8f;
    }
    disocclusion_priority[10].reactive_value = 1.0f;
    disocclusion_priority[11].motion_x_pixels = 128.0f;
    const auto disocclusion_priority_stats =
        osr::reconstruction::ClassifyTile(disocclusion_priority, trust_settings, tile_settings);
    Require(disocclusion_priority_stats.classification == osr::reconstruction::TileClass::DisocclusionRisk,
            "Disocclusion should take priority over reactive and motion risks.");

    auto reactive_priority = MakeStableTile();
    reactive_priority[0].reactive_value = 1.0f;
    reactive_priority[1].motion_x_pixels = 128.0f;
    const auto reactive_priority_stats =
        osr::reconstruction::ClassifyTile(reactive_priority, trust_settings, tile_settings);
    Require(reactive_priority_stats.classification == osr::reconstruction::TileClass::ReactiveRisk,
            "Reactive should take priority over motion risk.");

    auto motion_priority = MakeStableTile();
    motion_priority[0].motion_x_pixels = 128.0f;
    for (int i = 0; i < 64; ++i) {
        motion_priority[i].current_luma = (i % 2 == 0) ? 0.35f : 0.65f;
        motion_priority[i].history_luma = 0.50f;
    }
    const auto motion_priority_stats =
        osr::reconstruction::ClassifyTile(motion_priority, trust_settings, tile_settings);
    Require(motion_priority_stats.classification == osr::reconstruction::TileClass::MotionRisk,
            "Motion should take priority over shimmer risk.");

    auto shimmer = MakeStableTile();
    for (int i = 0; i < 64; ++i) {
        shimmer[i].current_luma = (i % 2 == 0) ? 0.35f : 0.65f;
        shimmer[i].history_luma = 0.50f;
    }
    const auto shimmer_stats = osr::reconstruction::ClassifyTile(shimmer, trust_settings, tile_settings);
    Require(shimmer_stats.classification == osr::reconstruction::TileClass::ShimmerRisk,
            "High luma variance should classify tile as ShimmerRisk.");

    auto partial = MakeStableTile();
    partial.resize(7);
    const auto partial_stats = osr::reconstruction::ClassifyTile(partial, trust_settings, tile_settings);
    Require(partial_stats.classification == osr::reconstruction::TileClass::Stable,
            "Partial stable tile should classify deterministically.");

    auto low_trust = MakeStableTile();
    for (auto& sample : low_trust) {
        sample.current_luma = 0.5f;
        sample.history_luma = 0.5f + trust_settings.color_consistency_threshold * 0.45f;
    }
    const auto low_trust_stats = osr::reconstruction::ClassifyTile(low_trust, trust_settings, tile_settings);
    Require(low_trust_stats.classification == osr::reconstruction::TileClass::ShimmerRisk,
            "Low average trust should classify as ShimmerRisk even without high luma variance.");

    return 0;
}
