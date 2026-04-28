#include "backends/dx12/debug_upscale_pass.h"

#include "backends/dx12/descriptor_helpers.h"
#include "core/logging.h"

#include <d3dcompiler.h>

#include <sstream>

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

constexpr const char* kSpatialDebugUpscaleHlsl = R"(
Texture2D<float4> g_input_color : register(t0);
RWTexture2D<float4> g_output_color : register(u0);

cbuffer UpscaleConstants : register(b0)
{
    uint2 g_input_size;
    uint2 g_output_size;
};

[numthreads(8, 8, 1)]
void main(uint3 dispatch_thread_id : SV_DispatchThreadID)
{
    if (dispatch_thread_id.x >= g_output_size.x || dispatch_thread_id.y >= g_output_size.y)
    {
        return;
    }

    float2 src = ((float2(dispatch_thread_id.xy) + 0.5f) * float2(g_input_size) / float2(g_output_size)) - 0.5f;
    src = clamp(src, 0.0f, float2(g_input_size - 1));

    uint2 p0 = uint2(floor(src));
    uint2 p1 = min(p0 + 1, g_input_size - 1);
    float2 t = src - float2(p0);

    float4 c00 = g_input_color.Load(int3(p0, 0));
    float4 c10 = g_input_color.Load(int3(p1.x, p0.y, 0));
    float4 c01 = g_input_color.Load(int3(p0.x, p1.y, 0));
    float4 c11 = g_input_color.Load(int3(p1, 0));
    float4 top = lerp(c00, c10, t.x);
    float4 bottom = lerp(c01, c11, t.x);
    float4 color = lerp(top, bottom, t.y);
    g_output_color[dispatch_thread_id.xy] = floor(saturate(color) * 255.0f + 0.5f) / 255.0f;
}
)";

} // namespace

const char* ToString(DebugViewMode mode) noexcept {
    switch (mode) {
    case DebugViewMode::Final: return "Final";
    case DebugViewMode::MotionVectors: return "MotionVectors";
    case DebugViewMode::Depth: return "Depth";
    case DebugViewMode::Disocclusion: return "Disocclusion";
    case DebugViewMode::Reactive: return "Reactive";
    case DebugViewMode::SynthesizedReactive: return "SynthesizedReactive";
    case DebugViewMode::HistoryTrust: return "HistoryTrust";
    case DebugViewMode::AccumulationWeight: return "AccumulationWeight";
    default: return "Unknown";
    }
}

DebugUpscaleDispatch BuildDebugUpscaleDispatch(const core::FrameContext& frame) {
    DebugUpscaleDispatch dispatch;
    dispatch.groups_x = (frame.display_size.width + 7u) / 8u;
    dispatch.groups_y = (frame.display_size.height + 7u) / 8u;
    return dispatch;
}

DebugUpscalePass::~DebugUpscalePass() {
    Shutdown();
}

void DebugUpscalePass::Shutdown() noexcept {
    SafeRelease(descriptor_heap_);
    SafeRelease(pipeline_state_);
    SafeRelease(root_signature_);
    SafeRelease(native_device_);
    descriptor_size_ = 0;
}

