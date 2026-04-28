#include "backends/dx12/debug_upscale_pass.h"

#include "backends/dx12/descriptor_helpers.h"
#include "core/logging.h"

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
    SafeRelease(heartbeat_texture_);
    SafeRelease(heartbeat_uav_heap_);
    SafeRelease(native_device_);
}

bool DebugUpscalePass::Initialize(void* native_device) {
    Shutdown();
    native_device_ = static_cast<ID3D12Device*>(native_device);
    if (!native_device_) {
        core::Logger::Instance().Log(core::LogLevel::Warning, 0, "dx12.debug_upscale", "Initialized debug upscale pass in metadata-only mode.");
        return true;
    }
    native_device_->AddRef();

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc {};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 1;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (Failed(native_device_->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&heartbeat_uav_heap_)))) {
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to create debug heartbeat UAV heap.");
        return false;
    }

    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Width = 1;
    desc.Height = 1;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = DXGI_FORMAT_R32_UINT;
    desc.SampleDesc.Count = 1;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    if (Failed(native_device_->CreateCommittedResource(&heap,
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &desc,
                                                       D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
                                                       nullptr,
                                                       IID_PPV_ARGS(&heartbeat_texture_)))) {
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "dx12.debug_upscale", "Failed to create debug heartbeat texture.");
        return false;
    }
    heartbeat_texture_->SetName(L"oSR debug upscale heartbeat");

    D3D12_UNORDERED_ACCESS_VIEW_DESC uav {};
    uav.Format = DXGI_FORMAT_R32_UINT;
    uav.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    native_device_->CreateUnorderedAccessView(heartbeat_texture_, nullptr, &uav, heartbeat_uav_heap_->GetCPUDescriptorHandleForHeapStart());

    core::Logger::Instance().Log(core::LogLevel::Info, 0, "dx12.debug_upscale", "Initialized debug upscale pass with command-list heartbeat UAV.");
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

    if (native_device_ == nullptr || native_command_list == nullptr || heartbeat_uav_heap_ == nullptr || heartbeat_texture_ == nullptr) {
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

    ID3D12DescriptorHeap* heaps[] = {heartbeat_uav_heap_};
    command_list->SetDescriptorHeaps(1, heaps);
    const uint32_t values[4] = {
        static_cast<uint32_t>(frame.frame_id & 0xffffffffu),
        frame.display_size.width,
        frame.display_size.height,
        (dispatch.groups_x & 0xffffu) | ((dispatch.groups_y & 0xffffu) << 16)
    };
    D3D12_RECT rect {0, 0, 1, 1};
    command_list->ClearUnorderedAccessViewUint(heartbeat_uav_heap_->GetGPUDescriptorHandleForHeapStart(),
                                               heartbeat_uav_heap_->GetCPUDescriptorHandleForHeapStart(),
                                               heartbeat_texture_,
                                               values,
                                               1,
                                               &rect);

    message << " mode=dx12_command_recorded_heartbeat_clear upscale_pending";
    core::Logger::Instance().Log(core::LogLevel::Info, frame.frame_id, "dx12.debug_upscale", message.str());
    return true;
}

} // namespace osr::backends::dx12
