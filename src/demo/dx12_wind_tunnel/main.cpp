#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>

#include "backends/dx12/dx12_backend.h"
#include "backends/dx12/temporal_resolve_pass.h"
#include "core/frame_context.h"
#include "core/logging.h"
#include "debug/capture_pack.h"
#include "debug/validation.h"
#include "demo/dx12_wind_tunnel/display_upscale.h"
#include "demo/dx12_wind_tunnel/dx12_texture_io.h"
#include "demo/dx12_wind_tunnel/presenter.h"
#include "demo/wind_tunnel/debug_dumps.h"
#include "demo/wind_tunnel/sequence_metrics.h"
#include "demo/wind_tunnel/synthetic_frame.h"
#include "demo/wind_tunnel/temporal_diagnostics.h"
#include "demo/wind_tunnel/temporal_resolve.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstring>
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
    ID3D12Resource* previous_history = nullptr;
    ID3D12Resource* depth = nullptr;
    ID3D12Resource* previous_depth = nullptr;
    ID3D12Resource* motion_vectors = nullptr;
    ID3D12Resource* reactive_mask = nullptr;
    ID3D12Resource* debug_history_weight = nullptr;
    ID3D12Resource* debug_color_residual = nullptr;
    ID3D12Resource* debug_depth_residual = nullptr;
};

struct FloatMapDiff {
    double max_abs = 0.0;
    double mean_abs = 0.0;
};

enum class ReconstructionMode {
    SpatialGpu,
    TemporalCpu,
    TemporalGpu
};

