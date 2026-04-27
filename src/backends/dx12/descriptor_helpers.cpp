#include "backends/dx12/descriptor_helpers.h"

#include <sstream>

namespace osr::backends::dx12 {

DescriptorRange RequiredDebugUpscaleDescriptors(const core::FrameContext& frame) {
    DescriptorRange range;
    range.srv_count = 1;
    range.uav_count = 1;
    range.cbv_count = 1;

    if (frame.depth.IsPresent()) {
        ++range.srv_count;
    }
    if (frame.motion_vectors.IsPresent()) {
        ++range.srv_count;
    }
    if (frame.reactive_mask.has_value() && frame.reactive_mask->IsPresent()) {
        ++range.srv_count;
    } else {
        ++range.internal_uav_count;
    }

    ++range.internal_uav_count;
    return range;
}

std::string DescribeDescriptorRange(const DescriptorRange& range) {
    std::ostringstream out;
    out << "srv=" << range.srv_count
        << " uav=" << range.uav_count
        << " cbv=" << range.cbv_count
        << " internal_uav=" << range.internal_uav_count;
    return out.str();
}

} // namespace osr::backends::dx12
