#include "debug/capture_analysis.h"
#include "demo/wind_tunnel/synthetic_roi.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <cstdint>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

template <typename T>
void WriteRaw(const std::filesystem::path& path, const std::vector<T>& values) {
    std::ofstream out(path, std::ios::binary);
    out.write(reinterpret_cast<const char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(T)));
}

bool Contains(const std::filesystem::path& path, const std::string& needle) {
    std::ifstream in(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find(needle) != std::string::npos;
}

struct Float2 {
    float x;
    float y;
};

} // namespace

int main() {
    const std::filesystem::path dir = "build/manual/capture_analysis_tests/frame_000001";
    std::filesystem::remove_all(dir.parent_path());
    std::filesystem::create_directories(dir);

    {
        std::ofstream manifest(dir / "artifacts.json", std::ios::trunc);
        manifest << "{\n";
        manifest << "  \"resources\": [\n";
        manifest << "    {\"name\":\"motion_vectors\",\"raw\":\"motion_vectors.rg32f.raw\",\"format\":\"rg32f\",\"width\":2,\"height\":2},\n";
        manifest << "    {\"name\":\"color_output\",\"raw\":\"color_output.rgba8.raw\",\"format\":\"rgba8\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"spatial_baseline\",\"raw\":\"spatial_baseline.rgba8.raw\",\"format\":\"rgba8\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"native_reference\",\"raw\":\"native_reference.rgba8.raw\",\"format\":\"rgba8\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"history_weight\",\"raw\":\"history_weight.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"color_residual\",\"raw\":\"color_residual.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"depth_residual\",\"raw\":\"depth_residual.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"feature_lock_strength\",\"raw\":\"feature_lock_strength.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"reactive_mask\",\"raw\":\"reactive_mask.r32f.raw\",\"format\":\"r32f\",\"width\":2,\"height\":2}\n";
        manifest << "  ]\n";
        manifest << "}\n";
    }
    {
        std::ofstream context(dir / "frame_context.json", std::ios::trunc);
        context << "{\n  \"frame_id\": 12,\n  \"source_api\": \"test\"\n}\n";
    }

    WriteRaw(dir / "history_weight.r32f.raw", std::vector<float> {0.0f, 0.25f, 0.5f, 0.75f, 1.0f, 0.1f, 0.2f, 0.3f});
    WriteRaw(dir / "color_residual.r32f.raw", std::vector<float> {0.0f, 0.2f, 0.1f, 0.3f, 0.0f, 0.0f, 0.17f, 0.01f});
    WriteRaw(dir / "depth_residual.r32f.raw", std::vector<float> {0.0f, 0.01f, 0.02f, 0.04f, 0.0f, 0.05f, 0.0f, 0.01f});
    WriteRaw(dir / "feature_lock_strength.r32f.raw", std::vector<float> {0.0f, 0.6f, 0.4f, 0.8f, 0.9f, 0.0f, 0.0f, 0.1f});
    WriteRaw(dir / "color_output.rgba8.raw", std::vector<uint32_t>(8, 0xff404040u));
    WriteRaw(dir / "spatial_baseline.rgba8.raw", std::vector<uint32_t>(8, 0xff404040u));
    WriteRaw(dir / "native_reference.rgba8.raw", std::vector<uint32_t>(8, 0xff404040u));
    WriteRaw(dir / "motion_vectors.rg32f.raw", std::vector<Float2> {{0.0f, 0.0f}, {3.0f, 4.0f}, {0.0f, 0.02f}, {0.0f, 0.0f}});
    WriteRaw(dir / "reactive_mask.r32f.raw", std::vector<float> {0.0f, 1.0f, 0.0f, 0.0f});

    const auto analysis = osr::debug::AnalyzeCaptureFrame(dir);
    if (!analysis.ok) {
        return Fail("capture analysis should parse synthetic artifacts");
    }
    if (analysis.display_size.width != 4 || analysis.display_size.height != 2 ||
        analysis.render_size.width != 2 || analysis.render_size.height != 2) {
        return Fail("capture analysis dimensions mismatch");
    }
    if (analysis.frame_id != 12) {
        return Fail("capture analysis frame id mismatch");
    }
    if (analysis.history_weight.over_threshold_pct != 25.0) {
        return Fail("history trusted percentage mismatch");
    }
    if (analysis.color_residual.over_threshold_pct != 37.5) {
        return Fail("color residual threshold percentage mismatch");
    }
    if (analysis.depth_residual.over_threshold_pct != 25.0) {
        return Fail("depth residual threshold percentage mismatch");
    }
    if (analysis.feature_lock_strength.over_threshold_pct != 37.5) {
        return Fail("feature lock active percentage mismatch");
    }
    if (analysis.motion_magnitude.max < 4.99 || analysis.motion_magnitude.max > 5.01) {
        return Fail("motion magnitude max mismatch");
    }
    if (analysis.motion_region_history_trusted_pct != 50.0 ||
        analysis.static_region_history_trusted_pct != 0.0) {
        return Fail("motion/static history trust split mismatch");
    }
    if (analysis.reactive_region.samples != 2 || analysis.reactive_region.history_trusted_pct != 50.0) {
        return Fail("reactive ROI remap mismatch");
    }
    const std::string summary = osr::debug::SummarizeCaptureAnalysis(analysis);
    if (summary.find("history_trusted_pct=25") == std::string::npos ||
        summary.find("motion_active_pct=50") == std::string::npos ||
        summary.find("motion_history_trusted_pct=50") == std::string::npos ||
        summary.find("feature_lock_active_pct=37.5") == std::string::npos) {
        return Fail("capture analysis summary missing expected metrics");
    }

    const std::filesystem::path roi_dir = "build/manual/capture_analysis_tests/frame_000012";
    std::filesystem::create_directories(roi_dir);
    {
        std::ofstream manifest(roi_dir / "artifacts.json", std::ios::trunc);
        manifest << "{\n";
        manifest << "  \"resources\": [\n";
        manifest << "    {\"name\":\"motion_vectors\",\"raw\":\"motion_vectors.rg32f.raw\",\"format\":\"rg32f\",\"width\":50,\"height\":50},\n";
        manifest << "    {\"name\":\"color_output\",\"raw\":\"color_output.rgba8.raw\",\"format\":\"rgba8\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"spatial_baseline\",\"raw\":\"spatial_baseline.rgba8.raw\",\"format\":\"rgba8\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"native_reference\",\"raw\":\"native_reference.rgba8.raw\",\"format\":\"rgba8\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"history_weight\",\"raw\":\"history_weight.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"color_residual\",\"raw\":\"color_residual.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"depth_residual\",\"raw\":\"depth_residual.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"feature_lock_strength\",\"raw\":\"feature_lock_strength.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"reactive_mask\",\"raw\":\"reactive_mask.r32f.raw\",\"format\":\"r32f\",\"width\":50,\"height\":50}\n";
        manifest << "  ]\n";
        manifest << "}\n";
    }
    {
        std::ofstream context(roi_dir / "frame_context.json", std::ios::trunc);
        context << "{\n  \"frame_id\": 12\n}\n";
    }
    std::vector<float> roi_history(10000, 0.8f);
    std::vector<float> roi_locks(10000, 0.0f);
    std::vector<uint32_t> roi_output(10000, 0xff404040u);
    std::vector<uint32_t> roi_spatial(10000, 0xff404040u);
    std::vector<uint32_t> roi_native(10000, 0xff404040u);
    for (uint32_t y = 0; y < 100; ++y) {
        for (uint32_t x = 0; x < 100; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / 100.0f;
            const float v = (static_cast<float>(y) + 0.5f) / 100.0f;
            const auto text = osr::demo::wind_tunnel::EvaluateSyntheticTextCoverage(u, v, 12, true);
            const auto material = osr::demo::wind_tunnel::EvaluateSyntheticMaterialCoverage(u, v, 12, true);
            if (text.glyph) {
                roi_locks[static_cast<size_t>(y) * 100 + x] = 0.75f;
                roi_output[static_cast<size_t>(y) * 100 + x] = 0xffffffffu;
                roi_spatial[static_cast<size_t>(y) * 100 + x] = 0xffc0c0c0u;
                roi_native[static_cast<size_t>(y) * 100 + x] = 0xffffffffu;
            }
            if (material.specular || material.transparent) {
                roi_history[static_cast<size_t>(y) * 100 + x] = 0.0f;
            }
        }
    }
    WriteRaw(roi_dir / "history_weight.r32f.raw", roi_history);
    WriteRaw(roi_dir / "color_residual.r32f.raw", std::vector<float>(10000, 0.05f));
    WriteRaw(roi_dir / "depth_residual.r32f.raw", std::vector<float>(10000, 0.01f));
    WriteRaw(roi_dir / "feature_lock_strength.r32f.raw", roi_locks);
    WriteRaw(roi_dir / "color_output.rgba8.raw", roi_output);
    WriteRaw(roi_dir / "spatial_baseline.rgba8.raw", roi_spatial);
    WriteRaw(roi_dir / "native_reference.rgba8.raw", roi_native);
    WriteRaw(roi_dir / "motion_vectors.rg32f.raw", std::vector<Float2>(2500, {0.0f, 0.0f}));
    WriteRaw(roi_dir / "reactive_mask.r32f.raw", std::vector<float>(2500, 0.0f));
    const auto roi_analysis = osr::debug::AnalyzeCaptureFrame(roi_dir);
    if (!roi_analysis.ok) {
        return Fail("ROI capture analysis should parse synthetic artifacts");
    }
    if (roi_analysis.text_region.samples == 0 ||
        roi_analysis.static_text_region.samples == 0 ||
        roi_analysis.moving_text_region.samples == 0 ||
        roi_analysis.specular_region.samples == 0 ||
        roi_analysis.transparent_region.samples == 0 ||
        roi_analysis.reactive_region.samples != 0) {
        return Fail("expected nonzero synthetic ROI samples");
    }
    if (roi_analysis.text_region.history_trusted_pct != 100.0 ||
        roi_analysis.static_text_region.history_trusted_pct != 100.0 ||
        roi_analysis.moving_text_region.history_trusted_pct != 100.0 ||
        roi_analysis.specular_region.history_trusted_pct != 0.0 ||
        roi_analysis.transparent_region.history_trusted_pct != 0.0 ||
        roi_analysis.reactive_region.history_trusted_pct != 0.0) {
        return Fail("synthetic ROI trusted history percentages mismatch");
    }
    if (roi_analysis.text_region.mean_feature_lock < 0.7 ||
        roi_analysis.static_text_region.mean_feature_lock < 0.7 ||
        roi_analysis.moving_text_region.mean_feature_lock < 0.7 ||
        roi_analysis.specular_region.mean_feature_lock != 0.0 ||
        roi_analysis.transparent_region.mean_feature_lock != 0.0) {
        return Fail("synthetic ROI feature-lock metrics mismatch");
    }
    if (roi_analysis.locked_detail.score < 95.0 ||
        roi_analysis.locked_detail.text_output_contrast < 0.7 ||
        roi_analysis.locked_detail.text_contrast_ratio < 1.25 ||
        roi_analysis.locked_detail.text_native_contrast_ratio < 0.95 ||
        roi_analysis.locked_detail.bad_lock_signal != 0.0) {
        return Fail("locked-detail metric should reward stable text without bad locks");
    }
    const auto passing_gate = osr::debug::EvaluateCaptureAnalysisGate(roi_analysis);
    if (!passing_gate.passed) {
        return Fail("ROI capture analysis gate should pass the controlled synthetic fixture");
    }
    auto failing_analysis = roi_analysis;
    failing_analysis.reactive_region.samples = 10;
    failing_analysis.reactive_region.history_trusted_pct = 25.0;
    if (osr::debug::EvaluateCaptureAnalysisGate(failing_analysis).passed) {
        return Fail("ROI capture analysis gate should fail reactive history leaks");
    }
    auto bad_lock_analysis = roi_analysis;
    bad_lock_analysis.locked_detail.bad_lock_signal = 0.20;
    if (osr::debug::EvaluateCaptureAnalysisGate(bad_lock_analysis).passed) {
        return Fail("ROI capture analysis gate should fail bad lock leakage");
    }
    osr::debug::CaptureAnalysisGateThresholds relaxed;
    relaxed.max_reactive_history_trusted_pct = 30.0;
    relaxed.max_bad_lock_signal = 0.30;
    if (!osr::debug::EvaluateCaptureAnalysisGate(failing_analysis, relaxed).passed) {
        return Fail("ROI capture analysis gate should honor custom relaxed thresholds");
    }
    {
        std::ofstream thresholds_file(dir.parent_path() / "thresholds.cfg", std::ios::trunc);
        thresholds_file << "max_reactive_history_trusted_pct = 30\n";
        thresholds_file << "min_static_history_trusted_pct = 80\n";
        thresholds_file << "min_locked_detail_score = 10\n";
        thresholds_file << "max_bad_lock_signal = 0.30\n";
    }
    const auto loaded_thresholds = osr::debug::LoadCaptureAnalysisGateThresholds(dir.parent_path() / "thresholds.cfg");
    if (!osr::debug::EvaluateCaptureAnalysisGate(failing_analysis, loaded_thresholds).passed ||
        !osr::debug::EvaluateCaptureAnalysisGate(bad_lock_analysis, loaded_thresholds).passed) {
        return Fail("ROI capture analysis gate should honor loaded threshold config");
    }
    const auto gate = osr::debug::EvaluateCaptureAnalysisGate(roi_analysis, loaded_thresholds);
    const auto analysis_json = dir.parent_path() / "capture_analysis.json";
    if (!osr::debug::WriteCaptureAnalysisJson(roi_analysis, gate, loaded_thresholds, analysis_json)) {
        return Fail("capture analysis JSON export should succeed");
    }
    if (!Contains(analysis_json, "\"schema\": \"osr.capture.analysis.v1\"") ||
        !Contains(analysis_json, "\"text\": {") ||
        !Contains(analysis_json, "\"static_text\": {") ||
        !Contains(analysis_json, "\"moving_text\": {") ||
        !Contains(analysis_json, "\"locked_detail\": {") ||
        !Contains(analysis_json, "\"text_contrast_ratio\"") ||
        !Contains(analysis_json, "\"text_native_contrast_ratio\"") ||
        !Contains(analysis_json, "\"min_locked_detail_score\":10") ||
        !Contains(analysis_json, "\"max_bad_lock_signal\":0.3") ||
        !Contains(analysis_json, "\"feature_lock_strength\": {") ||
        !Contains(analysis_json, "\"mean_feature_lock\"") ||
        !Contains(analysis_json, "\"max_reactive_history_trusted_pct\":30")) {
        return Fail("capture analysis JSON export missing expected fields");
    }

    const auto missing = osr::debug::AnalyzeCaptureFrame(dir / "missing");
    if (missing.ok || osr::debug::SummarizeCaptureAnalysis(missing).find("capture_analysis_failed") == std::string::npos) {
        return Fail("capture analysis should report missing artifacts");
    }

    return 0;
}
