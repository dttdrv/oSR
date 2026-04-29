#pragma once

#include "core/frame_context.h"

#include <string>

namespace osr::interop::xess_bridge {

enum class XessProxyMode {
    Passthrough,
    Observe,
    OsrExperimental
};

struct XessReplacementDecision {
    bool forward_to_real_xess = true;
    bool run_osr = false;
    std::string reason;
};

[[nodiscard]] XessProxyMode ParseXessProxyMode(const char* value) noexcept;
[[nodiscard]] const char* ToString(XessProxyMode mode) noexcept;
[[nodiscard]] XessReplacementDecision EvaluateXessReplacementDecision(XessProxyMode mode,
                                                                      const core::FrameContext& frame,
                                                                      bool has_execute_params,
                                                                      bool has_command_buffer,
                                                                      bool replacement_backend_available);

} // namespace osr::interop::xess_bridge
