#include "debug/frame_context_readiness.h"

#include <iostream>
#include <string_view>

namespace {

osr::core::ResourceDesc Resource(osr::core::ResourceKind kind, uint64_t id, uint32_t width, uint32_t height) {
    return {kind, nullptr, id, {width, height}, 87, osr::core::ToString(kind)};
}

osr::core::FrameContext CompleteHarnessFrame() {
    osr::core::FrameContext frame;
    frame.frame_id = 42;
    frame.source_api = "dx12_wind_tunnel";
    frame.color_input = Resource(osr::core::ResourceKind::ColorInput, 1, 1280, 720);
    frame.color_output = Resource(osr::core::ResourceKind::ColorOutput, 2, 1920, 1080);
    frame.depth = Resource(osr::core::ResourceKind::Depth, 3, 1280, 720);
    frame.motion_vectors = Resource(osr::core::ResourceKind::MotionVectors, 4, 1920, 1080);
    frame.reactive_mask = Resource(osr::core::ResourceKind::ReactiveMask, 5, 1280, 720);
    frame.exposure.exposure_texture = Resource(osr::core::ResourceKind::Exposure, 6, 1, 1);
    frame.render_size = {1280, 720};
    frame.upscale_size = {1920, 1080};
    frame.display_size = {1920, 1080};
    frame.frame_time_delta_ms = 16.667f;
    frame.camera.near_plane = 0.1f;
    frame.camera.far_plane = 1000.0f;
    frame.camera.vertical_fov_radians = 1.0471976f;
    frame.camera.view_space_to_meters = 1.0f;
    frame.reconstruction.sharpness = 0.5f;
    frame.jitter_offset = {0.25f, -0.25f};
    frame.motion_vector_scale = {1920.0f, -1080.0f};
    frame.motion_vector_space = osr::core::MotionVectorSpace::Pixel;
    frame.exposure.exposure_scale = 1.0f;
    frame.exposure.pre_exposure = 1.0f;
    frame.flags.depth_inverted = true;
    frame.flags.display_resolution_motion_vectors = true;
    return frame;
}

bool HasCode(const osr::debug::ReadinessReport& report, std::string_view code) {
    for (const auto& item : report.items) {
        if (item.code == code) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    const auto complete = osr::debug::EvaluateSrHarnessReadiness(CompleteHarnessFrame());
    if (!complete.ready) {
        std::cerr << "complete harness frame should be ready: "
                  << osr::debug::SummarizeReadiness(complete) << "\n";
        return 1;
    }

    auto incomplete = CompleteHarnessFrame();
    incomplete.reactive_mask.reset();
    incomplete.exposure.exposure_texture.reset();
    incomplete.motion_vector_scale = {};
    incomplete.motion_vector_space = osr::core::MotionVectorSpace::Unknown;
    incomplete.depth.extent = {1920, 1080};

    const auto report = osr::debug::EvaluateSrHarnessReadiness(incomplete);
    if (report.ready) {
        std::cerr << "incomplete harness frame should not be ready\n";
        return 1;
    }
    if (!HasCode(report, "missing_reactive_mask") ||
        !HasCode(report, "missing_exposure_texture") ||
        !HasCode(report, "unknown_motion_vector_space") ||
        !HasCode(report, "zero_motion_vector_scale") ||
        !HasCode(report, "depth_extent_mismatch")) {
        std::cerr << "readiness report missing expected diagnostics: "
                  << osr::debug::SummarizeReadiness(report) << "\n";
        return 1;
    }

    return 0;
}
