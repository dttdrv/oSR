#include "demo/wind_tunnel/synthetic_frame.h"

#include "core/quality_mode.h"

#include <algorithm>
#include <cmath>
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
    frame.context.notes.push_back(std::string("Synthetic MV mode: ") + ToString(settings.motion_vector_mode));
    return frame;
}

} // namespace osr::demo::wind_tunnel
