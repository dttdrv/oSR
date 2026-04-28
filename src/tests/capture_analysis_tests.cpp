#include "debug/capture_analysis.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

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
        manifest << "    {\"name\":\"history_weight\",\"raw\":\"history_weight.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"color_residual\",\"raw\":\"color_residual.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
        manifest << "    {\"name\":\"depth_residual\",\"raw\":\"depth_residual.r32f.raw\",\"format\":\"r32f\",\"width\":4,\"height\":2},\n";
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
        summary.find("motion_history_trusted_pct=50") == std::string::npos) {
        return Fail("capture analysis summary missing expected metrics");
    }

    const std::filesystem::path roi_dir = "build/manual/capture_analysis_tests/frame_000012";
    std::filesystem::create_directories(roi_dir);
    {
        std::ofstream manifest(roi_dir / "artifacts.json", std::ios::trunc);
        manifest << "{\n";
        manifest << "  \"resources\": [\n";
        manifest << "    {\"name\":\"motion_vectors\",\"raw\":\"motion_vectors.rg32f.raw\",\"format\":\"rg32f\",\"width\":50,\"height\":50},\n";
        manifest << "    {\"name\":\"history_weight\",\"raw\":\"history_weight.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"color_residual\",\"raw\":\"color_residual.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"depth_residual\",\"raw\":\"depth_residual.r32f.raw\",\"format\":\"r32f\",\"width\":100,\"height\":100},\n";
        manifest << "    {\"name\":\"reactive_mask\",\"raw\":\"reactive_mask.r32f.raw\",\"format\":\"r32f\",\"width\":50,\"height\":50}\n";
        manifest << "  ]\n";
        manifest << "}\n";
    }
    {
        std::ofstream context(roi_dir / "frame_context.json", std::ios::trunc);
        context << "{\n  \"frame_id\": 12\n}\n";
    }
    WriteRaw(roi_dir / "history_weight.r32f.raw", std::vector<float>(10000, 0.8f));
    WriteRaw(roi_dir / "color_residual.r32f.raw", std::vector<float>(10000, 0.05f));
    WriteRaw(roi_dir / "depth_residual.r32f.raw", std::vector<float>(10000, 0.01f));
    WriteRaw(roi_dir / "motion_vectors.rg32f.raw", std::vector<Float2>(2500, {0.0f, 0.0f}));
    WriteRaw(roi_dir / "reactive_mask.r32f.raw", std::vector<float>(2500, 1.0f));
    const auto roi_analysis = osr::debug::AnalyzeCaptureFrame(roi_dir);
    if (!roi_analysis.ok) {
        return Fail("ROI capture analysis should parse synthetic artifacts");
    }
    if (roi_analysis.text_region.samples == 0 ||
        roi_analysis.specular_region.samples == 0 ||
        roi_analysis.transparent_region.samples == 0 ||
        roi_analysis.reactive_region.samples != 10000) {
        return Fail("expected nonzero synthetic ROI samples");
    }
    if (roi_analysis.text_region.history_trusted_pct != 100.0 ||
        roi_analysis.specular_region.history_trusted_pct != 100.0 ||
        roi_analysis.transparent_region.history_trusted_pct != 100.0 ||
        roi_analysis.reactive_region.history_trusted_pct != 100.0) {
        return Fail("synthetic ROI trusted history percentages mismatch");
    }

    const auto missing = osr::debug::AnalyzeCaptureFrame(dir / "missing");
    if (missing.ok || osr::debug::SummarizeCaptureAnalysis(missing).find("capture_analysis_failed") == std::string::npos) {
        return Fail("capture analysis should report missing artifacts");
    }

    return 0;
}