bool DebugUpscalePass::Initialize(void* native_device) {
    Shutdown();
    native_device_ = static_cast<ID3D12Device*>(native_device);
    if (!native_device_) {
        core::Logger::Instance().Log(core::LogLevel::Warning, 0, "dx12.debug_upscale", "Initialized debug upscale pass in metadata-only mode.");
        return true;
    }
    native_device_->AddRef();

    D3D12_DESCRIPTOR_RANGE ranges[2] {};
    ranges[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    ranges[0].NumDescriptors = 1;
    ranges[0].BaseShaderRegister = 0;
    ranges[0].RegisterSpace = 0;
    ranges[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
    ranges[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
    ranges[1].NumDescriptors = 1;
    ranges[1].BaseShaderRegister = 0;
    ranges[1].RegisterSpace = 0;
    ranges[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

    D3D12_ROOT_PARAMETER root_params[2] {};
    root_params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    root_params[0].DescriptorTable.NumDescriptorRanges = 2;
    root_params[0].DescriptorTable.pDescriptorRanges = ranges;
    root_params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
    root_params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    root_params[1].Constants.ShaderRegister = 0;
    root_params[1].Constants.RegisterSpace = 0;
    root_params[1].Constants.Num32BitValues = 4;
    root_params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

    D3D12_ROOT_SIGNATURE_DESC root_desc {};
    root_desc.NumParameters = 2;
    root_desc.pParameters = root_params;
    root_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

    ID3DBlob* root_blob = nullptr;
    ID3DBlob* error_blob = nullptr;
    if (Failed(D3D12SerializeRootSignature(&root_desc, D3D_ROOT_SIGNATURE_VERSION_1, &root_blob, &error_blob))) {
        std::string error = error_blob ? static_cast<const char*>(error_blob->GetBufferPointer()) : "unknown";
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to serialize root signature: " + error);
        SafeRelease(error_blob);
        return false;
    }
    SafeRelease(error_blob);

    if (Failed(native_device_->CreateRootSignature(0,
                                                   root_blob->GetBufferPointer(),
                                                   root_blob->GetBufferSize(),
                                                   IID_PPV_ARGS(&root_signature_)))) {
        SafeRelease(root_blob);
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to create root signature.");
        return false;
    }
    SafeRelease(root_blob);

    ID3DBlob* shader_blob = nullptr;
    if (Failed(D3DCompile(kSpatialDebugUpscaleHlsl,
                          std::char_traits<char>::length(kSpatialDebugUpscaleHlsl),
                          "osr_spatial_debug_upscale",
                          nullptr,
                          nullptr,
                          "main",
                          "cs_5_0",
                          D3DCOMPILE_OPTIMIZATION_LEVEL3,
                          0,
                          &shader_blob,
                          &error_blob))) {
        std::string error = error_blob ? static_cast<const char*>(error_blob->GetBufferPointer()) : "unknown";
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to compile spatial debug shader: " + error);
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
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to create spatial debug compute PSO.");
        return false;
    }
    SafeRelease(shader_blob);

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 2;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (Failed(native_device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&descriptor_heap_)))) {
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to create debug upscale descriptor heap.");
        return false;
    }
    descriptor_size_ = native_device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    core::Logger::Instance().Log(core::LogLevel::Info, 0, "dx12.debug_upscale", "Initialized debug upscale pass with runtime-compiled compute shader.");
    return true;
}

bool DebugUpscalePass::Dispatch(void* native_command_list, const core::FrameContext& frame) {
    const auto dispatch = BuildDebugUpscaleDispatch(frame);
    const auto descriptors = RequiredDebugUpscaleDescriptors(frame);

    std::ostringstream message;
    message << "Dispatch debug upscale shader=" << dispatch.shader_name
            << " view=" << ToString(dispatch.debug_view)
            << " groups=(" << dispatch.groups_x << "," << dispatch.groups_y << ") "
            << DescribeDescriptorRange(descriptors);

    if (native_device_ == nullptr || native_command_list == nullptr || descriptor_heap_ == nullptr) {
        message << " mode=metadata_only";
        core::Logger::Instance().Log(core::LogLevel::Warning, frame.frame_id, "dx12.debug_upscale", message.str());
        return false;
    }

    auto* command_list = static_cast<ID3D12GraphicsCommandList*>(native_command_list);
    auto* input = static_cast<ID3D12Resource*>(frame.color_input.native_resource);
    auto* output = static_cast<ID3D12Resource*>(frame.color_output.native_resource);
    if (!input || !output) {
        message << " mode=missing_resources";
        core::Logger::Instance().Log(core::LogLevel::Warning, frame.frame_id, "dx12.debug_upscale", message.str());
        return false;
    }

    const bool copy_compatible = frame.render_size.width == frame.display_size.width &&
                                 frame.render_size.height == frame.display_size.height &&
                                 frame.color_input.api_format == frame.color_output.api_format;
    if (copy_compatible) {
        D3D12_RESOURCE_BARRIER barriers[2] {};
        barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[0].Transition.pResource = input;
        barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
        barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barriers[1].Transition.pResource = output;
        barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
        barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
        command_list->ResourceBarrier(2, barriers);
        command_list->CopyResource(output, input);
        std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
        std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
        command_list->ResourceBarrier(2, barriers);
        message << " mode=dx12_copy_recorded";
        core::Logger::Instance().Log(core::LogLevel::Info, frame.frame_id, "dx12.debug_upscale", message.str());
        return true;
    }

    D3D12_CPU_DESCRIPTOR_HANDLE srv_handle = descriptor_heap_->GetCPUDescriptorHandleForHeapStart();
    D3D12_CPU_DESCRIPTOR_HANDLE uav_handle = srv_handle;
    uav_handle.ptr += descriptor_size_;

    D3D12_SHADER_RESOURCE_VIEW_DESC srv {};
    srv.Format = static_cast<DXGI_FORMAT>(frame.color_input.api_format);
    srv.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Texture2D.MipLevels = 1;
    native_device_->CreateShaderResourceView(input, &srv, srv_handle);

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav {};
    uav.Format = static_cast<DXGI_FORMAT>(frame.color_output.api_format);
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    native_device_->CreateUnorderedAccessView(output, nullptr, &uav, uav_handle);

    D3D12_RESOURCE_BARRIER to_uav {};
    to_uav.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    to_uav.Transition.pResource = output;
    to_uav.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    to_uav.Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    to_uav.Transition.StateAfter = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    command_list->ResourceBarrier(1, &to_uav);

    ID3D12DescriptorHeap* heaps[] = {descriptor_heap_};
    command_list->SetDescriptorHeaps(1, heaps);
    command_list->SetComputeRootSignature(root_signature_);
    command_list->SetPipelineState(pipeline_state_);
    command_list->SetComputeRootDescriptorTable(0, descriptor_heap_->GetGPUDescriptorHandleForHeapStart());
    const uint32_t constants[4] = {
        frame.render_size.width,
        frame.render_size.height,
        frame.display_size.width,
        frame.display_size.height
    };
    command_list->SetComputeRoot32BitConstants(1, 4, constants, 0);
    command_list->Dispatch(dispatch.groups_x, dispatch.groups_y, 1);

    D3D12_RESOURCE_BARRIER barriers[2] {};
    barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
    barriers[0].UAV.pResource = output;
    barriers[1] = to_uav;
    barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_UNORDERED_ACCESS;
    barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
    command_list->ResourceBarrier(2, barriers);

    message << " mode=dx12_compute_upscale_recorded";
    core::Logger::Instance().Log(core::LogLevel::Info, frame.frame_id, "dx12.debug_upscale", message.str());
    return true;
}

} // namespace osr::backends::dx12
