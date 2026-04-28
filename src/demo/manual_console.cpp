#include "core/quality_mode.h"
#include "reconstruction/temporal_oracle.h"
#include "reconstruction/tile_classifier.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {

void WaitForEnter() {
    std::cout << "\nPress Enter to continue...";
    std::string line;
    std::getline(std::cin, line);
}

void PrintHeader(const char* title) {
    std::cout << "\n== " << title << " ==\n";
}

void PrintResolutionPlan(const osr::core::ResolutionPlan& plan) {
    std::cout << std::left << std::setw(18) << osr::core::ToString(plan.mode)
              << " scale " << std::fixed << std::setprecision(3) << plan.render_scale
              << "  render " << plan.render_size.width << "x" << plan.render_size.height
              << "  display " << plan.display_size.width << "x" << plan.display_size.height
              << "\n";
}

void ShowQualityModes(osr::core::Dimensions display) {
    PrintHeader("Resolution Modes");
    const osr::core::QualityMode modes[] = {
        osr::core::QualityMode::Native,
        osr::core::QualityMode::UltraQuality,
        osr::core::QualityMode::Quality,
        osr::core::QualityMode::Balanced,
        osr::core::QualityMode::Performance,
        osr::core::QualityMode::UltraPerformance
    };

    for (const auto mode : modes) {
        PrintResolutionPlan(osr::core::BuildResolutionPlan(display, mode));
    }
}

void CustomScale(osr::core::Dimensions display) {
    PrintHeader("Custom Render Scale");
    std::cout << "Enter render scale percent [33..100], e.g. 72: ";
    float percent = 0.0f;
    if (!(std::cin >> percent)) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid number.\n";
        return;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    const auto plan = osr::core::BuildCustomResolutionPlan(display, percent / 100.0f);
    PrintResolutionPlan(plan);
}

osr::core::Dimensions ChangeDisplayResolution(osr::core::Dimensions current) {
    PrintHeader("Display Resolution");
    std::cout << "Current display: " << current.width << "x" << current.height << "\n";
    std::cout << "Enter width height, e.g. 1920 1200: ";
    uint32_t width = 0;
    uint32_t height = 0;
    if (!(std::cin >> width >> height) || width == 0 || height == 0) {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "Invalid resolution; keeping current value.\n";
        return current;
    }
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return {width, height};
}

void PrintTrustScenario(const std::string& name, const osr::reconstruction::TemporalOracleSample& sample) {
    const osr::reconstruction::TrustFieldSettings settings;
    const auto result = osr::reconstruction::ResolveTemporalSample(sample, settings);
    std::cout << std::left << std::setw(20) << name
              << " trust " << std::fixed << std::setprecision(6) << result.trust.history_trust
              << "  evidence " << result.trust.evidence_trust
              << "  history_w " << result.trust.accumulation_weight
              << "  current_w " << result.current_weight
              << "  resolved_luma " << result.resolved_luma
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

void ShowTrustScenarios() {
    PrintHeader("Trust Oracle");
    using osr::reconstruction::TemporalOracleSample;

    PrintTrustScenario("stable_static", TemporalOracleSample{
        .current_luma = 0.50f,
        .history_luma = 0.505f,
        .current_depth = 0.4f,
        .reprojected_depth = 0.4f,
        .previous_trust = 1.0f
    });

    PrintTrustScenario("reactive_particle", TemporalOracleSample{
        .current_luma = 1.0f,
        .history_luma = 0.2f,
        .current_depth = 0.4f,
        .reprojected_depth = 0.4f,
        .reactive_value = 1.0f,
        .previous_trust = 1.0f
    });

    PrintTrustScenario("disoccluded_edge", TemporalOracleSample{
        .current_luma = 0.15f,
        .history_luma = 0.8f,
        .current_depth = 0.25f,
        .reprojected_depth = 0.7f,
        .disoccluded = true,
        .previous_trust = 1.0f
    });

    PrintTrustScenario("scene_cut_reset", TemporalOracleSample{
        .current_luma = 0.1f,
        .history_luma = 0.9f,
        .reset_history = true,
        .previous_trust = 1.0f
    });
}

void PrintTileScenario(const std::string& name, std::vector<osr::reconstruction::TemporalOracleSample> samples) {
    const osr::reconstruction::TrustFieldSettings trust_settings;
    const osr::reconstruction::TileClassifierSettings tile_settings;
    const auto stats = osr::reconstruction::ClassifyTile(samples, trust_settings, tile_settings);
    std::cout << std::left << std::setw(16) << name
              << " class " << std::setw(18) << osr::reconstruction::ToString(stats.classification)
              << " avg_trust " << std::fixed << std::setprecision(6) << stats.average_trust
              << " max_motion " << stats.max_motion_pixels
              << " max_reactive " << stats.max_reactive
              << "\n";
}

void ShowTileScenarios() {
    PrintHeader("Tile Risk");
    PrintTileScenario("stable", StableTile());

    auto motion = StableTile();
    motion[7].motion_x_pixels = 64.0f;
    PrintTileScenario("motion", motion);

    auto reactive = StableTile();
    reactive[5].reactive_value = 0.75f;
    reactive[5].current_luma = 0.9f;
    PrintTileScenario("reactive", reactive);

    auto disoccluded = StableTile();
    for (int i = 0; i < 8; ++i) {
        disoccluded[i].disoccluded = true;
        disoccluded[i].current_depth = 0.2f;
        disoccluded[i].reprojected_depth = 0.8f;
    }
    PrintTileScenario("disoccluded", disoccluded);
}

} // namespace

int main() {
    osr::core::Dimensions display {1920, 1200};

    for (;;) {
        std::cout << "\n";
        std::cout << "oSR Manual Console\n";
        std::cout << "Display: " << display.width << "x" << display.height << "\n";
        std::cout << "1. Show DLSS-style quality modes\n";
        std::cout << "2. Enter custom render-scale slider value\n";
        std::cout << "3. Change display resolution\n";
        std::cout << "4. Run trust oracle scenarios\n";
        std::cout << "5. Run tile risk scenarios\n";
        std::cout << "q. Quit\n";
        std::cout << "> ";

        std::string choice;
        if (!std::getline(std::cin, choice)) {
            return 0;
        }

        if (choice == "1") {
            ShowQualityModes(display);
            WaitForEnter();
        } else if (choice == "2") {
            CustomScale(display);
            WaitForEnter();
        } else if (choice == "3") {
            display = ChangeDisplayResolution(display);
            WaitForEnter();
        } else if (choice == "4") {
            ShowTrustScenarios();
            WaitForEnter();
        } else if (choice == "5") {
            ShowTileScenarios();
            WaitForEnter();
        } else if (choice == "q" || choice == "Q") {
            return 0;
        } else {
            std::cout << "Unknown option.\n";
        }
    }
}
