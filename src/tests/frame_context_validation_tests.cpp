#include "core/frame_context.h"
#include "debug/validation.h"

#include <iostream>

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
    frame.display_size = {1920, 1200};
    frame.jitter_offset = {0.25f, -0.25f};
    frame.motion_vector_scale = {1280.0f, 800.0f};
    frame.motion_vector_space = osr::core::MotionVectorSpace::Pixel;
    frame.exposure.pre_exposure = 1.0f;
    return frame;
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
    invalid.motion_vector_scale = {};
    invalid.motion_vector_space = osr::core::MotionVectorSpace::Unknown;
    const auto invalid_report = osr::core::ValidateFrameContext(invalid);
    if (!invalid_report.HasErrors() || !invalid_report.HasWarnings()) {
        std::cerr << "Expected invalid frame to produce errors and warnings, got "
                  << osr::debug::SummarizeValidation(invalid_report) << "\n";
        return 1;
    }

    return 0;
}

