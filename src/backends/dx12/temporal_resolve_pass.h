#pragma once

#include "core/frame_context.h"

#include <d3d12.h>

namespace osr::backends::dx12 {

struct TemporalResolveResources {
    ID3D12Resource* current_color = nullptr;
    ID3D12Resource* previous_history = nullptr;
    ID3D12Resource* current_depth = nullptr;
    ID3D12Resource* previous_depth = nullptr;
    ID3D12Resource* motion_vectors = nullptr;
    ID3D12Resource* reactive_mask = nullptr;
    ID3D12Resource* output_color = nullptr;
    ID3D12Resource* debug_history_weight = nullptr;
    ID3D12Resource* debug_color_residual = nullptr;
    ID3D12Resource* debug_depth_residual = nullptr;
};

struct TemporalResolveConstants {
    core::Dimensions render_size {};
    core::Dimensions display_size {};
    float max_history_weight = 0.92f;
    float reactive_penalty = 0.95f;
    float motion_rejection_pixels = 1.25f;
    float color_rejection_threshold = 0.12f;
    float depth_rejection_threshold = 0.025f;
    float sharpening_amount = 0.28f;
    float sharpening_low_trust_scale = 0.20f;
    float sharpening_reactive_scale = 0.25f;
    float history_clip_margin = 0.02f;
    core::Float2 jitter_offset {};
};

class TemporalResolvePass {
public:
    ~TemporalResolvePass();

    bool Initialize(void* native_device);
    bool Dispatch(void* native_command_list,
                  const core::FrameContext& frame,
                  const TemporalResolveResources& resources,
                  const TemporalResolveConstants& constants);

private:
    void Shutdown() noexcept;

    ID3D12Device* native_device_ = nullptr;
    ID3D12RootSignature* root_signature_ = nullptr;
    ID3D12PipelineState* pipeline_state_ = nullptr;
    ID3D12DescriptorHeap* descriptor_heap_ = nullptr;
    uint32_t descriptor_size_ = 0;
};

} // namespace osr::backends::dx12
