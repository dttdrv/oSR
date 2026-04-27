#include "reconstruction/disocclusion.h"

#include <cmath>

namespace osr::reconstruction {

bool IsLikelyDisoccluded(float depth_delta, float motion_length_pixels, const DisocclusionSettings& settings) {
    return std::fabs(depth_delta) > settings.depth_threshold ||
           motion_length_pixels > settings.motion_threshold_pixels;
}

} // namespace osr::reconstruction

