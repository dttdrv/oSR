#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <string>

namespace osr::backends::dx12 {

struct DescriptorRange {
    uint32_t srv_count = 0;
    uint32_t uav_count = 0;
    uint32_t cbv_count = 0;
};

[[nodiscard]] DescriptorRange RequiredDebugUpscaleDescriptors(const core::FrameContext& frame);
[[nodiscard]] std::string DescribeDescriptorRange(const DescriptorRange& range);

} // namespace osr::backends::dx12

