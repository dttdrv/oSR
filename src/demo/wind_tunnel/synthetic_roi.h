#pragma once

#include <cstdint>

namespace osr::demo::wind_tunnel {

struct SyntheticTextCoverage {
    bool panel = false;
    bool glyph = false;
    bool moving = false;
};

struct SyntheticMaterialCoverage {
    bool specular = false;
    bool transparent = false;
};

[[nodiscard]] SyntheticTextCoverage EvaluateSyntheticTextCoverage(float u,
                                                                  float v,
                                                                  uint64_t frame_id,
                                                                  bool enabled) noexcept;
[[nodiscard]] SyntheticMaterialCoverage EvaluateSyntheticMaterialCoverage(float u,
                                                                         float v,
                                                                         uint64_t frame_id,
                                                                         bool enabled) noexcept;

} // namespace osr::demo::wind_tunnel
