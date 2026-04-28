#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "backends/dx12/dx12_backend.h"
#include "core/frame_context.h"
#include "core/logging.h"
#include "debug/validation.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

namespace {

template <typename T>
void SafeRelease(T*& value) {
    if (value) {
        value->Release();
        value = nullptr;
    }
}

struct Dx12Objects {
    ID3D12Device* device = nullptr;
    ID3D12CommandQueue* queue = nullptr;
    ID3D12CommandAllocator* allocator = nullptr;
    ID3D12GraphicsCommandList* command_list = nullptr;
    ID3D12Resource* color_input = nullptr;
    ID3D12Resource* color_output = nullptr;
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* motion_vectors = nullptr;
    ID3D12Resource* reactive_mask = nullptr;
};

void Release(Dx12Objects& dx) {
    SafeRelease(dx.reactive_mask);
    SafeRelease(dx.motion_vectors);
    SafeRelease(dx.depth);
    SafeRelease(dx.color_output);
    SafeRelease(dx.color_input);
    SafeRelease(dx.command_list);
    SafeRelease(dx.allocator);
    SafeRelease(dx.queue);
    SafeRelease(dx.device);
}

bool Failed(HRESULT hr, const char* what) {
    if (SUCCEEDED(hr)) {
        return false;
    }
    std::cerr << what << " failed, HRESULT=0x" << std::hex << static_cast<unsigned long>(hr) << std::dec << "\n";
    return true;
}

bool CreateDeviceObjects(Dx12Objects& dx) {
    if (Failed(D3D12CreateDevice(nullptr, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&dx.device)), "D3D12CreateDevice")) {
        return false;
    }

    D3D12_COMMAND_QUEUE_DESC queue_desc {};
    queue_desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    if (Failed(dx.device->CreateCommandQueue(&queue_desc, IID_PPV_ARGS(&dx.queue)), "CreateCommandQueue")) {
        return false;
    }
    if (Failed(dx.device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&dx.allocator)), "CreateCommandAllocator")) {
        return false;
    }
    if (Failed(dx.device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, dx.allocator, nullptr, IID_PPV_ARGS(&dx.command_list)), "CreateCommandList")) {
        return false;
    }
    return true;
}

bool CreateTexture(ID3D12Device* device,
                   osr::core::Dimensions size,
                   DXGI_FORMAT format,
                   D3D12_RESOURCE_FLAGS flags,
                   ID3D12Resource** out_resource,
                   const wchar_t* name) {
    D3D12_HEAP_PROPERTIES heap {};
    heap.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC desc {};
    desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    desc.Alignment = 0;
    desc.Width = size.width;
    desc.Height = size.height;
    desc.DepthOrArraySize = 1;
    desc.MipLevels = 1;
    desc.Format = format;
    desc.SampleDesc.Count = 1;
    desc.SampleDesc.Quality = 0;
    desc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    desc.Flags = flags;

    const HRESULT hr = device->CreateCommittedResource(&heap,
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &desc,
                                                       D3D12_RESOURCE_STATE_COMMON,
                                                       nullptr,
                                                       IID_PPV_ARGS(out_resource));
    if (FAILED(hr)) {
        return false;
    }
    (*out_resource)->SetName(name);
    return true;
}

osr::core::ResourceDesc D3DResource(osr::core::ResourceKind kind,
                                    ID3D12Resource* resource,
                                    uint64_t debug_id,
                                    osr::core::Dimensions extent,
                                    DXGI_FORMAT format,
                                    const char* name) {
    return {kind, resource, debug_id, extent, static_cast<uint32_t>(format), name};
}

