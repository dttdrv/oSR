#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "backends/dx12/dx12_backend.h"
#include "core/frame_context.h"
#include "core/logging.h"
#include "debug/capture_pack.h"
#include "debug/validation.h"
#include "demo/dx12_wind_tunnel/dx12_texture_io.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
    osr::demo::dx12_wind_tunnel::Dx12Sync sync;
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
    osr::demo::dx12_wind_tunnel::ReleaseSync(dx.sync);
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
    if (Failed(dx.command_list->Close(), "Close initial command list")) {
        return false;
    }
    return osr::demo::dx12_wind_tunnel::InitializeSync(dx.device, dx.sync);
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
                                                       D3D12_RESOURCE_STATE_COPY_DEST,
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
                    const std::vector<osr::demo::dx12_wind_tunnel::TextureTransferResult>& transfers,
                    const std::filesystem::path& capture_path,
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
    out << "capture_pack: " << capture_path.string() << "\n";
    out << "transfer_match: " << (osr::demo::dx12_wind_tunnel::AllTransfersMatched(transfers) ? "true" : "false") << "\n";
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
    out << "transfers:\n";
    for (const auto& transfer : transfers) {
        out << "  - " << transfer.name
            << " format=" << static_cast<uint32_t>(transfer.format)
            << " extent=" << transfer.extent.width << "x" << transfer.extent.height
            << " row_size=" << transfer.row_size_bytes
            << " row_pitch=" << transfer.row_pitch
            << " total_bytes=" << transfer.total_bytes
            << " cpu_hash=" << transfer.cpu_hash
            << " gpu_hash=" << transfer.gpu_hash
            << " matched=" << (transfer.matched ? "true" : "false")
            << "\n";
    }
}

void CountValidation(const osr::core::ValidationReport& report, uint32_t& errors, uint32_t& warnings) {
    errors = 0;
    warnings = 0;
    for (const auto& message : report.messages) {
        if (message.severity == osr::core::ValidationSeverity::Error) {
            ++errors;
        } else if (message.severity == osr::core::ValidationSeverity::Warning) {
            ++warnings;
        }
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

    std::vector<osr::demo::dx12_wind_tunnel::TextureTransferResult> transfers;
    auto upload = [&](ID3D12Resource* resource,
                      DXGI_FORMAT format,
                      osr::core::Dimensions size,
                      const void* data,
                      uint64_t row_bytes,
                      const char* name) {
        osr::demo::dx12_wind_tunnel::TextureTransferResult result;
        const bool transfer_ok = osr::demo::dx12_wind_tunnel::UploadReadbackTexture2D(dx.device,
                                                                                      dx.queue,
                                                                                      dx.allocator,
                                                                                      dx.command_list,
                                                                                      dx.sync,
                                                                                      resource,
                                                                                      format,
                                                                                      size,
                                                                                      data,
                                                                                      row_bytes,
                                                                                      name,
                                                                                      result);
        transfers.push_back(result);
        return transfer_ok && result.matched;
    };

    const bool transfer_ok =
        upload(dx.color_input, DXGI_FORMAT_R8G8B8A8_UNORM, render_size, synthetic.color.data(), static_cast<uint64_t>(render_size.width) * sizeof(uint32_t), "color_input") &&
        upload(dx.depth, DXGI_FORMAT_R32_FLOAT, render_size, synthetic.depth.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "depth") &&
        upload(dx.motion_vectors, DXGI_FORMAT_R32G32_FLOAT, render_size, synthetic.motion_vectors.data(), static_cast<uint64_t>(render_size.width) * sizeof(osr::demo::wind_tunnel::Float2Buffer), "motion_vectors") &&
        upload(dx.reactive_mask, DXGI_FORMAT_R32_FLOAT, render_size, synthetic.reactive_mask.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "reactive_mask");
    synthetic.context.notes.push_back(transfer_ok ? "D3D12 upload/readback hashes matched CPU buffers." : "D3D12 upload/readback hash mismatch detected.");

    const auto report = osr::core::ValidateFrameContext(synthetic.context);
    osr::backends::dx12::Dx12Backend backend;
    backend.Initialize(dx.device);
    const bool dispatch_result = backend.DispatchDebugUpscale(dx.command_list, synthetic.context);

    osr::debug::CapturePackConfig capture_config;
    capture_config.root = "build/manual/captures";
    capture_config.scenario = "dx12_wind_tunnel";
    capture_config.mode = "h1_buffer_truth";
    capture_config.algorithm = "debug_upscale";
    osr::debug::CapturePackWriter capture;
    const bool capture_started = capture.BeginSession(capture_config);
    if (capture_started) {
        capture.WriteSessionManifest(synthetic.context, GetCommandLineA());
        uint32_t validation_errors = 0;
        uint32_t validation_warnings = 0;
        CountValidation(report, validation_errors, validation_warnings);
        osr::debug::HarnessFrameRow row;
        row.frame_id = synthetic.context.frame_id;
        row.render_size = synthetic.context.render_size;
        row.display_size = synthetic.context.display_size;
        row.jitter = synthetic.context.jitter_offset;
        row.motion_vector_scale = synthetic.context.motion_vector_scale;
        row.reset_history = synthetic.context.flags.reset_history;
        row.validation_errors = validation_errors;
        row.validation_warnings = validation_warnings;
        row.gpu_upload_ms = 0.0;
        row.gpu_reconstruct_ms = dispatch_result ? 0.0 : -1.0;
        capture.WriteFrameRow(row);
        osr::debug::HarnessMetricRow metrics;
        metrics.frame_id = synthetic.context.frame_id;
        capture.WriteMetricRow(metrics);
        capture.WriteValidationWarnings(synthetic.context.frame_id, report);
        capture.WriteFrameContextJson(synthetic.context);
    }

    ExportMetadata(synthetic.context, report, dispatch_result, transfers, capture.SessionPath(), "build/manual/osr_dx12_wind_tunnel_metadata.txt");

    std::cout << "oSR DX12 wind tunnel proof of life\n";
    std::cout << "Render: " << render_size.width << "x" << render_size.height
              << "  Display: " << display_size.width << "x" << display_size.height << "\n";
    std::cout << "Validation: " << osr::debug::SummarizeValidation(report) << "\n";
    std::cout << "Metadata: build/manual/osr_dx12_wind_tunnel_metadata.txt\n";
    std::cout << "Capture: " << capture.SessionPath().string() << "\n";
    std::cout << "Transfer hashes: " << (transfer_ok ? "matched" : "FAILED") << "\n";
    std::cout << "Log: build/manual/osr_dx12_wind_tunnel.log\n";

    const bool has_errors = report.HasErrors() || !transfer_ok || !capture_started;
    Release(dx);
    return has_errors ? 1 : 0;
}
