#include "backends/dx12/temporal_resolve_pass.h"

#include "core/logging.h"

#include <d3dcompiler.h>

#include <sstream>
#include <string>
#include <vector>

namespace osr::backends::dx12 {

namespace {

template <typename T>
void SafeRelease(T*& value) noexcept {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

bool Failed(HRESULT hr) noexcept {
    return FAILED(hr);
}

constexpr const char* kTemporalResolveHlsl = R"(
Texture2D<float4> g_current_color : register(t0);
Texture2D<float4> g_previous_history : register(t1);
Texture2D<float> g_current_depth : register(t2);
Texture2D<float> g_previous_depth : register(t3);
Texture2D<float2> g_motion_vectors : register(t4);
Texture2D<float> g_reactive_mask : register(t5);
RWTexture2D<float4> g_output_color : register(u0);
RWTexture2D<float> g_debug_history_weight : register(u1);
RWTexture2D<float> g_debug_color_residual : register(u2);
RWTexture2D<float> g_debug_depth_residual : register(u3);

cbuffer TemporalConstants : register(b0)
{
    uint2 g_render_size;
    uint2 g_display_size;
    float g_max_history_weight;
    float g_reactive_penalty;
    float g_motion_rejection_pixels;
    float g_color_rejection_threshold;
    float g_depth_rejection_threshold;
    float g_sharpening_amount;
    float g_sharpening_low_trust_scale;
    float g_sharpening_reactive_scale;
    float g_history_clip_margin;
    float2 g_jitter_offset;
};

float Luma(float3 c)
{
    return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float4 QuantizeRgba8(float4 c)
{
    return floor(saturate(c) * 255.0f + 0.5f) / 255.0f;
}

float4 SampleRenderColor(Texture2D<float4> texture_source, float2 p);

float4 SampleCurrentDisplay(float2 display_px)
{
    float2 render_float = ((display_px + 0.5f) * float2(g_render_size) / float2(g_display_size)) - 0.5f - g_jitter_offset;
    return QuantizeRgba8(SampleRenderColor(g_current_color, render_float));
}

float4 ApplyDetailRecovery(float4 resolved, float2 display_px, float history_weight, float reactive, bool disoccluded)
{
    if (g_sharpening_amount <= 0.0f || disoccluded)
    {
        return resolved;
    }

    float trust_scale = lerp(saturate(g_sharpening_low_trust_scale), 1.0f, saturate(history_weight));
    float reactive_scale = lerp(1.0f, saturate(g_sharpening_reactive_scale), saturate(reactive));
    float amount = saturate(g_sharpening_amount) * trust_scale * reactive_scale;
    if (amount <= 0.0f)
    {
        return resolved;
    }

    float2 left_px = float2(max(display_px.x - 1.0f, 0.0f), display_px.y);
    float2 right_px = float2(min(display_px.x + 1.0f, float(g_display_size.x - 1)), display_px.y);
    float2 up_px = float2(display_px.x, max(display_px.y - 1.0f, 0.0f));
    float2 down_px = float2(display_px.x, min(display_px.y + 1.0f, float(g_display_size.y - 1)));
    float3 center = SampleCurrentDisplay(display_px).rgb;
    float3 average = (SampleCurrentDisplay(left_px).rgb +
                      SampleCurrentDisplay(right_px).rgb +
                      SampleCurrentDisplay(up_px).rgb +
                      SampleCurrentDisplay(down_px).rgb) * 0.25f;
    resolved.rgb = saturate(resolved.rgb + (center - average) * amount);
    return resolved;
}

float4 SampleDisplay(Texture2D<float4> texture_source, float2 p)
{
    p = clamp(p, 0.0f, float2(g_display_size - 1));
    uint2 p0 = uint2(floor(p));
    uint2 p1 = min(p0 + 1, g_display_size - 1);
    float2 t = p - float2(p0);
    float4 c00 = texture_source.Load(int3(p0, 0));
    float4 c10 = texture_source.Load(int3(p1.x, p0.y, 0));
    float4 c01 = texture_source.Load(int3(p0.x, p1.y, 0));
    float4 c11 = texture_source.Load(int3(p1, 0));
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}

float4 ClipHistoryToCurrentNeighborhood(float4 history_color, float2 display_px)
{
    if (g_history_clip_margin <= 0.0f)
    {
        return history_color;
    }

    float2 left_px = float2(max(display_px.x - 1.0f, 0.0f), display_px.y);
    float2 right_px = float2(min(display_px.x + 1.0f, float(g_display_size.x - 1)), display_px.y);
    float2 up_px = float2(display_px.x, max(display_px.y - 1.0f, 0.0f));
    float2 down_px = float2(display_px.x, min(display_px.y + 1.0f, float(g_display_size.y - 1)));
    float3 c0 = SampleCurrentDisplay(display_px).rgb;
    float3 c1 = SampleCurrentDisplay(left_px).rgb;
    float3 c2 = SampleCurrentDisplay(right_px).rgb;
    float3 c3 = SampleCurrentDisplay(up_px).rgb;
    float3 c4 = SampleCurrentDisplay(down_px).rgb;
    float3 lo = min(c0, min(c1, min(c2, min(c3, c4))));
    float3 hi = max(c0, max(c1, max(c2, max(c3, c4))));
    history_color.rgb = clamp(history_color.rgb, saturate(lo - g_history_clip_margin), saturate(hi + g_history_clip_margin));
    return QuantizeRgba8(history_color);
}

float4 SampleRenderColor(Texture2D<float4> texture_source, float2 p)
{
    p = clamp(p, 0.0f, float2(g_render_size - 1));
    uint2 p0 = uint2(floor(p));
    uint2 p1 = min(p0 + 1, g_render_size - 1);
    float2 t = p - float2(p0);
    float4 c00 = texture_source.Load(int3(p0, 0));
    float4 c10 = texture_source.Load(int3(p1.x, p0.y, 0));
    float4 c01 = texture_source.Load(int3(p0.x, p1.y, 0));
    float4 c11 = texture_source.Load(int3(p1, 0));
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}

float SampleRenderFloat(Texture2D<float> texture_source, float2 p)
{
    p = clamp(p, 0.0f, float2(g_render_size - 1));
    uint2 p0 = uint2(floor(p));
    uint2 p1 = min(p0 + 1, g_render_size - 1);
    float2 t = p - float2(p0);
    float c00 = texture_source.Load(int3(p0, 0));
    float c10 = texture_source.Load(int3(p1.x, p0.y, 0));
    float c01 = texture_source.Load(int3(p0.x, p1.y, 0));
    float c11 = texture_source.Load(int3(p1, 0));
    return lerp(lerp(c00, c10, t.x), lerp(c01, c11, t.x), t.y);
}

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_display_size.x || dispatch_thread_id.y >= g_display_size.y)
    {
        return;
    }

    uint2 out_px = dispatch_thread_id.xy;
    float2 display_px = float2(out_px);
    uint2 render_px = min((out_px * g_render_size) / g_display_size, g_render_size - 1);

    float4 current_color = SampleCurrentDisplay(display_px);
    float2 mv = g_motion_vectors.Load(int3(render_px, 0));
    float motion_len = length(mv);
    float history_weight = saturate(g_max_history_weight);

    float reactive = g_reactive_mask.Load(int3(render_px, 0));
    history_weight *= saturate(1.0f - reactive * g_reactive_penalty);

    float2 history_px = display_px;
    float previous_depth = g_previous_depth.Load(int3(render_px, 0));
    bool previous_depth_oob = false;
    bool disoccluded = false;
    if (motion_len > 0.01f)
    {
        float2 display_per_render = float2(g_display_size) / float2(g_render_size);
        history_px = display_px + mv * display_per_render;
        if (history_px.x < 0.0f || history_px.y < 0.0f ||
            history_px.x > float(g_display_size.x - 1) || history_px.y > float(g_display_size.y - 1))
        {
            history_weight = 0.0f;
        }

        float2 previous_render_px = float2(render_px) + mv;
        if (previous_render_px.x < 0.0f || previous_render_px.y < 0.0f ||
            previous_render_px.x > float(g_render_size.x - 1) || previous_render_px.y > float(g_render_size.y - 1))
        {
            previous_depth_oob = true;
            disoccluded = true;
            history_weight = 0.0f;
        }
        else
        {
            previous_depth = SampleRenderFloat(g_previous_depth, previous_render_px);
        }
    }

    if (motion_len > g_motion_rejection_pixels)
    {
        history_weight = 0.0f;
    }
    else if (g_motion_rejection_pixels > 0.0f)
    {
        history_weight *= saturate(1.0f - motion_len / g_motion_rejection_pixels);
    }

    float4 history_color = ClipHistoryToCurrentNeighborhood(QuantizeRgba8(SampleDisplay(g_previous_history, history_px)), display_px);
    float color_residual = abs(Luma(current_color.rgb) - Luma(history_color.rgb));
    if (g_color_rejection_threshold > 0.0f && color_residual > g_color_rejection_threshold)
    {
        history_weight = 0.0f;
    }
    else if (g_color_rejection_threshold > 0.0f)
    {
        history_weight *= saturate(1.0f - color_residual / g_color_rejection_threshold);
    }

    float current_depth = g_current_depth.Load(int3(render_px, 0));
    float depth_residual = previous_depth_oob ? 1.0f : abs(current_depth - previous_depth);
    if (g_depth_rejection_threshold > 0.0f && depth_residual > g_depth_rejection_threshold)
    {
        history_weight = 0.0f;
        disoccluded = true;
    }
    else if (g_depth_rejection_threshold > 0.0f)
    {
        history_weight *= saturate(1.0f - depth_residual / g_depth_rejection_threshold);
    }

    float4 blended = QuantizeRgba8(lerp(current_color, history_color, history_weight));
    float4 resolved = ApplyDetailRecovery(blended, display_px, history_weight, reactive, disoccluded);
    g_output_color[out_px] = QuantizeRgba8(resolved);
    g_debug_history_weight[out_px] = history_weight;
    g_debug_color_residual[out_px] = color_residual;
    g_debug_depth_residual[out_px] = depth_residual;
}
)";

} // namespace

