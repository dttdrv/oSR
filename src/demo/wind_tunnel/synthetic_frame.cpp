#include "demo/wind_tunnel/synthetic_frame.h"

#include "core/quality_mode.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <string>

namespace osr::demo::wind_tunnel {

namespace {

uint8_t ClampByte(float value) noexcept {
    return static_cast<uint8_t>(std::clamp(value, 0.0f, 255.0f));
}

uint32_t Color(float r, float g, float b) noexcept {
    return 0xff000000u | (static_cast<uint32_t>(ClampByte(r)) << 16) |
           (static_cast<uint32_t>(ClampByte(g)) << 8) | static_cast<uint32_t>(ClampByte(b));
}

float Saturate(float value) noexcept {
    return std::clamp(value, 0.0f, 1.0f);
}

core::ResourceDesc Resource(core::ResourceKind kind,
                            void* native_resource,
                            uint64_t debug_id,
                            core::Dimensions extent,
                            const char* name) {
    return {kind, native_resource, debug_id, extent, 0, name};
}

Float2Buffer ApplyMotionVectorMode(Float2Buffer mv, MotionVectorMode mode, core::Float2 jitter_delta) noexcept {
    switch (mode) {
    case MotionVectorMode::Correct:
        return mv;
    case MotionVectorMode::Zero:
        return {};
    case MotionVectorMode::FlipX:
        mv.x = -mv.x;
        return mv;
    case MotionVectorMode::FlipY:
        mv.y = -mv.y;
        return mv;
    case MotionVectorMode::HalfScale:
        mv.x *= 0.5f;
        mv.y *= 0.5f;
        return mv;
    case MotionVectorMode::DoubleScale:
        mv.x *= 2.0f;
        mv.y *= 2.0f;
        return mv;
    case MotionVectorMode::JitterContaminated:
        mv.x += jitter_delta.x;
        mv.y += jitter_delta.y;
        return mv;
    }
    return mv;
}

bool GlyphBit(char glyph, int col, int row) noexcept {
    if (col < 0 || col >= 5 || row < 0 || row >= 7) {
        return false;
    }
    const char* bits = nullptr;
    switch (glyph) {
    case '0': bits = "11110"
                     "10010"
                     "10010"
                     "10010"
                     "10010"
                     "10010"
                     "11110"; break;
    case '6': bits = "01110"
                     "10000"
                     "10000"
                     "11110"
                     "10010"
                     "10010"
                     "01110"; break;
    case '7': bits = "11110"
                     "00010"
                     "00100"
                     "00100"
                     "01000"
                     "01000"
                     "01000"; break;
    case 'M': bits = "10001"
                     "11011"
                     "10101"
                     "10101"
                     "10001"
                     "10001"
                     "10001"; break;
    case 'O': bits = "01110"
                     "10001"
                     "10001"
                     "10001"
                     "10001"
                     "10001"
                     "01110"; break;
    case 'R': bits = "11110"
                     "10001"
                     "10001"
                     "11110"
                     "10100"
                     "10010"
                     "10001"; break;
    case 'S': bits = "01111"
                     "10000"
                     "10000"
                     "01110"
                     "00001"
                     "00001"
                     "11110"; break;
    case 'T': bits = "11111"
                     "00100"
                     "00100"
                     "00100"
                     "00100"
                     "00100"
                     "00100"; break;
    default:
        return false;
    }
    return bits[row * 5 + col] == '1';
}

bool TextGlyphAt(float u, float v, float left, float top, float glyph_height, const char* text) noexcept {
    constexpr int glyph_w = 5;
    constexpr int glyph_h = 7;
    constexpr int gap = 1;
    const float cell = glyph_height / static_cast<float>(glyph_h);
    const float total_width = static_cast<float>(std::max(0, static_cast<int>(std::strlen(text))) * (glyph_w + gap) - gap) * cell;
    if (u < left || v < top || u >= left + total_width || v >= top + glyph_height) {
        return false;
    }
    const int cell_x = static_cast<int>((u - left) / cell);
    const int cell_y = static_cast<int>((v - top) / cell);
    const int advance = glyph_w + gap;
    const int glyph_index = cell_x / advance;
    const int col = cell_x - glyph_index * advance;
    if (col >= glyph_w || glyph_index < 0 || glyph_index >= static_cast<int>(std::strlen(text))) {
        return false;
    }
    return GlyphBit(text[glyph_index], col, cell_y);
}

bool TextPanelAt(float u, float v, float left, float top, float glyph_height, const char* text) noexcept {
    constexpr int glyph_w = 5;
    constexpr int glyph_h = 7;
    constexpr int gap = 1;
    const float cell = glyph_height / static_cast<float>(glyph_h);
    const float total_width = static_cast<float>(std::max(0, static_cast<int>(std::strlen(text))) * (glyph_w + gap) - gap) * cell;
    const float pad = cell * 1.2f;
    return u >= left - pad && u < left + total_width + pad &&
           v >= top - pad && v < top + glyph_height + pad;
}

} // namespace

float Halton(uint32_t index, uint32_t base) noexcept {
    float f = 1.0f;
    float r = 0.0f;
    uint32_t i = index;
    while (i > 0) {
        f /= static_cast<float>(base);
        r += f * static_cast<float>(i % base);
        i /= base;
    }
    return r;
}

const char* ToString(MotionVectorMode mode) noexcept {
    switch (mode) {
    case MotionVectorMode::Correct:
        return "correct";
    case MotionVectorMode::Zero:
        return "zero";
    case MotionVectorMode::FlipX:
        return "flip-x";
    case MotionVectorMode::FlipY:
        return "flip-y";
    case MotionVectorMode::HalfScale:
        return "half-scale";
    case MotionVectorMode::DoubleScale:
        return "double-scale";
    case MotionVectorMode::JitterContaminated:
        return "jitter-contaminated";
    }
    return "unknown";
}

core::Dimensions BuildRenderSize(core::Dimensions display_size, float render_scale) noexcept {
    const float scale = core::ClampRenderScale(render_scale);
    return {
        static_cast<uint32_t>(std::max(1.0f, std::round(static_cast<float>(display_size.width) * scale))),
        static_cast<uint32_t>(std::max(1.0f, std::round(static_cast<float>(display_size.height) * scale)))
    };
}

core::Float2 BuildJitterOffset(uint64_t frame_id, uint32_t sequence_length, bool enabled) noexcept {
    if (!enabled) {
        return {};
    }
    const uint32_t length = std::max(1u, sequence_length);
    const uint32_t index = static_cast<uint32_t>(frame_id % length) + 1u;
    return {Halton(index, 2) - 0.5f, Halton(index, 3) - 0.5f};
}

SyntheticTextCoverage EvaluateSyntheticTextCoverage(float u,
                                                    float v,
                                                    uint64_t frame_id,
                                                    bool enabled) noexcept {
    SyntheticTextCoverage coverage;
    if (!enabled) {
        return coverage;
    }
    const float t = static_cast<float>(frame_id) * 0.03125f;
    const float cube_x = 0.5f + std::sin(t * 0.9f) * 0.18f;

    const float moving_left = cube_x - 0.060f;
    const float moving_top = 0.438f;
    constexpr const char* moving_text = "OSR";
    constexpr float moving_height = 0.050f;
    if (TextPanelAt(u, v, moving_left, moving_top, moving_height, moving_text)) {
        coverage.panel = true;
        coverage.moving = true;
    }
    if (TextGlyphAt(u, v, moving_left, moving_top, moving_height, moving_text)) {
        coverage.glyph = true;
        coverage.moving = true;
    }

    constexpr const char* static_text = "760M";
    constexpr float static_left = 0.088f;
    constexpr float static_top = 0.104f;
    constexpr float static_height = 0.060f;
    if (TextPanelAt(u, v, static_left, static_top, static_height, static_text)) {
        coverage.panel = true;
    }
    if (TextGlyphAt(u, v, static_left, static_top, static_height, static_text)) {
        coverage.glyph = true;
    }
    return coverage;
}

SyntheticMaterialCoverage EvaluateSyntheticMaterialCoverage(float u,
                                                           float v,
                                                           uint64_t frame_id,
                                                           bool enabled) noexcept {
    SyntheticMaterialCoverage coverage;
    if (!enabled) {
        return coverage;
    }
    const float t = static_cast<float>(frame_id) * 0.03125f;
    const float glint_x = 0.80f + std::sin(t * 3.1f) * 0.075f;
    const float glint_y = 0.24f + std::cos(t * 2.3f) * 0.040f;
    coverage.specular = std::hypot(u - glint_x, v - glint_y) < 0.010f;

    const bool pane = u > 0.855f && u < 0.925f && v > 0.48f && v < 0.65f;
    const float stripe = std::abs(std::fmod((u - 0.855f) * 10.0f + (v - 0.48f) * 13.0f + t * 0.16f, 1.0f) - 0.5f);
    coverage.transparent = pane && stripe < 0.040f;
    return coverage;
}

SyntheticFrame BuildSyntheticFrame(const SyntheticFrameSettings& settings) {
    SyntheticFrame frame;
    const core::Dimensions render_size = BuildRenderSize(settings.display_size, settings.render_scale);
    const size_t pixel_count = static_cast<size_t>(render_size.width) * render_size.height;
    frame.color.resize(pixel_count);
    frame.depth.resize(pixel_count);
    frame.motion_vectors.resize(pixel_count);
    frame.reactive_mask.resize(pixel_count);

    const core::Float2 jitter = BuildJitterOffset(settings.frame_id, settings.jitter_sequence_length, settings.jitter_enabled);
    const core::Float2 previous_jitter = BuildJitterOffset(settings.frame_id > 0 ? settings.frame_id - 1 : 0,
                                                           settings.jitter_sequence_length,
                                                           settings.jitter_enabled);
    const core::Float2 jitter_delta {
        previous_jitter.x - jitter.x,
        previous_jitter.y - jitter.y
    };
    const float t = static_cast<float>(settings.frame_id) * 0.03125f;
    const float inv_w = 1.0f / static_cast<float>(std::max(1u, render_size.width));
    const float inv_h = 1.0f / static_cast<float>(std::max(1u, render_size.height));
    const float cube_x = 0.5f + std::sin(t * 0.9f) * 0.18f;
    const float previous_cube_x = 0.5f + std::sin((t - 0.03125f) * 0.9f) * 0.18f;
    const float cube_motion_pixels = (previous_cube_x - cube_x) * static_cast<float>(render_size.width);

    for (uint32_t y = 0; y < render_size.height; ++y) {
        for (uint32_t x = 0; x < render_size.width; ++x) {
            const size_t idx = static_cast<size_t>(y) * render_size.width + x;
            const float u = (static_cast<float>(x) + 0.5f + jitter.x) * inv_w;
            const float v = (static_cast<float>(y) + 0.5f + jitter.y) * inv_h;
            const float grid = (std::fmod(std::floor(u * 32.0f) + std::floor(v * 20.0f), 2.0f) == 0.0f) ? 1.0f : 0.0f;
            const bool cube = std::abs(u - cube_x) < 0.09f && std::abs(v - 0.48f) < 0.14f;
            const bool rail = settings.rails_enabled &&
                              ((std::abs(u - 0.27f) < 0.004f || std::abs(u - 0.73f) < 0.004f) &&
                               v > 0.20f && v < 0.82f);
            const float particle_x = 0.5f + std::sin(t * 2.1f) * 0.28f;
            const float particle_y = 0.36f + std::cos(t * 1.7f) * 0.12f;
            const bool particle = settings.particles_enabled &&
                                  std::hypot(u - particle_x, v - particle_y) < 0.018f;

            uint32_t c = Color(20.0f + grid * 16.0f, 23.0f + grid * 18.0f, 29.0f + grid * 22.0f);
            float depth = 0.72f + v * 0.16f;
            Float2Buffer mv {};
            float reactive = 0.0f;

            if (cube) {
                c = Color(148.0f + 50.0f * Saturate(1.0f - v), 188.0f, 166.0f);
                depth = 0.36f;
                mv.x = cube_motion_pixels;
            }
            if (rail) {
                c = Color(228.0f, 211.0f, 145.0f);
                depth = 0.24f;
            }
            if (particle) {
                c = Color(244.0f, 105.0f, 70.0f);
                depth = 0.20f;
                mv.x = std::cos(t * 2.1f) * -0.28f * 2.1f * 0.03125f * static_cast<float>(render_size.width);
                mv.y = std::sin(t * 1.7f) * 0.12f * 1.7f * 0.03125f * static_cast<float>(render_size.height);
                reactive = 1.0f;
            }
            const auto text = EvaluateSyntheticTextCoverage(u, v, settings.frame_id, settings.text_enabled);
            if (text.panel) {
                c = text.moving ? Color(28.0f, 34.0f, 40.0f) : Color(32.0f, 36.0f, 39.0f);
                depth = text.moving ? 0.335f : 0.46f;
                mv.x = text.moving ? cube_motion_pixels : 0.0f;
                mv.y = 0.0f;
                reactive = 0.0f;
            }
            if (text.glyph) {
                c = text.moving ? Color(235.0f, 241.0f, 220.0f) : Color(241.0f, 213.0f, 126.0f);
                depth = text.moving ? 0.325f : 0.455f;
                mv.x = text.moving ? cube_motion_pixels : 0.0f;
                mv.y = 0.0f;
                reactive = 0.0f;
            }
            const auto material = EvaluateSyntheticMaterialCoverage(u, v, settings.frame_id, settings.material_stress_enabled);
            if (material.transparent) {
                const float shimmer = 0.5f + 0.5f * std::sin(t * 1.9f + u * 41.0f + v * 29.0f);
                c = Color(116.0f + shimmer * 24.0f, 188.0f + shimmer * 22.0f, 204.0f + shimmer * 18.0f);
                depth = 0.285f;
                mv = {};
                reactive = 1.0f;
            }
            if (material.specular) {
                c = Color(255.0f, 250.0f, 214.0f);
                depth = 0.215f;
                mv = {};
                reactive = 1.0f;
            }

            frame.color[idx] = c;
            frame.depth[idx] = depth;
            frame.motion_vectors[idx] = ApplyMotionVectorMode(mv, settings.motion_vector_mode, jitter_delta);
            frame.reactive_mask[idx] = reactive;
        }
    }

    frame.context.frame_id = settings.frame_id;
    frame.context.source_api = "oSR.synthetic.wind_tunnel";
    frame.context.render_size = render_size;
    frame.context.display_size = settings.display_size;
    frame.context.jitter_offset = jitter;
    frame.context.motion_vector_scale = {
        static_cast<float>(render_size.width),
        static_cast<float>(render_size.height)
    };
    frame.context.motion_vector_space = core::MotionVectorSpace::Pixel;
    frame.context.color_space = core::ColorSpace::LinearSdr;
    frame.context.flags.reset_history = settings.reset_history;
    frame.context.flags.depth_inverted = false;
    frame.context.flags.motion_vectors_jittered = settings.motion_vector_mode == MotionVectorMode::JitterContaminated;
    frame.context.exposure.exposure_scale = 1.0f;
    frame.context.exposure.pre_exposure = 1.0f;
    frame.context.color_input = Resource(core::ResourceKind::ColorInput, frame.color.data(), 0x1000, render_size, "synthetic_color_input_rgba8");
    frame.context.color_output = Resource(core::ResourceKind::ColorOutput, frame.color.data(), 0x1001, settings.display_size, "synthetic_color_output_placeholder");
    frame.context.depth = Resource(core::ResourceKind::Depth, frame.depth.data(), 0x1002, render_size, "synthetic_depth_f32");
    frame.context.motion_vectors = Resource(core::ResourceKind::MotionVectors, frame.motion_vectors.data(), 0x1003, render_size, "synthetic_motion_vectors_pixel_f32x2");
    frame.context.reactive_mask = Resource(core::ResourceKind::ReactiveMask, frame.reactive_mask.data(), 0x1004, render_size, "synthetic_reactive_mask_f32");
    frame.context.notes.push_back("Synthetic wind-tunnel frame emits color/depth/MV/reactive/reset for SR validation.");
    frame.context.notes.push_back("Motion vectors are current-to-previous in pixel units and exclude jitter.");
    frame.context.notes.push_back(settings.text_enabled
        ? "Synthetic text targets are enabled for readability/edge-preservation metrics."
        : "Synthetic text targets are disabled.");
    frame.context.notes.push_back(settings.material_stress_enabled
        ? "Synthetic specular/transparent material stress targets are enabled."
        : "Synthetic material stress targets are disabled.");
    frame.context.notes.push_back(std::string("Synthetic MV mode: ") + ToString(settings.motion_vector_mode));
    return frame;
}

} // namespace osr::demo::wind_tunnel
