#include "reconstruction/history_rejection.h"

#include <algorithm>
#include <cmath>

namespace osr::reconstruction {

bool RejectHistoryByDepth(float previous_depth, float current_depth, const HistoryRejectionSettings& settings) {
    const float denominator = std::max(std::fabs(current_depth), 0.0001f);
    const float relative_delta = std::fabs(previous_depth - current_depth) / denominator;
    return relative_delta > settings.depth_relative_threshold;
}

} // namespace osr::reconstruction