void Release(Dx12Objects& dx) {
    SafeRelease(dx.debug_depth_residual);
    SafeRelease(dx.debug_color_residual);
    SafeRelease(dx.debug_history_weight);
    SafeRelease(dx.reactive_mask);
    SafeRelease(dx.motion_vectors);
    SafeRelease(dx.previous_depth);
    SafeRelease(dx.depth);
    SafeRelease(dx.previous_history);
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
                    const std::string& reconstruction_result,
                    const osr::demo::wind_tunnel::TemporalResolveStats& temporal_resolve_stats,
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
    out << "reconstruction_result: " << reconstruction_result << "\n";
    out << "capture_pack: " << capture_path.string() << "\n";
    out << "transfer_match: " << (osr::demo::dx12_wind_tunnel::AllTransfersMatched(transfers) ? "true" : "false") << "\n";
    out << "temporal_resolve:\n";
    out << "  history_weight_mean: " << temporal_resolve_stats.history_weight_mean << "\n";
    out << "  history_weight_min: " << temporal_resolve_stats.history_weight_min << "\n";
    out << "  history_weight_max: " << temporal_resolve_stats.history_weight_max << "\n";
    out << "  reactive_suppressed_pct: " << temporal_resolve_stats.reactive_suppressed_pct << "\n";
    out << "  motion_suppressed_pct: " << temporal_resolve_stats.motion_suppressed_pct << "\n";
    out << "  color_rejected_pct: " << temporal_resolve_stats.color_rejected_pct << "\n";
    out << "  color_residual_mean: " << temporal_resolve_stats.color_residual_mean << "\n";
    out << "  depth_rejected_pct: " << temporal_resolve_stats.depth_rejected_pct << "\n";
    out << "  depth_residual_mean: " << temporal_resolve_stats.depth_residual_mean << "\n";
    out << "  sharpening_amount_mean: " << temporal_resolve_stats.sharpening_amount_mean << "\n";
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
            << " max_abs_diff=" << transfer.max_abs_diff
            << " mean_abs_diff=" << transfer.mean_abs_diff
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

const char* ToString(ReconstructionMode mode) noexcept {
    switch (mode) {
    case ReconstructionMode::SpatialGpu:
        return "spatial-gpu";
    case ReconstructionMode::TemporalCpu:
        return "temporal-cpu";
    case ReconstructionMode::TemporalGpu:
        return "temporal-gpu";
    }
    return "unknown";
}

bool ParseReconstructionMode(const std::string& value, ReconstructionMode& mode) {
    if (value == "spatial-gpu" || value == "spatial") {
        mode = ReconstructionMode::SpatialGpu;
    } else if (value == "temporal-cpu" || value == "temporal") {
        mode = ReconstructionMode::TemporalCpu;
    } else if (value == "temporal-gpu" || value == "gpu-temporal") {
        mode = ReconstructionMode::TemporalGpu;
    } else {
        return false;
    }
    return true;
}

bool WriteSequenceMetricsCsv(const osr::demo::wind_tunnel::SequenceMetricsResult& result,
                             const std::filesystem::path& path) {
    std::ofstream csv(path, std::ios::trunc);
    if (!csv) {
        return false;
    }
    csv << "frames,spatial_frame_delta_mean,temporal_frame_delta_mean,temporal_delta_ratio,"
           "stability_improvement_pct,ghost_score,reactive_trail_score,"
           "edge_preservation,thin_feature_contrast,text_readability_contrast,"
           "specular_history_leak,transparent_history_leak,"
           "reprojected_history_pct,reproject_out_of_bounds_pct,"
           "color_rejected_pct,color_residual_mean,depth_rejected_pct,depth_residual_mean,sharpening_amount_mean,"
           "temporal_history_weight_mean,temporal_reactive_suppressed_pct,temporal_motion_suppressed_pct\n";
    csv << result.frames << ","
        << result.spatial_frame_delta_mean << ","
        << result.temporal_frame_delta_mean << ","
        << result.temporal_delta_ratio << ","
        << result.stability_improvement_pct << ","
        << result.ghost_score << ","
        << result.reactive_trail_score << ","
        << result.edge_preservation << ","
        << result.thin_feature_contrast << ","
        << result.text_readability_contrast << ","
        << result.specular_history_leak << ","
        << result.transparent_history_leak << ","
        << result.reprojected_history_pct << ","
        << result.reproject_out_of_bounds_pct << ","
        << result.color_rejected_pct << ","
        << result.color_residual_mean << ","
        << result.depth_rejected_pct << ","
        << result.depth_residual_mean << ","
        << result.sharpening_amount_mean << ","
        << result.temporal_history_weight_mean << ","
        << result.temporal_reactive_suppressed_pct << ","
        << result.temporal_motion_suppressed_pct << "\n";
    return true;
}

std::vector<float> FloatBytesToVector(const std::vector<uint8_t>& bytes, osr::core::Dimensions size) {
    std::vector<float> out(static_cast<size_t>(size.width) * size.height, 0.0f);
    const size_t byte_count = std::min(bytes.size(), out.size() * sizeof(float));
    if (byte_count > 0) {
        std::memcpy(out.data(), bytes.data(), byte_count);
    }
    return out;
}

FloatMapDiff CompareFloatMaps(const std::vector<float>& expected, const std::vector<float>& actual) {
    FloatMapDiff diff;
    if (expected.size() != actual.size() || expected.empty()) {
        diff.max_abs = 1.0;
        diff.mean_abs = 1.0;
        return diff;
    }
    double sum = 0.0;
    for (size_t i = 0; i < expected.size(); ++i) {
        const double delta = std::abs(static_cast<double>(expected[i]) - static_cast<double>(actual[i]));
        diff.max_abs = std::max(diff.max_abs, delta);
        sum += delta;
    }
    diff.mean_abs = sum / static_cast<double>(expected.size());
    return diff;
}

FloatMapDiff MaxFloatDiff(FloatMapDiff lhs, FloatMapDiff rhs) noexcept {
    return {std::max(lhs.max_abs, rhs.max_abs), std::max(lhs.mean_abs, rhs.mean_abs)};
}

bool ReadTemporalDebugMaps(Dx12Objects& dx,
                           osr::core::Dimensions display_size,
                           osr::demo::wind_tunnel::TemporalResolveDebugMaps& maps,
                           std::vector<osr::demo::dx12_wind_tunnel::TextureTransferResult>* transfers) {
    std::vector<uint8_t> history_bytes;
    std::vector<uint8_t> color_residual_bytes;
    std::vector<uint8_t> depth_residual_bytes;
    osr::demo::dx12_wind_tunnel::TextureTransferResult history_readback;
    osr::demo::dx12_wind_tunnel::TextureTransferResult color_residual_readback;
    osr::demo::dx12_wind_tunnel::TextureTransferResult depth_residual_readback;
    const uint64_t debug_row_bytes = static_cast<uint64_t>(display_size.width) * sizeof(float);
    const bool ok =
        osr::demo::dx12_wind_tunnel::ReadbackTexture2DBytes(dx.device,
                                                            dx.queue,
                                                            dx.allocator,
                                                            dx.command_list,
                                                            dx.sync,
                                                            dx.debug_history_weight,
                                                            DXGI_FORMAT_R32_FLOAT,
                                                            display_size,
                                                            debug_row_bytes,
                                                            "debug_history_weight_after_dispatch",
                                                            history_bytes,
                                                            history_readback) &&
        osr::demo::dx12_wind_tunnel::ReadbackTexture2DBytes(dx.device,
                                                            dx.queue,
                                                            dx.allocator,
                                                            dx.command_list,
                                                            dx.sync,
                                                            dx.debug_color_residual,
                                                            DXGI_FORMAT_R32_FLOAT,
                                                            display_size,
                                                            debug_row_bytes,
                                                            "debug_color_residual_after_dispatch",
                                                            color_residual_bytes,
                                                            color_residual_readback) &&
        osr::demo::dx12_wind_tunnel::ReadbackTexture2DBytes(dx.device,
                                                            dx.queue,
                                                            dx.allocator,
                                                            dx.command_list,
                                                            dx.sync,
                                                            dx.debug_depth_residual,
                                                            DXGI_FORMAT_R32_FLOAT,
                                                            display_size,
                                                            debug_row_bytes,
                                                            "debug_depth_residual_after_dispatch",
                                                            depth_residual_bytes,
                                                            depth_residual_readback);
    if (!ok) {
        return false;
    }
    maps.display_size = display_size;
    maps.history_weight = FloatBytesToVector(history_bytes, display_size);
    maps.color_residual = FloatBytesToVector(color_residual_bytes, display_size);
    maps.depth_residual = FloatBytesToVector(depth_residual_bytes, display_size);
    if (transfers) {
        transfers->push_back(history_readback);
        transfers->push_back(color_residual_readback);
        transfers->push_back(depth_residual_readback);
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    bool headless = false;
    int present_frames = -1;
    bool metric_gate = false;
    osr::core::Dimensions requested_display_size {1280, 800};
    float requested_render_scale = 2.0f / 3.0f;
    uint64_t requested_frame_id = 8;
    uint32_t requested_frames = 1;
    int64_t requested_capture_frame_id = -1;
    bool requested_reset_history = false;
    ReconstructionMode reconstruction_mode = ReconstructionMode::SpatialGpu;
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
        } else if (std::string(argv[i]) == "--frame-id" && i + 1 < argc) {
            requested_frame_id = static_cast<uint64_t>(std::max(0, std::atoi(argv[++i])));
        } else if (std::string(argv[i]) == "--frames" && i + 1 < argc) {
            requested_frames = static_cast<uint32_t>(std::max(1, std::atoi(argv[++i])));
        } else if ((std::string(argv[i]) == "--capture-frame" || std::string(argv[i]) == "--capture-frame-id") && i + 1 < argc) {
            requested_capture_frame_id = static_cast<int64_t>(std::max(0, std::atoi(argv[++i])));
        } else if (std::string(argv[i]) == "--reset-history") {
            requested_reset_history = true;
        } else if ((std::string(argv[i]) == "--reconstruction" || std::string(argv[i]) == "--mode") && i + 1 < argc) {
            if (!ParseReconstructionMode(argv[++i], reconstruction_mode)) {
                std::cerr << "Unknown --reconstruction. Use spatial-gpu, temporal-cpu, or temporal-gpu.\n";
                return 2;
            }
        } else if (std::string(argv[i]) == "--mv-mode" && i + 1 < argc) {
            if (!ParseMotionVectorMode(argv[++i], mv_mode)) {
                std::cerr << "Unknown --mv-mode. Use correct, zero, flip-x, flip-y, half-scale, double-scale, or jitter-contaminated.\n";
                return 2;
            }
        }
    }

    std::filesystem::create_directories("build/manual");
    osr::core::Logger::Instance().Configure("build/manual/osr_dx12_wind_tunnel.log", osr::core::LogLevel::Debug);

    if (headless && requested_frames > 1 && reconstruction_mode != ReconstructionMode::TemporalGpu) {
        osr::demo::wind_tunnel::SequenceMetricsSettings sequence_settings;
        sequence_settings.display_size = requested_display_size;
        sequence_settings.render_scale = requested_render_scale;
        sequence_settings.start_frame = requested_frame_id;
        sequence_settings.frame_count = requested_frames;
        const auto sequence = osr::demo::wind_tunnel::RunSequenceMetrics(sequence_settings);
        const auto sequence_path = std::filesystem::path("build/manual/osr_dx12_sequence_metrics.csv");
        WriteSequenceMetricsCsv(sequence, sequence_path);
        std::cout << "oSR DX12 wind tunnel sequence lab\n";
        std::cout << "Frames: " << sequence.frames << "\n";
        std::cout << "Spatial frame delta mean: " << sequence.spatial_frame_delta_mean << "\n";
        std::cout << "Temporal frame delta mean: " << sequence.temporal_frame_delta_mean << "\n";
        std::cout << "Temporal/spatial delta ratio: " << sequence.temporal_delta_ratio << "\n";
        std::cout << "Stability improvement: " << sequence.stability_improvement_pct << "%\n";
        std::cout << "Ghost score: " << sequence.ghost_score << "\n";
        std::cout << "Reactive trail score: " << sequence.reactive_trail_score << "\n";
        std::cout << "Edge preservation: " << sequence.edge_preservation << "\n";
        std::cout << "Thin feature contrast: " << sequence.thin_feature_contrast << "\n";
        std::cout << "Text readability contrast: " << sequence.text_readability_contrast << "\n";
        std::cout << "Specular history leak: " << sequence.specular_history_leak << "\n";
        std::cout << "Transparent history leak: " << sequence.transparent_history_leak << "\n";
        std::cout << "Reprojected history: " << sequence.reprojected_history_pct << "%\n";
        std::cout << "Reproject OOB: " << sequence.reproject_out_of_bounds_pct << "%\n";
        std::cout << "Color rejected: " << sequence.color_rejected_pct << "%\n";
        std::cout << "Color residual mean: " << sequence.color_residual_mean << "\n";
        std::cout << "Depth rejected: " << sequence.depth_rejected_pct << "%\n";
        std::cout << "Depth residual mean: " << sequence.depth_residual_mean << "\n";
        std::cout << "Sharpening amount mean: " << sequence.sharpening_amount_mean << "\n";
        std::cout << "Temporal history weight mean: " << sequence.temporal_history_weight_mean << "\n";
        std::cout << "Metrics: " << sequence_path.string() << "\n";
        if (metric_gate && (sequence.temporal_delta_ratio > 0.80 ||
                            sequence.ghost_score > 0.45 ||
                            sequence.reactive_trail_score > 0.12 ||
                            sequence.edge_preservation < 0.72 ||
                            sequence.thin_feature_contrast < 0.82 ||
                            sequence.thin_feature_contrast > 1.45 ||
                            sequence.text_readability_contrast < 0.72 ||
                            sequence.text_readability_contrast > 1.35 ||
                            sequence.specular_history_leak > 0.12 ||
                            sequence.transparent_history_leak > 0.32)) {
            std::cerr << "Metric gate failed: temporal stability, ghost, reactive-trail, edge/text quality, or material history leak outside threshold.\n";
            return 3;
        }
        return 0;
    }

    Dx12Objects dx;
    if (!CreateDeviceObjects(dx)) {
        Release(dx);
        std::cout << "DX12 initialization failed. See console output.\n";
        return 1;
    }

    if (headless && requested_frames > 1 && reconstruction_mode == ReconstructionMode::TemporalGpu) {
        osr::demo::wind_tunnel::SyntheticFrameSettings sequence_frame_settings;
        sequence_frame_settings.display_size = requested_display_size;
        sequence_frame_settings.render_scale = requested_render_scale;
        sequence_frame_settings.frame_id = requested_frame_id;
        sequence_frame_settings.motion_vector_mode = mv_mode;
        auto first_frame = osr::demo::wind_tunnel::BuildSyntheticFrame(sequence_frame_settings);
        const auto render_size = first_frame.context.render_size;
        const auto display_size = first_frame.context.display_size;

        bool ok = true;
        ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.color_input, L"oSR sequence color input");
        ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.color_output, L"oSR sequence color output");
        ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, &dx.previous_history, L"oSR sequence previous history");
        ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.depth, L"oSR sequence depth");
        ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_NONE, &dx.previous_depth, L"oSR sequence previous depth");
        ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.motion_vectors, L"oSR sequence motion vectors");
        ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.reactive_mask, L"oSR sequence reactive mask");
        ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.debug_history_weight, L"oSR sequence debug history weight");
        ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.debug_color_residual, L"oSR sequence debug color residual");
        ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.debug_depth_residual, L"oSR sequence debug depth residual");
        if (!ok) {
            std::cerr << "Failed to create sequence D3D12 textures.\n";
            Release(dx);
            return 1;
        }

        auto upload_sequence = [&](ID3D12Resource* resource,
                                   DXGI_FORMAT format,
                                   osr::core::Dimensions size,
                                   const void* data,
                                   uint64_t row_bytes,
                                   const char* name) {
            osr::demo::dx12_wind_tunnel::TextureTransferResult result;
            return osr::demo::dx12_wind_tunnel::UploadReadbackTexture2D(dx.device,
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
                                                                         result) && result.matched;
        };

        auto copy_resource = [&](ID3D12Resource* src, ID3D12Resource* dst) {
            if (Failed(dx.allocator->Reset(), "Reset allocator") ||
                Failed(dx.command_list->Reset(dx.allocator, nullptr), "Reset command list")) {
                return false;
            }
            D3D12_RESOURCE_BARRIER barriers[2] {};
            barriers[0].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[0].Transition.pResource = src;
            barriers[0].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[0].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[0].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_SOURCE;
            barriers[1].Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
            barriers[1].Transition.pResource = dst;
            barriers[1].Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
            barriers[1].Transition.StateBefore = D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;
            barriers[1].Transition.StateAfter = D3D12_RESOURCE_STATE_COPY_DEST;
            dx.command_list->ResourceBarrier(2, barriers);
            dx.command_list->CopyResource(dst, src);
            std::swap(barriers[0].Transition.StateBefore, barriers[0].Transition.StateAfter);
            std::swap(barriers[1].Transition.StateBefore, barriers[1].Transition.StateAfter);
            dx.command_list->ResourceBarrier(2, barriers);
            return osr::demo::dx12_wind_tunnel::ExecuteAndWait(dx.queue, dx.command_list, dx.sync);
        };

        osr::backends::dx12::TemporalResolvePass temporal_pass;
        if (!temporal_pass.Initialize(dx.device)) {
            Release(dx);
            return 1;
        }

        std::vector<uint32_t> previous_cpu_temporal;
        osr::demo::wind_tunnel::SyntheticFrame previous_frame;
        double max_mean_abs_diff = 0.0;
        uint32_t max_abs_diff = 0;
        uint32_t frames_checked = 0;
        bool sequence_ok = true;
        bool sequence_capture_written = false;
        std::filesystem::path sequence_capture_path;
        const std::vector<float> zero_debug(static_cast<size_t>(display_size.width) * display_size.height, 0.0f);

        for (uint32_t frame_index = 0; frame_index < requested_frames && sequence_ok; ++frame_index) {
            osr::demo::wind_tunnel::SyntheticFrameSettings frame_settings;
            frame_settings.display_size = requested_display_size;
            frame_settings.render_scale = requested_render_scale;
            frame_settings.frame_id = requested_frame_id + frame_index;
            frame_settings.reset_history = frame_index == 0;
            frame_settings.motion_vector_mode = mv_mode;
            auto frame = osr::demo::wind_tunnel::BuildSyntheticFrame(frame_settings);
            frame.context.source_api = "oSR.dx12_temporal_sequence";
            frame.context.color_input = D3DResource(osr::core::ResourceKind::ColorInput, dx.color_input, 0x3000, render_size, DXGI_FORMAT_R8G8B8A8_UNORM, "dx12_sequence_color_input_rgba8");
            frame.context.color_output = D3DResource(osr::core::ResourceKind::ColorOutput, dx.color_output, 0x3001, display_size, DXGI_FORMAT_R8G8B8A8_UNORM, "dx12_sequence_color_output_rgba8");
            frame.context.depth = D3DResource(osr::core::ResourceKind::Depth, dx.depth, 0x3002, render_size, DXGI_FORMAT_R32_FLOAT, "dx12_sequence_depth_r32f");
            frame.context.motion_vectors = D3DResource(osr::core::ResourceKind::MotionVectors, dx.motion_vectors, 0x3003, render_size, DXGI_FORMAT_R32G32_FLOAT, "dx12_sequence_motion_vectors_r32g32f");
            frame.context.reactive_mask = D3DResource(osr::core::ResourceKind::ReactiveMask, dx.reactive_mask, 0x3004, render_size, DXGI_FORMAT_R32_FLOAT, "dx12_sequence_reactive_mask_r32f");

            const auto spatial = osr::demo::dx12_wind_tunnel::UpscaleBilinear(frame.color, render_size, display_size);
            std::vector<uint32_t> cpu_temporal = spatial;
            osr::demo::wind_tunnel::TemporalResolveStats stats;
            osr::demo::wind_tunnel::TemporalResolveDebugMaps cpu_debug_maps;
            if (!previous_cpu_temporal.empty()) {
                cpu_temporal = osr::demo::wind_tunnel::ResolveTemporalDisplay(spatial,
                                                                               previous_cpu_temporal,
                                                                               frame,
                                                                               display_size,
                                                                               {},
                                                                               &stats,
                                                                               &previous_frame,
                                                                               &cpu_debug_maps);
            }

            sequence_ok = sequence_ok &&
                upload_sequence(dx.color_input, DXGI_FORMAT_R8G8B8A8_UNORM, render_size, frame.color.data(), static_cast<uint64_t>(render_size.width) * sizeof(uint32_t), "sequence_color_input") &&
                upload_sequence(dx.color_output, DXGI_FORMAT_R8G8B8A8_UNORM, display_size, cpu_temporal.data(), static_cast<uint64_t>(display_size.width) * sizeof(uint32_t), "sequence_color_output_reference") &&
                upload_sequence(dx.depth, DXGI_FORMAT_R32_FLOAT, render_size, frame.depth.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "sequence_depth") &&
                upload_sequence(dx.motion_vectors, DXGI_FORMAT_R32G32_FLOAT, render_size, frame.motion_vectors.data(), static_cast<uint64_t>(render_size.width) * sizeof(osr::demo::wind_tunnel::Float2Buffer), "sequence_motion_vectors") &&
                upload_sequence(dx.reactive_mask, DXGI_FORMAT_R32_FLOAT, render_size, frame.reactive_mask.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "sequence_reactive_mask") &&
                upload_sequence(dx.debug_history_weight, DXGI_FORMAT_R32_FLOAT, display_size, zero_debug.data(), static_cast<uint64_t>(display_size.width) * sizeof(float), "sequence_debug_history_weight_zero") &&
                upload_sequence(dx.debug_color_residual, DXGI_FORMAT_R32_FLOAT, display_size, zero_debug.data(), static_cast<uint64_t>(display_size.width) * sizeof(float), "sequence_debug_color_residual_zero") &&
                upload_sequence(dx.debug_depth_residual, DXGI_FORMAT_R32_FLOAT, display_size, zero_debug.data(), static_cast<uint64_t>(display_size.width) * sizeof(float), "sequence_debug_depth_residual_zero");
            if (!sequence_ok) {
                break;
            }

            if (frame_index == 0) {
                sequence_ok = upload_sequence(dx.previous_history, DXGI_FORMAT_R8G8B8A8_UNORM, display_size, spatial.data(), static_cast<uint64_t>(display_size.width) * sizeof(uint32_t), "sequence_initial_history") &&
                              upload_sequence(dx.previous_depth, DXGI_FORMAT_R32_FLOAT, render_size, frame.depth.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "sequence_initial_previous_depth");
            } else if (Failed(dx.allocator->Reset(), "Reset allocator") ||
                       Failed(dx.command_list->Reset(dx.allocator, nullptr), "Reset command list")) {
                sequence_ok = false;
            } else {
                osr::backends::dx12::TemporalResolveResources temporal_resources;
                temporal_resources.current_color = dx.color_input;
                temporal_resources.previous_history = dx.previous_history;
                temporal_resources.current_depth = dx.depth;
                temporal_resources.previous_depth = dx.previous_depth;
                temporal_resources.motion_vectors = dx.motion_vectors;
                temporal_resources.reactive_mask = dx.reactive_mask;
                temporal_resources.output_color = dx.color_output;
                temporal_resources.debug_history_weight = dx.debug_history_weight;
                temporal_resources.debug_color_residual = dx.debug_color_residual;
                temporal_resources.debug_depth_residual = dx.debug_depth_residual;
                osr::backends::dx12::TemporalResolveConstants constants;
                constants.render_size = render_size;
                constants.display_size = display_size;
                sequence_ok = temporal_pass.Dispatch(dx.command_list, frame.context, temporal_resources, constants) &&
                              osr::demo::dx12_wind_tunnel::ExecuteAndWait(dx.queue, dx.command_list, dx.sync);
                osr::demo::dx12_wind_tunnel::TextureTransferResult readback;
                sequence_ok = sequence_ok &&
                    osr::demo::dx12_wind_tunnel::ReadbackTexture2D(dx.device,
                                                                   dx.queue,
                                                                   dx.allocator,
                                                                   dx.command_list,
                                                                   dx.sync,
                                                                   dx.color_output,
                                                                   DXGI_FORMAT_R8G8B8A8_UNORM,
                                                                   display_size,
                                                                   cpu_temporal.data(),
                                                                   static_cast<uint64_t>(display_size.width) * sizeof(uint32_t),
                                                                   "sequence_temporal_gpu_output",
                                                                   readback) &&
                    readback.matched;
                max_abs_diff = std::max(max_abs_diff, readback.max_abs_diff);
                max_mean_abs_diff = std::max(max_mean_abs_diff, readback.mean_abs_diff);
                ++frames_checked;
                if (requested_capture_frame_id >= 0 &&
                    static_cast<uint64_t>(requested_capture_frame_id) == frame.context.frame_id) {
                    osr::demo::wind_tunnel::TemporalResolveDebugMaps gpu_debug_maps;
                    sequence_ok = ReadTemporalDebugMaps(dx, display_size, gpu_debug_maps, nullptr);
                    if (sequence_ok) {
                        osr::debug::CapturePackConfig capture_config;
                        capture_config.root = "build/manual/captures";
                        capture_config.scenario = "dx12_temporal_sequence";
                        capture_config.mode = std::string("temporal_gpu_sequence_") + osr::demo::wind_tunnel::ToString(mv_mode);
                        capture_config.algorithm = ToString(reconstruction_mode);
                        osr::debug::CapturePackWriter sequence_capture;
                        sequence_ok = sequence_capture.BeginSession(capture_config) &&
                                      sequence_capture.WriteSessionManifest(frame.context, GetCommandLineA());
                        bool selected_debug_parity_ok = true;
                        if (sequence_ok) {
                            uint32_t validation_errors = 0;
                            uint32_t validation_warnings = 0;
                            const auto capture_report = osr::core::ValidateFrameContext(frame.context);
                            CountValidation(capture_report, validation_errors, validation_warnings);
                            osr::debug::HarnessFrameRow row;
                            row.frame_id = frame.context.frame_id;
                            row.render_size = render_size;
                            row.display_size = display_size;
                            row.jitter = frame.context.jitter_offset;
                            row.motion_vector_scale = frame.context.motion_vector_scale;
                            row.reset_history = frame.context.flags.reset_history;
                            row.validation_errors = validation_errors;
                            row.validation_warnings = validation_warnings;
                            row.gpu_reconstruct_ms = 0.0;
                            sequence_capture.WriteFrameRow(row);

                            osr::debug::HarnessMetricRow metrics;
                            metrics.frame_id = frame.context.frame_id;
                            metrics.reactive_trail_score = stats.reactive_history_weight_mean;
                            metrics.specular_history_leak = osr::demo::wind_tunnel::MeanMaterialHistoryLeak(gpu_debug_maps, display_size, frame.context.frame_id, true);
                            metrics.transparent_history_leak = osr::demo::wind_tunnel::MeanMaterialHistoryLeak(gpu_debug_maps, display_size, frame.context.frame_id, false);
                            metrics.history_reject_pct = 100.0 - stats.history_weight_mean * 100.0;
                            const auto history_diff = CompareFloatMaps(cpu_debug_maps.history_weight, gpu_debug_maps.history_weight);
                            const auto color_diff = CompareFloatMaps(cpu_debug_maps.color_residual, gpu_debug_maps.color_residual);
                            const auto depth_diff = CompareFloatMaps(cpu_debug_maps.depth_residual, gpu_debug_maps.depth_residual);
                            const auto selected_debug_diff = MaxFloatDiff(MaxFloatDiff(history_diff, color_diff), depth_diff);
                            selected_debug_parity_ok = selected_debug_diff.max_abs <= 0.35 && selected_debug_diff.mean_abs <= 0.002;
                            metrics.debug_history_weight_max_abs = history_diff.max_abs;
                            metrics.debug_history_weight_mean_abs = history_diff.mean_abs;
                            metrics.debug_color_residual_max_abs = color_diff.max_abs;
                            metrics.debug_color_residual_mean_abs = color_diff.mean_abs;
                            metrics.debug_depth_residual_max_abs = depth_diff.max_abs;
                            metrics.debug_depth_residual_mean_abs = depth_diff.mean_abs;
                            sequence_capture.WriteMetricRow(metrics);
                            sequence_capture.WriteValidationWarnings(frame.context.frame_id, capture_report);
                            sequence_capture.WriteFrameContextJson(frame.context);
                        }
                        std::ostringstream frame_dir_name;
                        frame_dir_name << "frame_" << std::setw(6) << std::setfill('0') << frame.context.frame_id;
                        sequence_capture_path = sequence_capture.SessionPath();
                        osr::demo::wind_tunnel::DebugDumpResult dump;
                        if (sequence_ok) {
                            dump = osr::demo::wind_tunnel::WriteSyntheticFrameDebugDumps(sequence_capture.SessionPath() / frame_dir_name.str(),
                                                                                         frame,
                                                                                         cpu_temporal,
                                                                                         0,
                                                                                         0,
                                                                                         0,
                                                                                         0,
                                                                                         0,
                                                                                         &gpu_debug_maps);
                        }
                        sequence_capture_written = dump.AllRequired();
                        sequence_ok = sequence_ok && sequence_capture_written && selected_debug_parity_ok;
                    }
                }
            }

            sequence_ok = sequence_ok &&
                          copy_resource(dx.color_output, dx.previous_history) &&
                          copy_resource(dx.depth, dx.previous_depth);
            previous_cpu_temporal = std::move(cpu_temporal);
            previous_frame = std::move(frame);
        }

        std::cout << "oSR DX12 temporal-gpu sequence\n";
        std::cout << "Frames: " << requested_frames << "\n";
        std::cout << "Checked temporal frames: " << frames_checked << "\n";
        std::cout << "Max byte diff: " << max_abs_diff << "\n";
        std::cout << "Max mean byte diff: " << max_mean_abs_diff << "\n";
        std::cout << "Persistent history: GPU output copied forward each frame\n";
        if (requested_capture_frame_id >= 0) {
            std::cout << "Capture frame: " << requested_capture_frame_id
                      << (sequence_capture_written ? " written to " : " not written ")
                      << sequence_capture_path.string() << "\n";
        }
        Release(dx);
        if (!sequence_ok ||
            (requested_capture_frame_id >= 0 && !sequence_capture_written) ||
            (metric_gate && (max_abs_diff > 32 || max_mean_abs_diff > 0.06))) {
            std::cerr << "Temporal-gpu sequence gate failed.\n";
            return 3;
        }
        return 0;
    }

    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = requested_display_size;
    settings.render_scale = requested_render_scale;
    settings.frame_id = requested_frame_id;
    settings.reset_history = requested_reset_history;
    settings.motion_vector_mode = mv_mode;
    auto synthetic = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    synthetic.context.notes.push_back(std::string("Reconstruction mode: ") + ToString(reconstruction_mode));
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
    ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R8G8B8A8_UNORM, D3D12_RESOURCE_FLAG_NONE, &dx.previous_history, L"oSR previous display history");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.depth, L"oSR synthetic depth");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_NONE, &dx.previous_depth, L"oSR previous depth");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32G32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.motion_vectors, L"oSR synthetic motion vectors");
    ok = ok && CreateTexture(dx.device, render_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.reactive_mask, L"oSR synthetic reactive mask");
    ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.debug_history_weight, L"oSR debug history weight");
    ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.debug_color_residual, L"oSR debug color residual");
    ok = ok && CreateTexture(dx.device, display_size, DXGI_FORMAT_R32_FLOAT, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, &dx.debug_depth_residual, L"oSR debug depth residual");
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

    const auto spatial_output = osr::demo::dx12_wind_tunnel::UpscaleBilinear(synthetic.color, render_size, display_size);
    const auto previous_display_output = osr::demo::dx12_wind_tunnel::UpscaleBilinear(previous_synthetic.color, render_size, display_size);
    osr::demo::wind_tunnel::TemporalResolveStats temporal_resolve_stats;
    osr::demo::wind_tunnel::TemporalResolveDebugMaps temporal_debug_maps;
    std::vector<uint32_t> resolved_output = spatial_output;
    const bool temporal_mode = reconstruction_mode == ReconstructionMode::TemporalCpu || reconstruction_mode == ReconstructionMode::TemporalGpu;
    if (temporal_mode) {
        osr::demo::wind_tunnel::TemporalResolveSettings resolve_settings;
        resolved_output = osr::demo::wind_tunnel::ResolveTemporalDisplay(spatial_output,
                                                                          previous_display_output,
                                                                          synthetic,
                                                                          display_size,
                                                                          resolve_settings,
                                                                          &temporal_resolve_stats,
                                                                          &previous_synthetic,
                                                                          &temporal_debug_maps);
        synthetic.context.notes.push_back(reconstruction_mode == ReconstructionMode::TemporalCpu
            ? "CPU temporal resolve blended display-space history before DX12 presentation."
            : "CPU temporal resolve produced the parity reference for the GPU temporal pass.");
    }

    const std::vector<float> zero_debug(static_cast<size_t>(display_size.width) * display_size.height, 0.0f);
    const bool transfer_ok =
        upload(dx.color_input, DXGI_FORMAT_R8G8B8A8_UNORM, render_size, synthetic.color.data(), static_cast<uint64_t>(render_size.width) * sizeof(uint32_t), "color_input") &&
        upload(dx.color_output, DXGI_FORMAT_R8G8B8A8_UNORM, display_size, resolved_output.data(), static_cast<uint64_t>(display_size.width) * sizeof(uint32_t), "color_output") &&
        upload(dx.previous_history, DXGI_FORMAT_R8G8B8A8_UNORM, display_size, previous_display_output.data(), static_cast<uint64_t>(display_size.width) * sizeof(uint32_t), "previous_history") &&
        upload(dx.depth, DXGI_FORMAT_R32_FLOAT, render_size, synthetic.depth.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "depth") &&
        upload(dx.previous_depth, DXGI_FORMAT_R32_FLOAT, render_size, previous_synthetic.depth.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "previous_depth") &&
        upload(dx.motion_vectors, DXGI_FORMAT_R32G32_FLOAT, render_size, synthetic.motion_vectors.data(), static_cast<uint64_t>(render_size.width) * sizeof(osr::demo::wind_tunnel::Float2Buffer), "motion_vectors") &&
        upload(dx.reactive_mask, DXGI_FORMAT_R32_FLOAT, render_size, synthetic.reactive_mask.data(), static_cast<uint64_t>(render_size.width) * sizeof(float), "reactive_mask") &&
        (reconstruction_mode != ReconstructionMode::TemporalGpu ||
         (upload(dx.debug_history_weight, DXGI_FORMAT_R32_FLOAT, display_size, zero_debug.data(), static_cast<uint64_t>(display_size.width) * sizeof(float), "debug_history_weight_zero") &&
          upload(dx.debug_color_residual, DXGI_FORMAT_R32_FLOAT, display_size, zero_debug.data(), static_cast<uint64_t>(display_size.width) * sizeof(float), "debug_color_residual_zero") &&
          upload(dx.debug_depth_residual, DXGI_FORMAT_R32_FLOAT, display_size, zero_debug.data(), static_cast<uint64_t>(display_size.width) * sizeof(float), "debug_depth_residual_zero")));
    synthetic.context.notes.push_back(transfer_ok ? "D3D12 upload/readback hashes matched CPU buffers." : "D3D12 upload/readback hash mismatch detected.");

    const auto report = osr::core::ValidateFrameContext(synthetic.context);
    osr::backends::dx12::Dx12Backend backend;
    const bool backend_initialized = reconstruction_mode == ReconstructionMode::SpatialGpu && backend.Initialize(dx.device);
    bool dispatch_result = false;
    if (reconstruction_mode == ReconstructionMode::SpatialGpu &&
        backend_initialized &&
        SUCCEEDED(dx.allocator->Reset()) &&
        SUCCEEDED(dx.command_list->Reset(dx.allocator, nullptr))) {
        dispatch_result = backend.DispatchDebugUpscale(dx.command_list, synthetic.context) &&
                          osr::demo::dx12_wind_tunnel::ExecuteAndWait(dx.queue, dx.command_list, dx.sync);
    } else if (reconstruction_mode == ReconstructionMode::TemporalGpu &&
               SUCCEEDED(dx.allocator->Reset()) &&
               SUCCEEDED(dx.command_list->Reset(dx.allocator, nullptr))) {
        osr::backends::dx12::TemporalResolvePass temporal_pass;
        osr::backends::dx12::TemporalResolveResources temporal_resources;
        temporal_resources.current_color = dx.color_input;
        temporal_resources.previous_history = dx.previous_history;
        temporal_resources.current_depth = dx.depth;
        temporal_resources.previous_depth = dx.previous_depth;
        temporal_resources.motion_vectors = dx.motion_vectors;
        temporal_resources.reactive_mask = dx.reactive_mask;
        temporal_resources.output_color = dx.color_output;
        temporal_resources.debug_history_weight = dx.debug_history_weight;
        temporal_resources.debug_color_residual = dx.debug_color_residual;
        temporal_resources.debug_depth_residual = dx.debug_depth_residual;
        osr::backends::dx12::TemporalResolveConstants temporal_constants;
        temporal_constants.render_size = render_size;
        temporal_constants.display_size = display_size;
        dispatch_result = temporal_pass.Initialize(dx.device) &&
                          temporal_pass.Dispatch(dx.command_list, synthetic.context, temporal_resources, temporal_constants) &&
                          osr::demo::dx12_wind_tunnel::ExecuteAndWait(dx.queue, dx.command_list, dx.sync);
    } else if (reconstruction_mode == ReconstructionMode::TemporalCpu) {
        dispatch_result = transfer_ok;
    }
    synthetic.context.notes.push_back(dispatch_result ? "Reconstruction output is present in D3D12 color_output." : "Reconstruction output did not complete.");
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
                                                       resolved_output.data(),
                                                       static_cast<uint64_t>(display_size.width) * sizeof(uint32_t),
                                                       "color_output_after_dispatch",
                                                       reconstructed_output);
    const bool reconstructed_output_match = reconstructed_output_readback && reconstructed_output.matched;
    if (!reconstructed_output.name.empty()) {
        transfers.push_back(reconstructed_output);
    }
    FloatMapDiff debug_map_diff;
    FloatMapDiff history_debug_map_diff;
    FloatMapDiff color_debug_map_diff;
    FloatMapDiff depth_debug_map_diff;
    bool debug_map_parity_checked = false;
    bool debug_map_parity_ok = true;
    if (dispatch_result && reconstruction_mode == ReconstructionMode::TemporalGpu) {
        const auto cpu_debug_maps = temporal_debug_maps;
        std::vector<uint8_t> history_bytes;
        std::vector<uint8_t> color_residual_bytes;
        std::vector<uint8_t> depth_residual_bytes;
        osr::demo::dx12_wind_tunnel::TextureTransferResult history_readback;
        osr::demo::dx12_wind_tunnel::TextureTransferResult color_residual_readback;
        osr::demo::dx12_wind_tunnel::TextureTransferResult depth_residual_readback;
        const uint64_t debug_row_bytes = static_cast<uint64_t>(display_size.width) * sizeof(float);
        const bool debug_readback =
            osr::demo::dx12_wind_tunnel::ReadbackTexture2DBytes(dx.device,
                                                                dx.queue,
                                                                dx.allocator,
                                                                dx.command_list,
                                                                dx.sync,
                                                                dx.debug_history_weight,
                                                                DXGI_FORMAT_R32_FLOAT,
                                                                display_size,
                                                                debug_row_bytes,
                                                                "debug_history_weight_after_dispatch",
                                                                history_bytes,
                                                                history_readback) &&
            osr::demo::dx12_wind_tunnel::ReadbackTexture2DBytes(dx.device,
                                                                dx.queue,
                                                                dx.allocator,
                                                                dx.command_list,
                                                                dx.sync,
                                                                dx.debug_color_residual,
                                                                DXGI_FORMAT_R32_FLOAT,
                                                                display_size,
                                                                debug_row_bytes,
                                                                "debug_color_residual_after_dispatch",
                                                                color_residual_bytes,
                                                                color_residual_readback) &&
            osr::demo::dx12_wind_tunnel::ReadbackTexture2DBytes(dx.device,
                                                                dx.queue,
                                                                dx.allocator,
                                                                dx.command_list,
                                                                dx.sync,
                                                                dx.debug_depth_residual,
                                                                DXGI_FORMAT_R32_FLOAT,
                                                                display_size,
                                                                debug_row_bytes,
                                                                "debug_depth_residual_after_dispatch",
                                                                depth_residual_bytes,
                                                                depth_residual_readback);
        if (debug_readback) {
            osr::demo::wind_tunnel::TemporalResolveDebugMaps gpu_debug_maps;
            gpu_debug_maps.display_size = display_size;
            gpu_debug_maps.history_weight = FloatBytesToVector(history_bytes, display_size);
            gpu_debug_maps.color_residual = FloatBytesToVector(color_residual_bytes, display_size);
            gpu_debug_maps.depth_residual = FloatBytesToVector(depth_residual_bytes, display_size);
            history_debug_map_diff = CompareFloatMaps(cpu_debug_maps.history_weight, gpu_debug_maps.history_weight);
            color_debug_map_diff = CompareFloatMaps(cpu_debug_maps.color_residual, gpu_debug_maps.color_residual);
            depth_debug_map_diff = CompareFloatMaps(cpu_debug_maps.depth_residual, gpu_debug_maps.depth_residual);
            debug_map_diff = MaxFloatDiff(MaxFloatDiff(history_debug_map_diff, color_debug_map_diff), depth_debug_map_diff);
            debug_map_parity_checked = true;
            debug_map_parity_ok = debug_map_diff.max_abs <= 0.25 && debug_map_diff.mean_abs <= 0.002;
            temporal_debug_maps.display_size = display_size;
            temporal_debug_maps.history_weight = std::move(gpu_debug_maps.history_weight);
            temporal_debug_maps.color_residual = std::move(gpu_debug_maps.color_residual);
            temporal_debug_maps.depth_residual = std::move(gpu_debug_maps.depth_residual);
            transfers.push_back(history_readback);
            transfers.push_back(color_residual_readback);
            transfers.push_back(depth_residual_readback);
            std::ostringstream note;
            note << "Temporal-gpu debug maps were read back from shader UAVs; map parity max_abs="
                 << debug_map_diff.max_abs << " mean_abs=" << debug_map_diff.mean_abs << ".";
            synthetic.context.notes.push_back(note.str());
        } else {
            synthetic.context.notes.push_back("Temporal-gpu debug map readback failed; CPU oracle maps remain in capture.");
        }
    }
    synthetic.context.notes.push_back(reconstructed_output_match ? "D3D12 debug upscale output matched CPU nearest reference." : "D3D12 debug upscale output did not match CPU nearest reference.");

    osr::debug::CapturePackConfig capture_config;
    capture_config.root = "build/manual/captures";
    capture_config.scenario = "dx12_wind_tunnel";
    capture_config.mode = std::string("h1_buffer_truth_") + osr::demo::wind_tunnel::ToString(mv_mode);
    capture_config.algorithm = ToString(reconstruction_mode);
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
        row.gpu_reconstruct_ms = reconstruction_mode == ReconstructionMode::SpatialGpu && dispatch_result ? 0.0 : -1.0;
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
        metrics.history_reject_pct = 100.0 - temporal_resolve_stats.history_weight_mean * 100.0;
        if (temporal_mode) {
            metrics.reactive_trail_score = temporal_resolve_stats.reactive_history_weight_mean;
            metrics.specular_history_leak = osr::demo::wind_tunnel::MeanMaterialHistoryLeak(temporal_debug_maps,
                                                                                            synthetic.context.display_size,
                                                                                            synthetic.context.frame_id,
                                                                                            true);
            metrics.transparent_history_leak = osr::demo::wind_tunnel::MeanMaterialHistoryLeak(temporal_debug_maps,
                                                                                               synthetic.context.display_size,
                                                                                               synthetic.context.frame_id,
                                                                                               false);
            metrics.debug_history_weight_max_abs = history_debug_map_diff.max_abs;
            metrics.debug_history_weight_mean_abs = history_debug_map_diff.mean_abs;
            metrics.debug_color_residual_max_abs = color_debug_map_diff.max_abs;
            metrics.debug_color_residual_mean_abs = color_debug_map_diff.mean_abs;
            metrics.debug_depth_residual_max_abs = depth_debug_map_diff.max_abs;
            metrics.debug_depth_residual_mean_abs = depth_debug_map_diff.mean_abs;
        }
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
                                                                                       resolved_output,
                                                                                       color_input_hash,
                                                                                       color_output_hash,
                                                                                       depth_hash,
                                                                                       motion_vectors_hash,
                                                                                       reactive_mask_hash,
                                                                                       temporal_mode ? &temporal_debug_maps : nullptr);
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

    const std::string reconstruction_result = dispatch_result
        ? (reconstruction_mode == ReconstructionMode::SpatialGpu ? "dx12_compute_spatial_recorded_and_executed" :
           reconstruction_mode == ReconstructionMode::TemporalGpu ? "dx12_compute_temporal_resolve_recorded_and_executed" :
           "cpu_temporal_resolve_uploaded")
        : "reconstruction_failed";
    ExportMetadata(synthetic.context, report, reconstruction_result, temporal_resolve_stats, temporal_diagnostics, temporal_verdict, transfers, capture.SessionPath(), "build/manual/osr_dx12_wind_tunnel_metadata.txt");

    std::cout << "oSR DX12 wind tunnel proof of life\n";
    std::cout << "Render: " << render_size.width << "x" << render_size.height
              << "  Display: " << display_size.width << "x" << display_size.height << "\n";
    std::cout << "MV mode: " << osr::demo::wind_tunnel::ToString(mv_mode) << "\n";
    std::cout << "Reconstruction: " << ToString(reconstruction_mode) << "\n";
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
    if (debug_map_parity_checked) {
        std::cout << "Temporal debug map parity: "
                  << (debug_map_parity_ok ? "matched" : "FAILED")
                  << " max_abs=" << debug_map_diff.max_abs
                  << " mean_abs=" << debug_map_diff.mean_abs << "\n";
    }
    if (temporal_mode) {
        std::cout << "Temporal resolve: history_mean=" << temporal_resolve_stats.history_weight_mean
                  << " reactive_suppressed=" << temporal_resolve_stats.reactive_suppressed_pct
                  << "% motion_suppressed=" << temporal_resolve_stats.motion_suppressed_pct
                  << "% depth_rejected=" << temporal_resolve_stats.depth_rejected_pct
                  << "% sharpening_mean=" << temporal_resolve_stats.sharpening_amount_mean << "\n";
    }
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

    const bool has_errors = report.HasErrors() || !transfer_ok || !reconstructed_output_match || !debug_map_parity_ok || !capture_started || !present_ok;
    osr::demo::dx12_wind_tunnel::ReleasePresentState(present);
    Release(dx);
    if (!has_errors && temporal_verdict.metric_gate_failed) {
        return 3;
    }
    return has_errors ? 1 : 0;
}
