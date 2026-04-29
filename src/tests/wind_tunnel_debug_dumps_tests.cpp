#include "demo/wind_tunnel/debug_dumps.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

bool HeaderIs(const std::filesystem::path& path, const std::string& header) {
    std::ifstream in(path, std::ios::binary);
    std::string actual(header.size(), '\0');
    in.read(actual.data(), static_cast<std::streamsize>(actual.size()));
    return actual == header;
}

bool FileContains(const std::filesystem::path& path, const std::string& needle) {
    std::ifstream in(path, std::ios::binary);
    const std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return contents.find(needle) != std::string::npos;
}

} // namespace

int main() {
    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = {64, 40};
    settings.render_scale = 0.5f;
    settings.frame_id = 2;
    const auto frame = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    const auto display = frame.color;
    osr::demo::wind_tunnel::TemporalResolveDebugMaps temporal_maps;
    temporal_maps.display_size = settings.display_size;
    temporal_maps.history_weight.assign(static_cast<size_t>(settings.display_size.width) * settings.display_size.height, 0.5f);
    temporal_maps.color_residual.assign(temporal_maps.history_weight.size(), 0.25f);
    temporal_maps.depth_residual.assign(temporal_maps.history_weight.size(), 0.01f);
    temporal_maps.feature_lock_strength.assign(temporal_maps.history_weight.size(), 0.75f);

    const std::filesystem::path dir = "build/manual/debug_dump_tests/frame_000002";
    std::filesystem::remove_all(dir.parent_path());
    const auto result = osr::demo::wind_tunnel::WriteSyntheticFrameDebugDumps(dir, frame, display, 0, 0, 0, 0, 0, &temporal_maps);
    if (!result.color_input_ppm || !result.depth_pgm || !result.motion_vectors_pgm ||
        !result.motion_vectors_x_pgm || !result.motion_vectors_y_pgm || !result.reactive_mask_pgm) {
        return Fail("expected core debug images to be written");
    }
    if (!result.color_input_raw || !result.depth_raw || !result.motion_vectors_raw || !result.reactive_mask_raw) {
        return Fail("expected raw buffers to be written");
    }
    if (!HeaderIs(dir / "color_input.ppm", "P6\n32 20\n255\n")) {
        return Fail("color_input.ppm header mismatch");
    }
    if (!HeaderIs(dir / "depth.pgm", "P5\n32 20\n255\n")) {
        return Fail("depth.pgm header mismatch");
    }
    if (std::filesystem::file_size(dir / "motion_vectors.rg32f.raw") != frame.motion_vectors.size() * sizeof(osr::demo::wind_tunnel::Float2Buffer)) {
        return Fail("motion vector raw size mismatch");
    }
    if (!HeaderIs(dir / "motion_vectors_x.pgm", "P5\n32 20\n255\n") ||
        !HeaderIs(dir / "motion_vectors_y.pgm", "P5\n32 20\n255\n")) {
        return Fail("motion vector component view header mismatch");
    }
    if (!std::filesystem::exists(dir / "artifacts.json")) {
        return Fail("missing artifacts manifest");
    }
    if (!result.history_weight_pgm || !result.color_residual_pgm || !result.depth_residual_pgm ||
        !result.feature_lock_strength_pgm ||
        !result.history_weight_raw || !result.color_residual_raw || !result.depth_residual_raw ||
        !result.feature_lock_strength_raw) {
        return Fail("expected temporal resolve debug maps to be written");
    }
    if (!HeaderIs(dir / "history_weight.pgm", "P5\n64 40\n255\n")) {
        return Fail("history_weight.pgm header mismatch");
    }
    if (result.output_ppm) {
        return Fail("output ppm should fail when display output size does not match display extent");
    }
    if (std::filesystem::file_size(dir / "history_weight.r32f.raw") != temporal_maps.history_weight.size() * sizeof(float)) {
        return Fail("history weight raw size mismatch");
    }
    if (!FileContains(dir / "artifacts.json", "\"raw\":\"history_weight.r32f.raw\"") ||
        !FileContains(dir / "artifacts.json", "\"view_x\":\"motion_vectors_x.pgm\"") ||
        !FileContains(dir / "artifacts.json", "\"name\":\"feature_lock_strength\"")) {
        return Fail("artifacts manifest should reference raw temporal maps and MV component views");
    }

    return 0;
}
