#include "demo/wind_tunnel/debug_dumps.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>

namespace osr::demo::wind_tunnel {

namespace {

uint8_t R(uint32_t c) noexcept { return static_cast<uint8_t>((c >> 16) & 0xff); }
uint8_t G(uint32_t c) noexcept { return static_cast<uint8_t>((c >> 8) & 0xff); }
uint8_t B(uint32_t c) noexcept { return static_cast<uint8_t>(c & 0xff); }

uint8_t FloatToByte(float value, float min_value, float max_value) noexcept {
    const float denom = std::max(0.000001f, max_value - min_value);
    const float normalized = std::clamp((value - min_value) / denom, 0.0f, 1.0f);
    return static_cast<uint8_t>(std::round(normalized * 255.0f));
}

} // namespace

bool DebugDumpResult::AllRequired() const noexcept {
    return color_input_ppm && depth_pgm && motion_vectors_pgm && motion_vectors_x_pgm && motion_vectors_y_pgm && reactive_mask_pgm &&
           color_input_raw && depth_raw && motion_vectors_raw && reactive_mask_raw &&
           output_ppm && output_raw && artifacts_json;
}

bool WriteRgbaPpm(const std::filesystem::path& path,
                  const std::vector<uint32_t>& rgba,
                  core::Dimensions extent) {
    if (rgba.size() < static_cast<size_t>(extent.width) * extent.height) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << "P6\n" << extent.width << " " << extent.height << "\n255\n";
    for (uint32_t y = 0; y < extent.height; ++y) {
        for (uint32_t x = 0; x < extent.width; ++x) {
            const uint32_t c = rgba[static_cast<size_t>(y) * extent.width + x];
            const char rgb[3] = {static_cast<char>(R(c)), static_cast<char>(G(c)), static_cast<char>(B(c))};
            out.write(rgb, sizeof(rgb));
        }
    }
    return true;
}

bool WriteFloatPgm(const std::filesystem::path& path,
                   const std::vector<float>& values,
                   core::Dimensions extent,
                   float min_value,
                   float max_value) {
    if (values.size() < static_cast<size_t>(extent.width) * extent.height) {
        return false;
    }
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out << "P5\n" << extent.width << " " << extent.height << "\n255\n";
    for (uint32_t y = 0; y < extent.height; ++y) {
        for (uint32_t x = 0; x < extent.width; ++x) {
            const uint8_t v = FloatToByte(values[static_cast<size_t>(y) * extent.width + x], min_value, max_value);
            out.write(reinterpret_cast<const char*>(&v), 1);
        }
    }
    return true;
}

bool WriteMotionMagnitudePgm(const std::filesystem::path& path,
                             const std::vector<Float2Buffer>& motion_vectors,
                             core::Dimensions extent) {
    if (motion_vectors.size() < static_cast<size_t>(extent.width) * extent.height) {
        return false;
    }
    std::vector<float> magnitudes(motion_vectors.size());
    float max_magnitude = 0.0f;
    for (size_t i = 0; i < motion_vectors.size(); ++i) {
        const float mag = std::hypot(motion_vectors[i].x, motion_vectors[i].y);
        magnitudes[i] = mag;
        max_magnitude = std::max(max_magnitude, mag);
    }
    return WriteFloatPgm(path, magnitudes, extent, 0.0f, std::max(1.0f, max_magnitude));
}

bool WriteMotionComponentPgm(const std::filesystem::path& path,
                             const std::vector<Float2Buffer>& motion_vectors,
                             core::Dimensions extent,
                             bool x_component) {
    if (motion_vectors.size() < static_cast<size_t>(extent.width) * extent.height) {
        return false;
    }
    std::vector<float> values(motion_vectors.size());
    float max_abs = 1.0f;
    for (size_t i = 0; i < motion_vectors.size(); ++i) {
        values[i] = x_component ? motion_vectors[i].x : motion_vectors[i].y;
        max_abs = std::max(max_abs, std::abs(values[i]));
    }
    return WriteFloatPgm(path, values, extent, -max_abs, max_abs);
}

bool WriteRawBytes(const std::filesystem::path& path, const void* data, size_t size) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        return false;
    }
    out.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    return true;
}

