#include "interop/xess_bridge/xess_replacement_policy.h"

#include <iostream>
#include <string_view>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

osr::core::ResourceDesc Resource(osr::core::ResourceKind kind, uint64_t id, uint32_t width, uint32_t height) {
    return {kind, reinterpret_cast<void*>(static_cast<uintptr_t>(id)), id, {width, height}, 44, osr::core::ToString(kind)};
}

osr::core::FrameContext CompleteFrame() {
    osr::core::FrameContext frame;
    frame.frame_id = 5;
    frame.source_api = "xess_vk_proxy";
    frame.color_input = Resource(osr::core::ResourceKind::ColorInput, 1, 960, 544);
    frame.color_output = Resource(osr::core::ResourceKind::ColorOutput, 2, 1920, 1080);
    frame.depth = Resource(osr::core::ResourceKind::Depth, 3, 960, 544);
    frame.motion_vectors = Resource(osr::core::ResourceKind::MotionVectors, 4, 960, 544);
    frame.render_size = {960, 544};
    frame.upscale_size = {1920, 1080};
    frame.display_size = {1920, 1080};
    frame.jitter_offset = {0.25f, -0.25f};
    frame.motion_vector_scale = {960.0f, 544.0f};
    frame.motion_vector_space = osr::core::MotionVectorSpace::Pixel;
    frame.frame_time_delta_ms = 16.667f;
    frame.exposure.exposure_scale = 1.0f;
    frame.exposure.pre_exposure = 1.0f;
    return frame;
}

bool HasReason(const osr::interop::xess_bridge::XessReplacementDecision& decision, std::string_view needle) {
    return decision.reason.find(needle) != std::string::npos;
}

} // namespace

int main() {
    using namespace osr::interop::xess_bridge;

    if (ParseXessProxyMode(nullptr) != XessProxyMode::Passthrough ||
        ParseXessProxyMode("") != XessProxyMode::Passthrough ||
        ParseXessProxyMode("passthrough") != XessProxyMode::Passthrough ||
        ParseXessProxyMode("observe") != XessProxyMode::Observe ||
        ParseXessProxyMode("osr") != XessProxyMode::OsrExperimental ||
        ParseXessProxyMode("replacement") != XessProxyMode::OsrExperimental) {
        return Fail("proxy mode parsing failed");
    }

    auto frame = CompleteFrame();
    auto decision = EvaluateXessReplacementDecision(XessProxyMode::Passthrough, frame, true, true, true);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "passthrough")) {
        return Fail("passthrough mode should always forward");
    }

    decision = EvaluateXessReplacementDecision(XessProxyMode::Observe, frame, true, true, true);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "observe")) {
        return Fail("observe mode should decode but still forward");
    }

    decision = EvaluateXessReplacementDecision(XessProxyMode::OsrExperimental, frame, false, true, true);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "missing_execute_params")) {
        return Fail("osr mode should not replace without execute params");
    }

    decision = EvaluateXessReplacementDecision(XessProxyMode::OsrExperimental, frame, true, false, true);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "missing_command_buffer")) {
        return Fail("osr mode should not replace without a Vulkan command buffer");
    }

    auto nms_like = frame;
    nms_like.motion_vector_scale = {};
    decision = EvaluateXessReplacementDecision(XessProxyMode::OsrExperimental, nms_like, true, true, true);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "zero_motion_vector_scale")) {
        return Fail("osr mode must refuse to infer NMS motion-vector scale");
    }

    auto missing_output = frame;
    missing_output.color_output = {};
    decision = EvaluateXessReplacementDecision(XessProxyMode::OsrExperimental, missing_output, true, true, true);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "missing_color_output")) {
        return Fail("osr mode should require output texture ownership");
    }

    decision = EvaluateXessReplacementDecision(XessProxyMode::OsrExperimental, frame, true, true, false);
    if (!decision.forward_to_real_xess || decision.run_osr || !HasReason(decision, "replacement_backend_unavailable")) {
        return Fail("osr mode should forward until the Vulkan writer exists");
    }

    decision = EvaluateXessReplacementDecision(XessProxyMode::OsrExperimental, frame, true, true, true);
    if (decision.forward_to_real_xess || !decision.run_osr || !HasReason(decision, "ready")) {
        return Fail("osr mode should choose replacement only when all gates pass and writer is available");
    }

    return 0;
}