TemporalResolvePass::~TemporalResolvePass() {
    Shutdown();
}

void TemporalResolvePass::Shutdown() noexcept {
    SafeRelease(descriptor_heap_);
    SafeRelease(pipeline_state_);
    SafeRelease(root_signature_);
    SafeRelease(native_device_);
    descriptor_size_ = 0;
}

bool TemporalResolvePass::Initialize(void* native_device) {
    Shutdown();
    native_device_ = static_cast<ID3D12Device*>(native_device);
    if (!native_device_) {
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.temporal_resolve", "Temporal resolve pass requires a native D3D12 device.");
        return false;
    }
    native_device_->AddRef();

    D3D12_DESCRIPTOR_RANGE ranges[2] {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 6;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 4;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[2] {};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[0].DescriptorTable.NumDescriptorRanges = 2;
    root_params[0].DescriptorTable.pDescriptorRanges = ranges;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_params[1].Constants.ShaderRegister = 0;
    root_params[1].Constants.Num32BitValues = 15;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC root_desc {};
    root_desc.NumParameters = 2;
    root_desc.pParameters = root_params;
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ID3DBlob* root_blob = nullptr;
    ID3DBlob* error_blob = nullptr;
    if (Failed(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &root_blob, &error_blob))) {
        const std::string error = error_blob ? static_cast<const char*>(error_blob->GetBufferPointer()) : "unknown";
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.temporal_resolve", "Failed to serialize root signature: " + error);
        SafeRelease(error_blob);
        return false;
    }
    SafeRelease(error_blob);

    if (Failed(native_device_->CreateRootSignature(0,
                                                   root_blob->GetBufferPointer(),
                                                   root_blob->GetBufferSize(),
                                                   IID_PPV_ARGS(&root_signature_)))) {
        SafeRelease(root_blob);
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.temporal_resolve", "Failed to create root signature.");
        return false;
    }
    SafeRelease(root_blob);

    ID3DBlob* shader_blob = nullptr;
    if (Failed(D3DCompile(kTemporalResolveHlsl,
                          std::char_traits<char>::length(kTemporalResolveHlsl),
                          "osr_temporal_resolve",
                          nullptr,
                          nullptr,
                          "main",
                          "cs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3,
                          0,
                          &shader_blob,
                          &error_blob))) {
        const std::string error = error_blob ? static_cast<const char*>(error_blob->GetBufferPointer()) : "unknown";
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.temporal_resolve", "Failed to compile temporal resolve shader: " + error);
        SafeRelease(error_blob);
        return false;
    }
    SafeRelease(error_blob);

    D3D12_COMPUTE_PIPELINE_STATE_DESC pso_desc {};
    pso_desc.pRootSignature = root_signature_;
    pso_desc.CS.pShaderBytecode = shader_blob->GetBufferPointer();
    pso_desc.CS.BytecodeLength = shader_blob->GetBufferSize();
    if (Failed(native_device_->CreateComputePipelineState(&pso_desc, IID_PPV_ARGS(&pipeline_state_)))) {
        SafeRelease(shader_blob);
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.temporal_resolve", "Failed to create compute PSO.");
        return false;
    }
    SafeRelease(shader_blob);

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 10;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (Failed(native_device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&descriptor_heap_)))) {
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.temporal_resolve", "Failed to create descriptor heap.");
        return false;
    }
    descriptor_size_ = native_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    core::Logger::Instance().Log(core::LogLevel::Info, 0, "dx12.temporal_resolve", "Initialized temporal resolve compute pass.");
    return true;
}

