#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <vector>

namespace osr::demo::wind_tunnel {

struct SyntheticFrameSettings {
    core::Dimensions display_size {1920, 1200};
    float render_scale = 2.0f / 3.0f;
    uint64_t frame_id = 0;
    bool jitter_enabled = true;
    uint32_t jitter_sequence_length = 16;
    bool reset_history = false;
    bool particles_enabled = true;
    bool rails_enabled = true;
};

struct Float2Buffer {
    float x = 0.0f;
    float y = 0.0f;
};

struct SyntheticFrame {
    core::FrameContext context;
    std::vector<uint32_t> color;
    std::vector<float> depth;
    std::vector<Float2Buffer> motion_vectors;
    std::vector<float> reactive_mask;
};

[[nodiscard]] float Halton(uint32_t index, uint32_t base) noexcept;
[[nodiscard]] core::Dimensions BuildRenderSize(core::Dimensions display_size, float render_scale) noexcept;
[[nodiscard]] core::Float2 BuildJitterOffset(uint64_t frame_id, uint32_t sequence_length, bool enabled) noexcept;
[[nodiscard]] SyntheticFrame BuildSyntheticFrame(const SyntheticFrameSettings& settings);

} // namespace osr::demo::wind_tunnel