void ExportMetadata(const osr::core::FrameContext& frame,
                    const osr::core::ValidationReport& report,
                    bool dispatch_result,
                    const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::trunc);
    out << "oSR DX12 wind tunnel proof of life\n";
    out << "frame_id: " << frame.frame_id << "\n";
    out << "source_api: " << frame.source_api << "\n";
    out << "render_size: " << frame.render_size.width << "x" << frame.render_size.height << "\n";
    out << "display_size: " << frame.display_size.width << "x" << frame.display_size.height << "\n";
    out << "jitter: " << frame.jitter_offset.x << ", " << frame.jitter_offset.y << "\n";
    out << "motion_vector_scale: " << frame.motion_vector_scale.x << ", " << frame.motion_vector_scale.y << "\n";
    out << "motion_vector_space: " << osr::core::ToString(frame.motion_vector_space) << "\n";
    out << "reset_history: " << (frame.flags.reset_history ? "true" : "false") << "\n";
    out << "reactive_mask: " << (frame.reactive_mask.has_value() ? "present" : "absent") << "\n";
    out << "validation: " << osr::debug::SummarizeValidation(report) << "\n";
    out << "debug_dispatch_result: " << (dispatch_result ? "recorded_or_ready" : "metadata_only_or_pending") << "\n";
    out << "resources:\n";
    out << "  color_input: " << frame.color_input.debug_name << " native=" << frame.color_input.native_resource << "\n";
    out << "  color_output: " << frame.color_output.debug_name << " native=" << frame.color_output.native_resource << "\n";
    out << "  depth: " << frame.depth.debug_name << " native=" << frame.depth.native_resource << "\n";
    out << "  motion_vectors: " << frame.motion_vectors.debug_name << " native=" << frame.motion_vectors.native_resource << "\n";
    if (frame.reactive_mask) {
        out << "  reactive_mask: " << frame.reactive_mask->debug_name << " native=" << frame.reactive_mask->native_resource << "\n";
    }
    out << "validation_messages:\n";
    for (const auto& message : report.messages) {
        out << "  - " << message.code << ": " << message.message << "\n";
    }
}

} // namespace

int main() {
    std::filesystem::create_directories("build/manual");
    osr::core::Logger::Instance().Configure("build/manual/osr_dx12_wind_tunnel.log", osr::core::LogLevel::Debug);

    Dx12Objects dx;
    if (!CreateDeviceObjects(dx)) {
        Release(dx);
        std::cout << "DX12 initialization failed. See console output.\n";
        return 1;
    }

    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = {1280, 800};
    settings.render_scale = 2.0f / 3.0f;
    settings.frame_id = 1;
    settings.reset_history = true;
    auto synthetic = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);

    const auto render_size = synthetic.context.render_size;
    const auto display_size = synthetic.context.display_size;
    bool ok = true;
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.color_input, L"oSR synthetic color input");
    ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.color_output, L"oSR synthetic color output");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.depth, L"oSR synthetic depth");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.motion_vectors, L"oSR synthetic motion vectors");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.reactive_mask, L"oSR synthetic reactive mask");
    if (!ok) {
        std::cerr << "Failed to create one or more D3D12 textures.\n";
        Release(dx);
        return 1;
    }

    synthetic.context.source_api = "oSR.dx12_wind_tunnel";
    synthetic.context.color_input = D3DResource(osr::core::ResourceKind::ColorInput, dx.color_input, 0x2000, render_size, DXGI_FORMAT_R8G8B8A8_UNORM, "dx12_color_input_rgba8");
    synthetic.context.color_output = D3DResource(osr::core::ResourceKind::ColorOutput, dx.color_output, 0x2001, display_size, DXGI_FORMAT_R8G8B8A8_UNORM, "dx12_color_output_rgba8");
    synthetic.context.depth = D3DResource(osr::core::ResourceKind::Depth, dx.depth, 0x2002, render_size, DXGI_FORMAT_R32_FLOAT, "dx12_depth_r32f");
    synthetic.context.motion_vectors = D3DResource(osr::core::ResourceKind::MotionVectors, dx.motion_vectors, 0x2003, render_size, DXGI_FORMAT_R32G32_FLOAT, "dx12_motion_vectors_r32g32f");
    synthetic.context.reactive_mask = D3DResource(osr::core::ResourceKind::ReactiveMask, dx.reactive_mask, 0x2004, render_size, DXGI_FORMAT_R32_FLOAT, "dx12_reactive_mask_r32f");
    synthetic.context.notes.push_back("D3D12 resources are allocated; CPU upload/present is intentionally a later step.");

    const auto report = osr::core::ValidateFrameContext(synthetic.context);
    osr::backends::dx12::Dx12Backend backend;
    backend.Initialize(dx.device);
    const bool dispatch_result = backend.DispatchDebugUpscale(dx.command_list, synthetic.context);
    ExportMetadata(synthetic.context, report, dispatch_result, "build/manual/osr_dx12_wind_tunnel_metadata.txt");

    std::cout << "oSR DX12 wind tunnel proof of life\n";
    std::cout << "Render: " << render_size.width << "x" << render_size.height
              << "  Display: " << display_size.width << "x" << display_size.height << "\n";
    std::cout << "Validation: " << osr::debug::SummarizeValidation(report) << "\n";
    std::cout << "Metadata: build/manual/osr_dx12_wind_tunnel_metadata.txt\n";
    std::cout << "Log: build/manual/osr_dx12_wind_tunnel.log\n";

    const bool has_errors = report.HasErrors();
    Release(dx);
    return has_errors ? 1 : 0;
}
