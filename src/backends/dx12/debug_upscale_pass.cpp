#include "backends/dx12/debug_upscale_pass.h"

#include "backends/dx12/descriptor_helpers.h"
#include "core/logging.h"

#include <sstream>

namespace osr::backends::dx12 {

DebugUpscaleDispatch BuildDebugUpscaleDispatch(const core::FrameContext& frame) {
    DebugUpscaleDispatch dispatch;
    dispatch.groups_x = (frame.display_size.width + 7u) / 8u;
    dispatch.groups_y = (frame.display_size.height + 7u) / 8u;
    return dispatch;
}

bool DebugUpscalePass::Initialize(void* native_device) {
    native_device_ = native_device;
    core::Logger::Instance().Log(core::LogLevel::Info, 0, "dx12.debug_upscale", "Initialized debug upscale pass skeleton.");
    return true;
}

bool DebugUpscalePass::Dispatch(void* native_command_list, const core::FrameContext& frame) {
    const auto dispatch = BuildDebugUpscaleDispatch(frame);
    const auto descriptors = RequiredDebugUpscaleDescriptors(frame);

    std::ostringstream message;
    message << "Dispatch debug upscale shader=" << dispatch.shader_name
            << " groups=(" << dispatch.groups_x << "," << dispatch.groups_y << ") "
            << DescribeDescriptorRange(descriptors);

    if (native_device_ == nullptr || native_command_list == nullptr) {
        message << " mode=metadata_only";
        core::Logger::Instance().Log(core::LogLevel::Warning, frame.frame_id, "dx12.debug_upscale", message.str());
        return false;
    }

    message << " mode=dx12_command_recording_pending";
    core::Logger::Instance().Log(core::LogLevel::Info, frame.frame_id, "dx12.debug_upscale", message.str());
    return true;
}

} // namespace osr::backends::dx12

