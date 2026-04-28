#pragma once

#include "core/frame_context.h"

#include <d3d12.h>
#include <string>

namespace osr::backends::dx12 {

enum class DebugViewMode {
    Final,
    MotionVectors,
    Depth,
    Disocclusion,
    Reactive,
    SynthesizedReactive,
    HistoryTrust,
    AccumulationWeight
};

struct DebugUpscaleDispatch {
    uint32_t groups_x = 0;
    uint32_t groups_y = 0;
    std::string shader_name = "spatial_debug_upscale.hlsl";
    DebugViewMode debug_view = DebugViewMode::Final;
};

[[nodiscard]] const char* ToString(DebugViewMode mode) noexcept;
[[nodiscard]] DebugUpscaleDispatch BuildDebugUpscaleDispatch(const core::FrameContext& frame);

class DebugUpscalePass {
public:
    ~DebugUpscalePass();

    bool Initialize(void* native_device);
    bool Dispatch(void* native_command_list, const core::FrameContext& frame);

private:
    void Shutdown() noexcept;

    ID3D12Device* native_device_ = nullptr;
    ID3D12DescriptorHeap* heartbeat_uav_heap_ = nullptr;
    ID3D12Resource* heartbeat_texture_ = nullptr;
};

} // namespace osr::backends::dx12
