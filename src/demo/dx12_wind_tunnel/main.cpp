#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "backends/dx12/dx12_backend.h"
#include "core/frame_context.h"
#include "core/logging.h"
#include "debug/capture_pack.h"
#include "debug/validation.h"
#include "demo/dx12_wind_tunnel/display_upscale.h"
#include "demo/dx12_wind_tunnel/dx12_texture_io.h"
#include "demo/dx12_wind_tunnel/presenter.h"
#include "demo/wind_tunnel/debug_dumps.h"
#include "demo/wind_tunnel/synthetic_frame.h"
#include "demo/wind_tunnel/temporal_diagnostics.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstdlib>
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
                    const osr::demo::wind_tunnel::TemporalDiagnostics& diagnostics,
                    const osr::demo::wind_tunnel::TemporalDiagnosticVerdict& verdict,
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
    out << "motion_vectors_jittered: " << (frame.flags.motion_vectors_jittered ? "true" : "false") << "\n";
    out << "reset_history: " << (frame.flags.reset_history ? "true" : "false") << "\n";
    out << "reactive_mask: " << (frame.reactive_mask.has_value() ? "present" : "absent") << "\n";
    out << "validation: " << osr::debug::SummarizeValidation(report) << "\n";
    out << "debug_dispatch_result: " << (dispatch_result ? "recorded_and_executed" : "metadata_only_or_pending") << "\n";
    out << "capture_pack: " << capture_path.string() << "\n";
    out << "transfer_match: " << (osr::demo::dx12_wind_tunnel::AllTransfersMatched(transfers) ? "true" : "false") << "\n";
    out << "temporal_diagnostics:\n";
    out << "  samples: " << diagnostics.sample_count << "\n";
    out << "  mv_luma_residual_mean: " << diagnostics.mv_luma_residual_mean << "\n";
    out << "  mv_luma_residual_p95: " << diagnostics.mv_luma_residual_p95 << "\n";
    out << "  mv_depth_residual_mean: " << diagnostics.mv_depth_residual_mean << "\n";
    out << "  mv_depth_residual_p95: " << diagnostics.mv_depth_residual_p95 << "\n";
    out << "  bad_history_trusted_pct: " << diagnostics.bad_history_trusted_pct << "\n";
    out << "  good_history_rejected_pct: " << diagnostics.good_history_rejected_pct << "\n";
    out << "  reactive_history_trusted_pct: " << diagnostics.reactive_history_trusted_pct << "\n";
    out << "  disocclusion_history_trusted_pct: " << diagnostics.disocclusion_history_trusted_pct << "\n";
    out << "  trust_evidence_agreement_pct: " << diagnostics.trust_evidence_agreement_pct << "\n";
    out << "  history_trust_mean: " << diagnostics.history_trust_mean << "\n";
    out << "  accumulation_weight_mean: " << diagnostics.accumulation_weight_mean << "\n";
    out << "diagnostic_verdict:\n";
    out << "  max_severity: " << verdict.max_severity << "\n";
    out << "  metric_gate_failed: " << (verdict.metric_gate_failed ? "true" : "false") << "\n";
    for (const auto& finding : verdict.findings) {
        out << "  - " << finding.tag
            << " severity=" << finding.severity
            << " cause=" << finding.likely_cause
            << " evidence=" << finding.evidence_value
            << "\n";
    }
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

bool ParseMotionVectorMode(const std::string& value, osr::demo::wind_tunnel::MotionVectorMode& mode) {
    if (value == "correct") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::Correct;
    } else if (value == "zero") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::Zero;
    } else if (value == "flip-x") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::FlipX;
    } else if (value == "flip-y") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::FlipY;
    } else if (value == "half-scale") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::HalfScale;
    } else if (value == "double-scale") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::DoubleScale;
    } else if (value == "jitter-contaminated" || value == "jitter") {
        mode = osr::demo::wind_tunnel::MotionVectorMode::JitterContaminated;
    } else {
        return false;
    }
    return true;
}

bool ParseDimensions(const std::string& value, osr::core::Dimensions& dimensions) {
    const auto x = value.find('x');
    if (x == std::string::npos || x == 0 || x + 1 >= value.size()) {
        return false;
    }
    const int width = std::atoi(value.substr(0, x).c_str());
    const int height = std::atoi(value.substr(x + 1).c_str());
    if (width <= 0 || height <= 0) {
        return false;
    }
    dimensions = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    return true;
}

} // namespace

