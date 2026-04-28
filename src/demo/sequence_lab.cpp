#include "demo/wind_tunnel/sequence_metrics.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

bool ParseDimensions(const std::string& value, osr::core::Dimensions& dimensions) {
    const auto x = value.find('x');
    if (x == std::string::npos || x == 0 || x + 1 >= value.size()) {
        return false;
    }
    const int width = std::atoi(value.substr(0, x).c_str());
    const int height = std::atoi(value.substr(x + 1).c_str());
    if (width <= 0 || height <= 0) {
        return false;
    }
    dimensions = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    return true;
}

} // namespace

int main(int argc, char** argv) {
    osr::demo::wind_tunnel::SequenceMetricsSettings settings;
    bool metric_gate = false;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--frames" && i + 1 < argc) {
            settings.frame_count = static_cast<uint32_t>(std::max(0, std::atoi(argv[++i])));
        } else if (arg == "--display-size" && i + 1 < argc) {
            if (!ParseDimensions(argv[++i], settings.display_size)) {
                std::cerr << "Invalid --display-size. Use WIDTHxHEIGHT.\n";
                return 2;
            }
        } else if (arg == "--render-scale" && i + 1 < argc) {
            settings.render_scale = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--metric-gate") {
            metric_gate = true;
        }
    }

    const auto result = osr::demo::wind_tunnel::RunSequenceMetrics(settings);
    std::filesystem::create_directories("build/manual");
    std::ofstream csv("build/manual/osr_sequence_lab_metrics.csv", std::ios::trunc);
    csv << "frames,spatial_frame_delta_mean,temporal_frame_delta_mean,temporal_delta_ratio,"
           "stability_improvement_pct,ghost_score,reactive_trail_score,"
           "edge_preservation,"
           "reprojected_history_pct,reproject_out_of_bounds_pct,"
           "color_rejected_pct,color_residual_mean,"
           "temporal_history_weight_mean,temporal_reactive_suppressed_pct,temporal_motion_suppressed_pct\n";
    csv << result.frames << ","
        << result.spatial_frame_delta_mean << ","
        << result.temporal_frame_delta_mean << ","
        << result.temporal_delta_ratio << ","
        << result.stability_improvement_pct << ","
        << result.ghost_score << ","
        << result.reactive_trail_score << ","
        << result.edge_preservation << ","
        << result.reprojected_history_pct << ","
        << result.reproject_out_of_bounds_pct << ","
        << result.color_rejected_pct << ","
        << result.color_residual_mean << ","
        << result.temporal_history_weight_mean << ","
        << result.temporal_reactive_suppressed_pct << ","
        << result.temporal_motion_suppressed_pct << "\n";

    std::cout << "oSR sequence lab\n";
    std::cout << "Frames: " << result.frames << "\n";
    std::cout << "Spatial frame delta mean: " << result.spatial_frame_delta_mean << "\n";
    std::cout << "Temporal frame delta mean: " << result.temporal_frame_delta_mean << "\n";
    std::cout << "Temporal/spatial delta ratio: " << result.temporal_delta_ratio << "\n";
    std::cout << "Stability improvement: " << result.stability_improvement_pct << "%\n";
    std::cout << "Ghost score: " << result.ghost_score << "\n";
    std::cout << "Reactive trail score: " << result.reactive_trail_score << "\n";
    std::cout << "Edge preservation: " << result.edge_preservation << "\n";
    std::cout << "Reprojected history: " << result.reprojected_history_pct << "%\n";
    std::cout << "Reproject OOB: " << result.reproject_out_of_bounds_pct << "%\n";
    std::cout << "Color rejected: " << result.color_rejected_pct << "%\n";
    std::cout << "Color residual mean: " << result.color_residual_mean << "\n";
    std::cout << "Temporal history weight mean: " << result.temporal_history_weight_mean << "\n";
    std::cout << "Metrics: build/manual/osr_sequence_lab_metrics.csv\n";

    if (metric_gate && (result.temporal_delta_ratio > 0.80 ||
                        result.ghost_score > 0.45 ||
                        result.reactive_trail_score > 0.12 ||
                        result.edge_preservation < 0.72)) {
        std::cerr << "Metric gate failed: temporal stability, ghost, reactive-trail, or edge preservation outside threshold.\n";
        return 3;
    }
    return 0;
}
