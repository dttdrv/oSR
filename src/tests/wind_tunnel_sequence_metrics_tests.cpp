#include "demo/wind_tunnel/sequence_metrics.h"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

} // namespace

int main() {
    osr::demo::wind_tunnel::SequenceMetricsSettings settings;
    settings.display_size = {320, 200};
    settings.render_scale = 2.0f / 3.0f;
    settings.frame_count = 16;
    const auto result = osr::demo::wind_tunnel::RunSequenceMetrics(settings);
    if (result.frames != settings.frame_count) {
        return Fail("sequence metrics did not process requested frame count");
    }
    if (result.spatial_frame_delta_mean <= 0.0) {
        return Fail("spatial sequence delta should be non-zero");
    }
    if (result.temporal_frame_delta_mean <= 0.0) {
        return Fail("temporal sequence delta should be non-zero");
    }
    if (result.temporal_delta_ratio >= 0.80) {
        return Fail("temporal resolve should reduce mean frame delta in the synthetic sequence");
    }
    if (result.stability_improvement_pct <= 20.0) {
        return Fail("temporal resolve should report meaningful stability improvement");
    }
    if (result.reactive_trail_score > 0.12) {
        return Fail("temporal resolve should keep reactive trail score low");
    }
    if (result.edge_preservation < 0.72) {
        return Fail("temporal resolve should preserve enough edge energy");
    }
    if (result.thin_feature_contrast < 0.82 || result.thin_feature_contrast > 1.45) {
        return Fail("temporal resolve should preserve thin-feature contrast without excessive ringing");
    }
    if (result.text_readability_contrast < 0.72 || result.text_readability_contrast > 1.35) {
        return Fail("temporal resolve should preserve readable text contrast without excessive ringing");
    }
    if (result.specular_history_leak > 0.12) {
        return Fail("temporal resolve should suppress history on specular stress regions");
    }
    if (result.transparent_history_leak > 0.32) {
        return Fail("temporal resolve should limit history on transparent stress regions");
    }
    if (result.reprojected_history_pct <= 0.0) {
        return Fail("temporal resolve should reproject some moving history");
    }
    if (result.color_residual_mean <= 0.0) {
        return Fail("sequence metrics should report color residuals");
    }
    if (result.depth_residual_mean <= 0.0) {
        return Fail("sequence metrics should report depth residuals");
    }
    if (result.sharpening_amount_mean <= 0.0 || result.sharpening_amount_mean > 1.0) {
        return Fail("sequence metrics should report bounded sharpening amount");
    }
    if (result.ghost_score > 0.45) {
        return Fail("temporal resolve should keep motion ghost score below gate");
    }
    if (result.temporal_history_weight_mean <= 0.0 || result.temporal_history_weight_mean > 1.0) {
        return Fail("temporal history weight mean out of range");
    }
    return 0;
}
