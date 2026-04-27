#pragma once

#include "core/frame_context.h"

namespace osr::reconstruction {

struct SpatialUpscaleSettings {
    bool edge_aware = false;
    float sharpness = 0.0f;
};

[[nodiscard]] bool ValidateSpatialUpscaleInputs(const core::FrameContext& frame);

} // namespace osr::reconstruction

