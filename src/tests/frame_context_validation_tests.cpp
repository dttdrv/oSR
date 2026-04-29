#include "core/frame_context.h"
#include "debug/validation.h"

#include <iostream>
#include <string_view>

namespace {

osr::core::ResourceDesc Resource(osr::core::ResourceKind kind, uint64_t id, uint32_t width, uint32_t height) {
    return {kind, nullptr, id, {width, height}, 0, osr::core::ToString(kind)};
}

osr::core::FrameContext ValidFrame() {
    osr::core::FrameContext frame;
    frame.frame_id = 1;
    frame.source_api = "test";
    frame.color_input = Resource(osr::core::ResourceKind::ColorInput, 1, 1280, 800);
    frame.color_output = Resource(osr::core::ResourceKind::ColorOutput, 2, 1920, 1200);
    frame.depth = Resource(osr::core::ResourceKind::Depth, 3, 1280, 800);
    frame.motion_vectors = Resource(osr::core::ResourceKind::MotionVectors, 4, 1280, 800);
    frame.render_size = {1280, 800};
    frame.upscale_size = {1920, 1200};
    frame.display_size = {1920, 1200};
    frame.frame_time_delta_ms = 16.667f;
    frame.camera.near_plane = 0.1f;
    frame.camera.far_plane = 1000.0f;
    frame.camera.vertical_fov_radians = 1.0471976f;
    frame.camera.view_space_to_meters = 1.0f;
    frame.reconstruction.sharpness = 0.5f;
    frame.reconstruction.sharpening_enabled = true;
    frame.reconstruction.debug_view_enabled = false;
    frame.jitter_offset = {0.25f, -0.25f};
    frame.motion_vector_scale = {1280.0f, 800.0f};
    frame.motion_vector_space = osr::core::MotionVectorSpace::Pixel;
    frame.exposure.pre_exposure = 1.0f;
    return frame;
}

bool HasCode(const osr::core::ValidationReport& report, std::string_view code) {
    for (const auto& message : report.messages) {
        if (message.code == code) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    const auto valid = osr::core::ValidateFrameContext(ValidFrame());
    if (valid.HasErrors()) {
        std::cerr << "Expected valid frame without errors, got " << osr::debug::SummarizeValidation(valid) << "\n";
        return 1;
    }

    auto invalid = ValidFrame();
    invalid.render_size = {};
    invalid.upscale_size = {};
    invalid.frame_time_delta_ms = 0.0f;
    invalid.camera.near_plane = 1.0f;
    invalid.camera.far_plane = 0.5f;
    invalid.camera.vertical_fov_radians = 0.0f;
    invalid.camera.view_space_to_meters = 0.0f;
    invalid.reconstruction.sharpness = 2.0f;
    invalid.motion_vector_scale = {};
    invalid.motion_vector_space = osr::core::MotionVectorSpace::Unknown;
    const auto invalid_report = osr::core::ValidateFrameContext(invalid);
    if (!invalid_report.HasErrors() || !invalid_report.HasWarnings()) {
        std::cerr << "Expected invalid frame to produce errors and warnings, got "
                  << osr::debug::SummarizeValidation(invalid_report) << "\n";
        return 1;
    }
    if (!HasCode(invalid_report, "invalid_upscale_size") ||
        !HasCode(invalid_report, "unknown_frame_time_delta") ||
        !HasCode(invalid_report, "invalid_camera_range") ||
        !HasCode(invalid_report, "invalid_camera_fov") ||
        !HasCode(invalid_report, "invalid_view_space_scale") ||
        !HasCode(invalid_report, "sharpness_out_of_range")) {
        std::cerr << "Expected vendor-parity validation warnings/errors, got "
                  << osr::debug::SummarizeValidation(invalid_report) << "\n";
        return 1;
    }

    return 0;
}