int main(int argc, char** argv) {
    bool headless = false;
    int present_frames = -1;
    bool metric_gate = false;
    osr::core::Dimensions requested_display_size {1280, 800};
    float requested_render_scale = 2.0f / 3.0f;
    auto mv_mode = osr::demo::wind_tunnel::MotionVectorMode::Correct;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--headless") {
            headless = true;
        } else if (std::string(argv[i]) == "--metric-gate" || std::string(argv[i]) == "--fail-on-diagnostics") {
            metric_gate = true;
        } else if (std::string(argv[i]) == "--present-frames" && i + 1 < argc) {
            present_frames = std::max(0, std::atoi(argv[++i]));
        } else if (std::string(argv[i]) == "--display-size" && i + 1 < argc) {
            if (!ParseDimensions(argv[++i], requested_display_size)) {
                std::cerr << "Unknown --display-size. Use WIDTHxHEIGHT, for example 1280x800.\n";
                return 2;
            }
        } else if (std::string(argv[i]) == "--render-scale" && i + 1 < argc) {
            requested_render_scale = static_cast<float>(std::atof(argv[++i]));
        } else if (std::string(argv[i]) == "--mv-mode" && i + 1 < argc) {
            if (!ParseMotionVectorMode(argv[++i], mv_mode)) {
                std::cerr << "Unknown --mv-mode. Use correct, zero, flip-x, flip-y, half-scale, double-scale, or jitter-contaminated.\n";
                return 2;
            }
        }
    }

    std::filesystem::create_directories("build/manual");
    osr::core::Logger::Instance().Configure("build/manual/osr_dx12_wind_tunnel.log", osr::core::LogLevel::Debug);

    Dx12Objects dx;
    if (!CreateDeviceObjects(dx)) {
        Release(dx);
        std::cout << "DX12 initialization failed. See console output.\n";
        return 1;
    }

    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = requested_display_size;
    settings.render_scale = requested_render_scale;
    settings.frame_id = 1;
    settings.reset_history = true;
    settings.motion_vector_mode = mv_mode;
    auto synthetic = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    synthetic.context.notes.push_back("DX12 debug upscale uses heartbeat command recording for scaled output until compute shader bytecode is embedded.");
    auto previous_settings = settings;
    previous_settings.frame_id = settings.frame_id > 0 ? settings.frame_id - 1 : 0;
    previous_settings.reset_history = false;
    previous_settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::Correct;
    const auto previous_synthetic = osr::demo::wind_tunnel::BuildSyntheticFrame(previous_settings);
    const auto temporal_diagnostics = osr::demo::wind_tunnel::ComputeTemporalDiagnostics(previous_synthetic, synthetic);
    const auto temporal_verdict = osr::demo::wind_tunnel::AnalyzeTemporalDiagnostics(temporal_diagnostics, mv_mode, metric_gate);

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

    const auto display_output = osr::demo::dx12_wind_tunnel::UpscaleNearest(synthetic.color, render_size, display_size);

    const bool transfer_ok =
        upload(dx.color_input, DXGI_FORMAT_R8G8B8A8_UNORM, render_size, synthetic.color.data(), static_cast<uint64_t>(render_size.width) * sizeof(uint32_t), "color_input") &&
        upload(dx.color_output, DXGI_FORMAT_R8G8B8A8_UNORM, display_size, display_output.data(), static_cast<uint64_t>(display_size.width) * sizeof(uint32_t), "color_output") &&
        upload(dx.depth, DXGI_FORMAT_R32_FLOAT, render_size, synthetic.depth.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "depth") &&
        upload(dx.motion_vectors, DXGI_FORMAT_R32G32_FLOAT, render_size, synthetic.motion_vectors.data(), static_cast<uint64_t>(render_size.width) * sizeof(osr::demo::wind_tunnel::Float2Buffer), "motion_vectors") &&
        upload(dx.reactive_mask, DXGI_FORMAT_R32_FLOAT, render_size, synthetic.reactive_mask.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "reactive_mask");
    synthetic.context.notes.push_back(transfer_ok ? "D3D12 upload/readback hashes matched CPU buffers." : "D3D12 upload/readback hash mismatch detected.");

    const auto report = osr::core::ValidateFrameContext(synthetic.context);
    osr::backends::dx12::Dx12Backend backend;
    const bool backend_initialized = backend.Initialize(dx.device);
    bool dispatch_result = false;
    if (backend_initialized &&
        SUCCEEDED(dx.allocator->Reset()) &&
        SUCCEEDED(dx.command_list->Reset(dx.allocator, nullptr))) {
        dispatch_result = backend.DispatchDebugUpscale(dx.command_list, synthetic.context) &&
                          osr::demo::dx12_wind_tunnel::ExecuteAndWait(dx.queue, dx.command_list, dx.sync);
    }
    synthetic.context.notes.push_back(dispatch_result ? "D3D12 debug upscale command list executed." : "D3D12 debug upscale command list did not execute.");
    osr::demo::dx12_wind_tunnel::TextureTransferResult reconstructed_output;
    const bool reconstructed_output_readback = dispatch_result &&
        osr::demo::dx12_wind_tunnel::ReadbackTexture2D(dx.device,
                                                       dx.queue,
                                                       dx.allocator,
                                                       dx.command_list,
                                                       dx.sync,
                                                       dx.color_output,
                                                       DXGI_FORMAT_R8G8B8A8_UNORM,
                                                       display_size,
                                                       display_output.data(),
                                                       static_cast<uint64_t>(display_size.width) * sizeof(uint32_t),
                                                       "color_output_after_dispatch",
                                                       reconstructed_output);
    const bool reconstructed_output_match = reconstructed_output_readback && reconstructed_output.matched;
    if (!reconstructed_output.name.empty()) {
        transfers.push_back(reconstructed_output);
    }
    synthetic.context.notes.push_back(reconstructed_output_match ? "D3D12 debug upscale output matched CPU nearest reference." : "D3D12 debug upscale output did not match CPU nearest reference.");

    osr::debug::CapturePackConfig capture_config;
    capture_config.root = "build/manual/captures";
    capture_config.scenario = "dx12_wind_tunnel";
    capture_config.mode = std::string("h1_buffer_truth_") + osr::demo::wind_tunnel::ToString(mv_mode);
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
        metrics.mv_luma_residual_mean = temporal_diagnostics.mv_luma_residual_mean;
        metrics.mv_luma_residual_p95 = temporal_diagnostics.mv_luma_residual_p95;
        metrics.mv_depth_residual_mean = temporal_diagnostics.mv_depth_residual_mean;
        metrics.mv_depth_residual_p95 = temporal_diagnostics.mv_depth_residual_p95;
        metrics.bad_history_trusted_pct = temporal_diagnostics.bad_history_trusted_pct;
        metrics.good_history_rejected_pct = temporal_diagnostics.good_history_rejected_pct;
        metrics.reactive_history_trusted_pct = temporal_diagnostics.reactive_history_trusted_pct;
        metrics.disocclusion_history_trusted_pct = temporal_diagnostics.disocclusion_history_trusted_pct;
        metrics.trust_evidence_agreement_pct = temporal_diagnostics.trust_evidence_agreement_pct;
        metrics.history_trust_mean = temporal_diagnostics.history_trust_mean;
        metrics.accumulation_weight_mean = temporal_diagnostics.accumulation_weight_mean;
        capture.WriteMetricRow(metrics);
        capture.WriteValidationWarnings(synthetic.context.frame_id, report);
        for (const auto& finding : temporal_verdict.findings) {
            capture.WriteDiagnosticWarning(synthetic.context.frame_id,
                                           finding.tag,
                                           finding.likely_cause,
                                           finding.suggested_action,
                                           finding.severity,
                                           finding.evidence_value);
        }
        capture.WriteFrameContextJson(synthetic.context);
        std::ostringstream frame_dir_name;
        frame_dir_name << "frame_" << std::setw(6) << std::setfill('0') << synthetic.context.frame_id;
        uint64_t color_input_hash = 0;
        uint64_t color_output_hash = 0;
        uint64_t depth_hash = 0;
        uint64_t motion_vectors_hash = 0;
        uint64_t reactive_mask_hash = 0;
        for (const auto& transfer : transfers) {
            if (transfer.name == "color_input") color_input_hash = transfer.gpu_hash;
            if (transfer.name == "color_output") color_output_hash = transfer.gpu_hash;
            if (transfer.name == "depth") depth_hash = transfer.gpu_hash;
            if (transfer.name == "motion_vectors") motion_vectors_hash = transfer.gpu_hash;
            if (transfer.name == "reactive_mask") reactive_mask_hash = transfer.gpu_hash;
        }
        const auto dump_result = osr::demo::wind_tunnel::WriteSyntheticFrameDebugDumps(capture.SessionPath() / frame_dir_name.str(),
                                                                                       synthetic,
                                                                                       display_output,
                                                                                       color_input_hash,
                                                                                       color_output_hash,
                                                                                       depth_hash,
                                                                                       motion_vectors_hash,
                                                                                       reactive_mask_hash);
        if (!dump_result.AllRequired()) {
            osr::core::ValidationReport dump_report;
            dump_report.messages.push_back({
                osr::core::ValidationSeverity::Warning,
                "debug_dump_write_failed",
                "One or more raw/debug image files were not written."
            });
            capture.WriteValidationWarnings(synthetic.context.frame_id, dump_report);
        }
    }

    ExportMetadata(synthetic.context, report, dispatch_result, temporal_diagnostics, temporal_verdict, transfers, capture.SessionPath(), "build/manual/osr_dx12_wind_tunnel_metadata.txt");

    std::cout << "oSR DX12 wind tunnel proof of life\n";
    std::cout << "Render: " << render_size.width << "x" << render_size.height
              << "  Display: " << display_size.width << "x" << display_size.height << "\n";
    std::cout << "MV mode: " << osr::demo::wind_tunnel::ToString(mv_mode) << "\n";
    std::cout << "Validation: " << osr::debug::SummarizeValidation(report) << "\n";
    std::cout << "Temporal diagnostics: luma_mean=" << temporal_diagnostics.mv_luma_residual_mean
              << " bad_trusted=" << temporal_diagnostics.bad_history_trusted_pct
              << "% agreement=" << temporal_diagnostics.trust_evidence_agreement_pct << "%\n";
    std::cout << "Diagnostic verdict: severity=" << temporal_verdict.max_severity
              << " findings=" << temporal_verdict.findings.size()
              << (temporal_verdict.metric_gate_failed ? " metric_gate=FAILED" : " metric_gate=ok") << "\n";
    std::cout << "Metadata: build/manual/osr_dx12_wind_tunnel_metadata.txt\n";
    std::cout << "Capture: " << capture.SessionPath().string() << "\n";
    std::cout << "Transfer hashes: " << (transfer_ok ? "matched" : "FAILED") << "\n";
    std::cout << "Reconstruct output hash: " << (reconstructed_output_match ? "matched" : "FAILED") << "\n";
    std::cout << "Log: build/manual/osr_dx12_wind_tunnel.log\n";

    osr::demo::dx12_wind_tunnel::PresentState present;
    bool present_ok = true;
    if (!headless) {
        present_ok = osr::demo::dx12_wind_tunnel::CreatePresentState(GetModuleHandle(nullptr), dx.device, dx.queue, display_size, present);
        if (present_ok) {
            std::cout << "DX12 window open. Close it to exit.\n";
            int frames_presented = 0;
            while (osr::demo::dx12_wind_tunnel::PumpWindowMessages(present)) {
                present_ok = osr::demo::dx12_wind_tunnel::PresentOutputTexture(dx.queue, dx.allocator, dx.command_list, dx.sync, dx.color_output, present);
                if (!present_ok) {
                    break;
                }
                ++frames_presented;
                if (present_frames >= 0 && frames_presented >= present_frames) {
                    break;
                }
                Sleep(16);
            }
        } else {
            std::cerr << "Failed to create DX12 presentation window.\n";
        }
    }

    const bool has_errors = report.HasErrors() || !transfer_ok || !reconstructed_output_match || !capture_started || !present_ok;
    osr::demo::dx12_wind_tunnel::ReleasePresentState(present);
    Release(dx);
    if (!has_errors && temporal_verdict.metric_gate_failed) {
        return 3;
    }
    return has_errors ? 1 : 0;
}
