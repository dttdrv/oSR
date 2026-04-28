#pragma once

#include "core/frame_context.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <cstdint>
#include <filesystem>
#include <vector>

namespace osr::demo::wind_tunnel {

struct DebugDumpResult {
    bool color_input_ppm = false;
    bool depth_pgm = false;
    bool motion_vectors_pgm = false;
    bool reactive_mask_pgm = false;
    bool color_input_raw = false;
    bool depth_raw = false;
    bool motion_vectors_raw = false;
    bool reactive_mask_raw = false;
    bool output_ppm = false;
    bool output_raw = false;
    bool artifacts_json = false;

    [[nodiscard]] bool AllRequired() const noexcept;
};

bool WriteRgbaPpm(const std::filesystem::path& path,
                  const std::vector<uint32_t>& rgba,
                  core::Dimensions extent);
bool WriteFloatPgm(const std::filesystem::path& path,
                   const std::vector<float>& values,
                   core::Dimensions extent,
                   float min_value,
                   float max_value);
bool WriteMotionMagnitudePgm(const std::filesystem::path& path,
                             const std::vector<Float2Buffer>& motion_vectors,
                             core::Dimensions extent);
bool WriteRawBytes(const std::filesystem::path& path, const void* data, size_t size);

DebugDumpResult WriteSyntheticFrameDebugDumps(const std::filesystem::path& frame_dir,
                                              const SyntheticFrame& frame,
                                              const std::vector<uint32_t>& display_output,
                                              uint64_t color_input_hash = 0,
                                              uint64_t color_output_hash = 0,
                                              uint64_t depth_hash = 0,
                                              uint64_t motion_vectors_hash = 0,
                                              uint64_t reactive_mask_hash = 0);

} // namespace osr::demo::wind_tunnel
