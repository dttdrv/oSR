#include "backends/dx12/dx12_backend.h"

#include "core/logging.h"

namespace osr::backends::dx12 {

bool Dx12Backend::Initialize(void* native_device) {
    native_device_ = native_device;
    core::Logger::Instance().Log(core::LogLevel::Info, 0, "dx12.backend", "DX12 backend initialized.");
    return debug_upscale_.Initialize(native_device_);
}

bool Dx12Backend::DispatchDebugUpscale(void* native_command_list, const core::FrameContext& frame) {
    core::Logger::Instance().Log(core::LogLevel::Debug, frame.frame_id, "dx12.backend", "Dispatching debug upscale path.");
    return debug_upscale_.Dispatch(native_command_list, frame);
}

} // namespace osr::backends::dx12