DebugDumpResult WriteSyntheticFrameDebugDumps(const std::filesystem::path& frame_dir,
                                              const SyntheticFrame& frame,
                                              const std::vector<uint32_t>& display_output,
                                              uint64_t color_input_hash,
                                              uint64_t color_output_hash,
                                              uint64_t depth_hash,
                                              uint64_t motion_vectors_hash,
                                              uint64_t reactive_mask_hash,
                                              const TemporalResolveDebugMaps* temporal_debug_maps) {
    std::filesystem::create_directories(frame_dir);
    DebugDumpResult result;
    const auto render_size = frame.context.render_size;
    const auto display_size = frame.context.display_size;

    result.color_input_ppm = WriteRgbaPpm(frame_dir / "color_input.ppm", frame.color, render_size);
    result.depth_pgm = WriteFloatPgm(frame_dir / "depth.pgm", frame.depth, render_size, 0.0f, 1.0f);
    result.motion_vectors_pgm = WriteMotionMagnitudePgm(frame_dir / "motion_vectors_magnitude.pgm", frame.motion_vectors, render_size);
    result.motion_vectors_x_pgm = WriteMotionComponentPgm(frame_dir / "motion_vectors_x.pgm", frame.motion_vectors, render_size, true);
    result.motion_vectors_y_pgm = WriteMotionComponentPgm(frame_dir / "motion_vectors_y.pgm", frame.motion_vectors, render_size, false);
    result.reactive_mask_pgm = WriteFloatPgm(frame_dir / "reactive_mask.pgm", frame.reactive_mask, render_size, 0.0f, 1.0f);
    result.output_ppm = WriteRgbaPpm(frame_dir / "color_output.ppm", display_output, display_size);
    const bool has_temporal_maps = temporal_debug_maps &&
                                   temporal_debug_maps->display_size.width == display_size.width &&
                                   temporal_debug_maps->display_size.height == display_size.height &&
                                   temporal_debug_maps->history_weight.size() == static_cast<size_t>(display_size.width) * display_size.height &&
                                   temporal_debug_maps->color_residual.size() == temporal_debug_maps->history_weight.size() &&
                                   temporal_debug_maps->depth_residual.size() == temporal_debug_maps->history_weight.size() &&
                                   temporal_debug_maps->feature_lock_strength.size() == temporal_debug_maps->history_weight.size();
    if (has_temporal_maps) {
        result.history_weight_pgm = WriteFloatPgm(frame_dir / "history_weight.pgm",
                                                  temporal_debug_maps->history_weight,
                                                  display_size,
                                                  0.0f,
                                                  1.0f);
        result.color_residual_pgm = WriteFloatPgm(frame_dir / "color_residual.pgm",
                                                  temporal_debug_maps->color_residual,
                                                  display_size,
                                                  0.0f,
                                                  1.0f);
        result.depth_residual_pgm = WriteFloatPgm(frame_dir / "depth_residual.pgm",
                                                  temporal_debug_maps->depth_residual,
                                                  display_size,
                                                  0.0f,
                                                  0.1f);
        result.feature_lock_strength_pgm = WriteFloatPgm(frame_dir / "feature_lock_strength.pgm",
                                                         temporal_debug_maps->feature_lock_strength,
                                                         display_size,
                                                         0.0f,
                                                         1.0f);
        result.history_weight_raw = WriteRawBytes(frame_dir / "history_weight.r32f.raw",
                                                  temporal_debug_maps->history_weight.data(),
                                                  temporal_debug_maps->history_weight.size() * sizeof(float));
        result.color_residual_raw = WriteRawBytes(frame_dir / "color_residual.r32f.raw",
                                                  temporal_debug_maps->color_residual.data(),
                                                  temporal_debug_maps->color_residual.size() * sizeof(float));
        result.depth_residual_raw = WriteRawBytes(frame_dir / "depth_residual.r32f.raw",
                                                  temporal_debug_maps->depth_residual.data(),
                                                  temporal_debug_maps->depth_residual.size() * sizeof(float));
        result.feature_lock_strength_raw = WriteRawBytes(frame_dir / "feature_lock_strength.r32f.raw",
                                                         temporal_debug_maps->feature_lock_strength.data(),
                                                         temporal_debug_maps->feature_lock_strength.size() * sizeof(float));
    }

    result.color_input_raw = WriteRawBytes(frame_dir / "color_input.rgba8.raw", frame.color.data(), frame.color.size() * sizeof(uint32_t));
    result.depth_raw = WriteRawBytes(frame_dir / "depth.r32f.raw", frame.depth.data(), frame.depth.size() * sizeof(float));
    result.motion_vectors_raw = WriteRawBytes(frame_dir / "motion_vectors.rg32f.raw", frame.motion_vectors.data(), frame.motion_vectors.size() * sizeof(Float2Buffer));
    result.reactive_mask_raw = WriteRawBytes(frame_dir / "reactive_mask.r32f.raw", frame.reactive_mask.data(), frame.reactive_mask.size() * sizeof(float));
    result.output_raw = WriteRawBytes(frame_dir / "color_output.rgba8.raw", display_output.data(), display_output.size() * sizeof(uint32_t));

    std::ofstream manifest(frame_dir / "artifacts.json", std::ios::trunc);
    if (manifest) {
        manifest << "{\n";
        manifest << "  \"schema\": \"osr.capture.artifacts.v1\",\n";
        manifest << "  \"resources\": [\n";
        manifest << "    {\"name\":\"color_input\",\"view\":\"color_input.ppm\",\"raw\":\"color_input.rgba8.raw\",\"format\":\"rgba8\",\"width\":" << render_size.width << ",\"height\":" << render_size.height << ",\"hash\":" << color_input_hash << "},\n";
        manifest << "    {\"name\":\"color_output\",\"view\":\"color_output.ppm\",\"raw\":\"color_output.rgba8.raw\",\"format\":\"rgba8\",\"width\":" << display_size.width << ",\"height\":" << display_size.height << ",\"hash\":" << color_output_hash << "},\n";
        manifest << "    {\"name\":\"depth\",\"view\":\"depth.pgm\",\"raw\":\"depth.r32f.raw\",\"format\":\"r32f\",\"width\":" << render_size.width << ",\"height\":" << render_size.height << ",\"hash\":" << depth_hash << ",\"view_min\":0,\"view_max\":1},\n";
        manifest << "    {\"name\":\"motion_vectors\",\"view\":\"motion_vectors_magnitude.pgm\",\"view_x\":\"motion_vectors_x.pgm\",\"view_y\":\"motion_vectors_y.pgm\",\"raw\":\"motion_vectors.rg32f.raw\",\"format\":\"rg32f\",\"width\":" << render_size.width << ",\"height\":" << render_size.height << ",\"hash\":" << motion_vectors_hash << "},\n";
        manifest << "    {\"name\":\"reactive_mask\",\"view\":\"reactive_mask.pgm\",\"raw\":\"reactive_mask.r32f.raw\",\"format\":\"r32f\",\"width\":" << render_size.width << ",\"height\":" << render_size.height << ",\"hash\":" << reactive_mask_hash << ",\"view_min\":0,\"view_max\":1}";
        if (has_temporal_maps) {
            manifest << ",\n";
            manifest << "    {\"name\":\"history_weight\",\"view\":\"history_weight.pgm\",\"raw\":\"history_weight.r32f.raw\",\"format\":\"r32f\",\"width\":" << display_size.width << ",\"height\":" << display_size.height << ",\"view_min\":0,\"view_max\":1},\n";
            manifest << "    {\"name\":\"color_residual\",\"view\":\"color_residual.pgm\",\"raw\":\"color_residual.r32f.raw\",\"format\":\"r32f\",\"width\":" << display_size.width << ",\"height\":" << display_size.height << ",\"view_min\":0,\"view_max\":1},\n";
            manifest << "    {\"name\":\"depth_residual\",\"view\":\"depth_residual.pgm\",\"raw\":\"depth_residual.r32f.raw\",\"format\":\"r32f\",\"width\":" << display_size.width << ",\"height\":" << display_size.height << ",\"view_min\":0,\"view_max\":0.1},\n";
            manifest << "    {\"name\":\"feature_lock_strength\",\"view\":\"feature_lock_strength.pgm\",\"raw\":\"feature_lock_strength.r32f.raw\",\"format\":\"r32f\",\"width\":" << display_size.width << ",\"height\":" << display_size.height << ",\"view_min\":0,\"view_max\":1}\n";
        } else {
            manifest << "\n";
        }
        manifest << "  ]\n";
        manifest << "}\n";
        result.artifacts_json = true;
    }

    return result;
}

} // namespace osr::demo::wind_tunnel
