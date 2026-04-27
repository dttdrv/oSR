#pragma once

#include "backends/dx12/debug_upscale_pass.h"
#include "core/frame_context.h"

namespace osr::backends::dx12 {

class Dx12Backend {
public:
    bool Initialize(void* native_device);
    bool DispatchDebugUpscale(void* native_command_list, const core::FrameContext& frame);

private:
    void* native_device_ = nullptr;
    DebugUpscalePass debug_upscale_;
};

} // namespace osr::backends::dx12

