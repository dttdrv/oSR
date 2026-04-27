#pragma once

namespace osr::reconstruction {

struct HistoryRejectionSettings {
    float depth_relative_threshold = 0.02f;
    float reactive_history_scale = 0.25f;
};

[[nodiscard]] bool RejectHistoryByDepth(float previous_depth, float current_depth, const HistoryRejectionSettings& settings);

} // namespace osr::reconstruction

