#pragma once

#include "core/frame_context.h"

#include <filesystem>
#include <cstddef>
#include <string>
#include <string_view>

namespace osr::debug {

struct CapturePackConfig {
    std::filesystem::path root = "build/manual/captures";
    std::string run_name;
    std::string scenario = "dx12_wind_tunnel";
    std::string mode = "h1_buffer_truth";
    std::string algorithm = "debug_upscale";
    bool overwrite_existing = false;
};

struct HarnessFrameRow {
    uint64_t frame_id = 0;
    double scenario_time_ms = 0.0;
    core::Dimensions render_size {};
    core::Dimensions display_size {};
    core::Float2 jitter {};
    core::Float2 motion_vector_scale {};
    bool reset_history = false;
    double gpu_upload_ms = 0.0;
    double gpu_reconstruct_ms = 0.0;
    double gpu_present_ms = 0.0;
    double cpu_frame_ms = 0.0;
    uint32_t validation_errors = 0;
    uint32_t validation_warnings = 0;
};

struct HarnessMetricRow {
    uint64_t frame_id = 0;
    double ghost_score = 0.0;
    double shimmer_score = 0.0;
    double disocclusion_leak = 0.0;
    double reactive_trail_score = 0.0;
    double edge_preservation = 0.0;
    double text_contrast = 0.0;
    double history_reject_pct = 0.0;
    double residual_search_pct = 0.0;
    double mv_luma_residual_mean = 0.0;
    double mv_luma_residual_p95 = 0.0;
    double mv_depth_residual_mean = 0.0;
    double mv_depth_residual_p95 = 0.0;
    double bad_history_trusted_pct = 0.0;
    double good_history_rejected_pct = 0.0;
    double reactive_history_trusted_pct = 0.0;
    double disocclusion_history_trusted_pct = 0.0;
    double trust_evidence_agreement_pct = 0.0;
    double history_trust_mean = 0.0;
    double accumulation_weight_mean = 0.0;
};

class CapturePackWriter {
public:
    bool BeginSession(const CapturePackConfig& config);
    bool WriteSessionManifest(const core::FrameContext& first_frame, const std::string& command_line);
    bool WriteFrameRow(const HarnessFrameRow& row);
    bool WriteMetricRow(const HarnessMetricRow& row);
    bool WriteValidationWarnings(uint64_t frame_id, const core::ValidationReport& report);
    bool WriteDiagnosticWarning(uint64_t frame_id,
                                std::string_view tag,
                                std::string_view likely_cause,
                                std::string_view suggested_action,
                                uint32_t severity,
                                double evidence_value);
    bool WriteFrameContextJson(const core::FrameContext& frame);

    [[nodiscard]] const std::filesystem::path& SessionPath() const noexcept;

private:
    CapturePackConfig config_;
    std::filesystem::path session_path_;
    std::filesystem::path frames_csv_;
    std::filesystem::path metrics_csv_;
    std::filesystem::path warnings_jsonl_;
    std::filesystem::path bookmarks_jsonl_;
};

[[nodiscard]] std::string JsonEscape(std::string_view value);
[[nodiscard]] std::string CsvEscape(std::string_view value);
[[nodiscard]] uint64_t HashBytes(const void* data, size_t size) noexcept;
[[nodiscard]] const char* ToString(core::ValidationSeverity severity) noexcept;

} // namespace osr::debug
