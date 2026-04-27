#pragma once

#include "core/frame_context.h"

namespace osr::reconstruction {

struct TemporalAccumulationSettings {
    float max_history_weight = 0.85f;
    bool conservative_first_frame = true;
};

[[nodiscard]] bool ShouldResetHistory(const core::FrameContext& previous, const core::FrameContext& current);

} // namespace osr::reconstruction

