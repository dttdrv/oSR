#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <filesystem>
#include <string>

namespace osr::debug {

struct CaptureValueStats {
    uint64_t samples = 0;
    double min = 0.0;
    double max = 0.0;
    double mean = 0.0;
    double threshold = 0.0;
    double over_threshold_pct = 0.0;
};

struct CaptureRegionStats {
    uint64_t samples = 0;
    double mean_history = 0.0;
    double history_trusted_pct = 0.0;
    double mean_color_residual = 0.0;
};

struct CaptureFrameAnalysis {
    bool ok = false;
    std::string error;
    core::Dimensions display_size {};
    core::Dimensions render_size {};
    CaptureValueStats history_weight;
    CaptureValueStats color_residual;
    CaptureValueStats depth_residual;
    CaptureValueStats motion_magnitude;
    uint64_t frame_id = 0;
    double motion_region_history_trusted_pct = 0.0;
    double static_region_history_trusted_pct = 0.0;
    double motion_region_mean_history = 0.0;
    double static_region_mean_history = 0.0;
    CaptureRegionStats text_region;
    CaptureRegionStats specular_region;
    CaptureRegionStats transparent_region;
    CaptureRegionStats reactive_region;
};

[[nodiscard]] CaptureFrameAnalysis AnalyzeCaptureFrame(const std::filesystem::path& frame_dir);
[[nodiscard]] std::string SummarizeCaptureAnalysis(const CaptureFrameAnalysis& analysis);

} // namespace osr::debug
