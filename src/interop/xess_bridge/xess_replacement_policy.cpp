#include "interop/xess_bridge/xess_replacement_policy.h"

#include <cmath>
#include <cstring>
#include <utility>

namespace osr::interop::xess_bridge {

namespace {

bool Equals(const char* lhs, const char* rhs) noexcept {
    return lhs != nullptr && std::strcmp(lhs, rhs) == 0;
}

bool ZeroMotionVectorScale(const core::FrameContext& frame) noexcept {
    return std::fabs(frame.motion_vector_scale.x) < 0.00001f &&
           std::fabs(frame.motion_vector_scale.y) < 0.00001f;
}

XessReplacementDecision Forward(std::string reason) {
    XessReplacementDecision decision;
    decision.forward_to_real_xess = true;
    decision.run_osr = false;
    decision.reason = std::move(reason);
    return decision;
}

XessReplacementDecision Replace(std::string reason) {
    XessReplacementDecision decision;
    decision.forward_to_real_xess = false;
    decision.run_osr = true;
    decision.reason = std::move(reason);
    return decision;
}

} // namespace

XessProxyMode ParseXessProxyMode(const char* value) noexcept {
    if (value == nullptr || value[0] == '\0' ||
        Equals(value, "passthrough") ||
        Equals(value, "forward")) {
        return XessProxyMode::Passthrough;
    }
    if (Equals(value, "observe") || Equals(value, "log")) {
        return XessProxyMode::Observe;
    }
    if (Equals(value, "osr") ||
        Equals(value, "replace") ||
        Equals(value, "replacement") ||
        Equals(value, "experimental")) {
        return XessProxyMode::OsrExperimental;
    }
    return XessProxyMode::Passthrough;
}

const char* ToString(XessProxyMode mode) noexcept {
    switch (mode) {
    case XessProxyMode::Passthrough: return "passthrough";
    case XessProxyMode::Observe: return "observe";
    case XessProxyMode::OsrExperimental: return "osr";
    }
    return "passthrough";
}

XessReplacementDecision EvaluateXessReplacementDecision(XessProxyMode mode,
                                                        const core::FrameContext& frame,
                                                        bool has_execute_params,
                                                        bool has_command_buffer,
                                                        bool replacement_backend_available) {
    if (mode == XessProxyMode::Passthrough) {
        return Forward("mode=passthrough");
    }
    if (mode == XessProxyMode::Observe) {
        return Forward("mode=observe");
    }
    if (!has_execute_params) {
        return Forward("missing_execute_params");
    }
    if (!has_command_buffer) {
        return Forward("missing_command_buffer");
    }
    if (!frame.color_input.IsPresent()) {
        return Forward("missing_color_input");
    }
    if (!frame.color_output.IsPresent()) {
        return Forward("missing_color_output");
    }
    if (!frame.depth.IsPresent()) {
        return Forward("missing_depth");
    }
    if (!frame.motion_vectors.IsPresent()) {
        return Forward("missing_motion_vectors");
    }
    if (!frame.render_size.IsValid()) {
        return Forward("invalid_render_size");
    }
    if (!frame.upscale_size.IsValid() || !frame.display_size.IsValid()) {
        return Forward("invalid_output_size");
    }
    if (frame.motion_vector_space == core::MotionVectorSpace::Unknown) {
        return Forward("unknown_motion_vector_space");
    }
    if (ZeroMotionVectorScale(frame)) {
        return Forward("zero_motion_vector_scale");
    }
    if (frame.exposure.exposure_scale <= 0.0f || frame.exposure.pre_exposure <= 0.0f) {
        return Forward("invalid_exposure_scale");
    }
    if (!replacement_backend_available) {
        return Forward("replacement_backend_unavailable");
    }
    return Replace("ready");
}

} // namespace osr::interop::xess_bridge