bool TemporalResolvePass::Dispatch(void* native_command_list,
                                   const core::FrameContext& frame,
                                   const TemporalResolveResources& resources,
                                   const TemporalResolveConstants& constants) {
    if (!native_device_ || !native_command_list ||
        !resources.current_color || !resources.previous_history ||
        !resources.current_depth || !resources.previous_depth ||
        !resources.motion_vectors || !resources.reactive_mask ||
        !resources.output_color ||
        !resources.debug_history_weight ||
        !resources.debug_color_residual ||
        !resources.debug_depth_residual) {
        core::Logger::Instance().Log(core::LogLevel::Error, frame.frame_id, "dx12.temporal_resolve", "Missing temporal resolve resource.");
        return false;
    }

    auto* command_list = static_cast<ID3D12GraphicsCommandList*>(native_command_list);
    auto cpu = descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
    auto write_srv = [&](ID3D12Resource* resource, DXGI_FORMAT format, uint32_t slot) {
        D3D12_SHADER_RESOURCE_VIEW_DESC srv {};
        srv.Format = format;
        srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
        srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srv.Texture2D.MipLevels = 1;
        auto handle = cpu;
        handle.ptr += static_cast<size_t>(slot) * descriptor_size_;
        native_device_->CreateShaderResourceView(resource, &srv, handle);
    };
    write_srv(resources.current_color, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    write_srv(resources.previous_history, DXGI_FORMAT_R8G8B8A8_UNORM, 1);
    write_srv(resources.current_depth, DXGI_FORMAT_R32_FLOAT, 2);
    write_srv(resources.previous_depth, DXGI_FORMAT_R32_FLOAT, 3);
    write_srv(resources.motion_vectors, DXGI_FORMAT_R32G32_FLOAT, 4);
    write_srv(resources.reactive_mask, DXGI_FORMAT_R32_FLOAT, 5);

    auto write_uav = [&](ID3D12Resource* resource, DXGI_FORMAT format, uint32_t slot) {
        D3D12_UNORDERED_ACCESS_VIEW_DESC uav {};
        uav.Format = format;
        uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
        auto handle = cpu;
        handle.ptr += static_cast<size_t>(6u + slot) * descriptor_size_;
        native_device_->CreateUnorderedAccessView(resource, nullptr, &uav, handle);
    };
    write_uav(resources.output_color, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
    write_uav(resources.debug_history_weight, DXGI_FORMAT_R32_FLOAT, 1);
    write_uav(resources.debug_color_residual, DXGI_FORMAT_R32_FLOAT, 2);
    write_uav(resources.debug_depth_residual, DXGI_FORMAT_R32_FLOAT, 3);

    std::vector<D3D12_RESOURCE_BARRIER> to_uav;
    auto push_transition = [&](ID3D12Resource* resource) {
        if (!resource) {
            return;
        }
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource = resource;
        barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
        to_uav.push_back(barrier);
    };
    push_transition(resources.output_color);
    push_transition(resources.debug_history_weight);
    push_transition(resources.debug_color_residual);
    push_transition(resources.debug_depth_residual);
    command_list->ResourceBarrier(static_cast<UINT>(to_uav.size()), to_uav.data());

    ID3D12DescriptorHeap* heaps[] = {descriptor_heap_};
    command_list->SetDescriptorHeaps(1, heaps);
    command_list->SetComputeRootSignature(root_signature_);
    command_list->SetPipelineState(pipeline_state_);
    command_list->SetComputeRootDescriptorTable(0, descriptor_heap_->GetGPUDescriptorHandleForHeapStart());

    struct Constants {
        uint32_t render_width;
        uint32_t render_height;
        uint32_t display_width;
        uint32_t display_height;
        float max_history_weight;
        float reactive_penalty;
        float motion_rejection_pixels;
        float color_rejection_threshold;
        float depth_rejection_threshold;
        float sharpening_amount;
        float sharpening_low_trust_scale;
        float sharpening_reactive_scale;
        float history_clip_margin;
        float jitter_x;
        float jitter_y;
    };
    const Constants c {
        constants.render_size.width,
        constants.render_size.height,
        constants.display_size.width,
        constants.display_size.height,
        constants.max_history_weight,
        constants.reactive_penalty,
        constants.motion_rejection_pixels,
        constants.color_rejection_threshold,
        constants.depth_rejection_threshold,
        constants.sharpening_amount,
        constants.sharpening_low_trust_scale,
        constants.sharpening_reactive_scale,
        constants.history_clip_margin,
        constants.jitter_offset.x,
        constants.jitter_offset.y
    };
    command_list->SetComputeRoot32BitConstants(1, 15, &c, 0);
    command_list->Dispatch((constants.display_size.width + 7u) / 8u,
                           (constants.display_size.height + 7u) / 8u,
                           1);

    std::vector<D3D12_RESOURCE_BARRIER> barriers;
    auto push_uav_barrier = [&](ID3D12Resource* resource) {
        if (!resource) {
            return;
        }
        D3D12_RESOURCE_BARRIER barrier {};
        barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = resource;
        barriers.push_back(barrier);
    };
    push_uav_barrier(resources.output_color);
    push_uav_barrier(resources.debug_history_weight);
    push_uav_barrier(resources.debug_color_residual);
    push_uav_barrier(resources.debug_depth_residual);
    for (auto barrier : to_uav) {
        std::swap(barrier.Transition.StateBefore, barrier.Transition.StateAfter);
        barriers.push_back(barrier);
    }
    command_list->ResourceBarrier(static_cast<UINT>(barriers.size()), barriers.data());

    std::ostringstream message;
    message << "Dispatch temporal resolve groups=("
            << ((constants.display_size.width + 7u) / 8u) << ","
            << ((constants.display_size.height + 7u) / 8u) << ")"
            << " render=" << constants.render_size.width << "x" << constants.render_size.height
            << " display=" << constants.display_size.width << "x" << constants.display_size.height;
    core::Logger::Instance().Log(core::LogLevel::Info, frame.frame_id, "dx12.temporal_resolve", message.str());
    return true;
}

} // namespace osr::backends::dx12
