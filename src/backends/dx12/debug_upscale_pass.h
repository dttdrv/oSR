#pragma once

#include "core/frame_context.h"

#include <string>

namespace osr::backends::dx12 {

struct DebugUpscaleDispatch {
    uint32_t groups_x = 0;
    uint32_t groups_y = 0;
    std::string shader_name = "spatial_debug_upscale.hlsl";
};

[[nodiscard]] DebugUpscaleDispatch BuildDebugUpscaleDispatch(const core::FrameContext& frame);

class DebugUpscalePass {
public:
    bool Initialize(void* native_device);
    bool Dispatch(void* native_command_list, const core::FrameContext& frame);

private:
    void* native_device_ = nullptr;
};

} // namespace osr::backends::dx12

