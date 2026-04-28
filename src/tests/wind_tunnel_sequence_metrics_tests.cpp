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
    if (result.ghost_score > 0.65) {
        return Fail("temporal resolve should keep motion ghost score below gate");
    }
    if (result.temporal_history_weight_mean <= 0.0 || result.temporal_history_weight_mean > 1.0) {
        return Fail("temporal history weight mean out of range");
    }
    return 0;
}
