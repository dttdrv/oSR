#include "debug/capture_compare.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

void WriteAnalysis(const std::filesystem::path& dir,
                   bool gate_passed,
                   double motion,
                   double stat,
                   double text,
                   double specular,
                   double transparent,
                   double reactive,
                   double color_reject,
                   double text_contrast_ratio,
                   double text_native_contrast_ratio,
                   double bad_lock,
                   double locked_detail) {
    std::filesystem::create_directories(dir);
    std::ofstream out(dir / "capture_analysis.json", std::ios::trunc);
    out << "{\n";
    out << "  \"schema\": \"osr.capture.analysis.v1\",\n";
    out << "  \"ok\": true,\n";
    out << "  \"error\": \"\",\n";
    out << "  \"frame_id\": 12,\n";
    out << "  \"gate\": {\"passed\":" << (gate_passed ? "true" : "false") << ",\"reason\":\"" << (gate_passed ? "ok" : "bad") << "\"},\n";
    out << "  \"global\": {\"color_residual\":{\"over_threshold_pct\":" << color_reject << "}},\n";
    out << "  \"motion_static_split\": {\"motion_region_history_trusted_pct\":" << motion
        << ",\"static_region_history_trusted_pct\":" << stat << "},\n";
    out << "  \"locked_detail\": {\"text_contrast_ratio\":" << text_contrast_ratio
        << ",\"text_native_contrast_ratio\":" << text_native_contrast_ratio
        << ",\"bad_lock_signal\":" << bad_lock
        << ",\"score\":" << locked_detail << "},\n";
    out << "  \"regions\": {\n";
    out << "    \"text\": {\"history_trusted_pct\":" << text << "},\n";
    out << "    \"specular\": {\"history_trusted_pct\":" << specular << "},\n";
    out << "    \"transparent\": {\"history_trusted_pct\":" << transparent << "},\n";
    out << "    \"reactive\": {\"history_trusted_pct\":" << reactive << "}\n";
    out << "  }\n";
    out << "}\n";
}

void WriteSequenceMetrics(const std::filesystem::path& dir,
                          double temporal_delta_ratio,
                          double stability_improvement_pct) {
    std::ofstream out(dir / "sequence_gate_metrics.csv", std::ios::trunc);
    out << "frames,spatial_frame_delta_mean,temporal_frame_delta_mean,temporal_delta_ratio,stability_improvement_pct\n";
    out << "16,0.01,0.007," << temporal_delta_ratio << "," << stability_improvement_pct << "\n";
}

} // namespace

int main() {
    const std::filesystem::path root = "build/manual/capture_compare_tests";
    std::filesystem::remove_all(root);
    WriteAnalysis(root / "good", true, 0.0, 99.0, 55.0, 0.0, 1.0, 0.0, 0.5, 1.08, 0.97, 0.05, 95.0);
    WriteAnalysis(root / "blurry", true, 0.0, 89.0, 20.0, 0.0, 1.0, 0.0, 0.5, 0.90, 0.80, 0.05, 5.0);
    WriteAnalysis(root / "leaky", false, 5.0, 99.0, 55.0, 15.0, 20.0, 10.0, 10.0, 1.08, 0.97, 0.95, 90.0);
    WriteSequenceMetrics(root / "good", 0.72, 28.0);
    WriteSequenceMetrics(root / "blurry", 0.88, 12.0);

    auto good = osr::debug::LoadCaptureComparisonRow(root / "good");
    auto blurry = osr::debug::LoadCaptureComparisonRow(root / "blurry");
    auto leaky = osr::debug::LoadCaptureComparisonRow(root / "leaky");
    if (!good.loaded || !good.analysis_ok || !good.gate_passed) {
        return Fail("good comparison row failed to load");
    }
    if (good.text_native_contrast_ratio < 0.969 || good.text_native_contrast_ratio > 0.971) {
        return Fail("native contrast ratio failed to load");
    }
    if (!good.sequence_metrics_loaded ||
        good.sequence_temporal_delta_ratio < 0.719 ||
        good.sequence_temporal_delta_ratio > 0.721) {
        return Fail("sequence metrics failed to load");
    }
    if (!(good.score > blurry.score && blurry.score > leaky.score)) {
        return Fail("comparison score ordering mismatch");
    }

    auto ranked = osr::debug::RankCaptureComparisons({leaky, blurry, good});
    if (ranked.size() != 3 || ranked[0].path.filename() != "good" || ranked[2].path.filename() != "leaky") {
        return Fail("ranked comparison order mismatch");
    }
    const auto header = osr::debug::CaptureComparisonCsvHeader();
    const auto row = osr::debug::CaptureComparisonCsvRow(1, ranked[0]);
    if (header.find("reactive_history_trusted_pct") == std::string::npos ||
        header.find("text_contrast_ratio") == std::string::npos ||
        header.find("text_native_contrast_ratio") == std::string::npos ||
        header.find("bad_lock_signal") == std::string::npos ||
        header.find("locked_detail_score") == std::string::npos ||
        header.find("sequence_temporal_delta_ratio") == std::string::npos ||
        row.find(",1,1,1,") == std::string::npos) {
        return Fail("comparison CSV output missing expected fields");
    }
    return 0;
}
