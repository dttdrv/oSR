#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace osr::debug {

struct CaptureComparisonRow {
    std::filesystem::path path;
    bool loaded = false;
    bool analysis_ok = false;
    bool gate_passed = false;
    std::string error;
    std::string gate_reason;
    uint64_t frame_id = 0;
    double score = 0.0;
    double motion_history_trusted_pct = 0.0;
    double static_history_trusted_pct = 0.0;
    double text_history_trusted_pct = 0.0;
    double specular_history_trusted_pct = 0.0;
    double transparent_history_trusted_pct = 0.0;
    double reactive_history_trusted_pct = 0.0;
    double color_reject_candidate_pct = 0.0;
    double text_contrast_ratio = 0.0;
    double bad_lock_signal = 0.0;
    double locked_detail_score = 0.0;
};

[[nodiscard]] CaptureComparisonRow LoadCaptureComparisonRow(const std::filesystem::path& capture_path);
[[nodiscard]] std::vector<CaptureComparisonRow> RankCaptureComparisons(std::vector<CaptureComparisonRow> rows);
[[nodiscard]] std::string CaptureComparisonCsvHeader();
[[nodiscard]] std::string CaptureComparisonCsvRow(size_t rank, const CaptureComparisonRow& row);

} // namespace osr::debug
