#include "core/frame_context.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <cmath>
#include <iostream>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

bool Approximately(float lhs, float rhs, float epsilon = 0.0001f) {
    return std::abs(lhs - rhs) <= epsilon;
}

bool HasNonZeroMotion(const osr::demo::wind_tunnel::SyntheticFrame& frame) {
    for (const auto& mv : frame.motion_vectors) {
        if (std::abs(mv.x) + std::abs(mv.y) > 0.01f) {
            return true;
        }
    }
    return false;
}

} // namespace

int main() {
    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = {1920, 1200};
    settings.render_scale = 2.0f / 3.0f;
    settings.frame_id = 8;
    settings.reset_history = true;

    const auto frame = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    if (std::string(osr::demo::wind_tunnel::ToString(settings.motion_vector_mode)) != "correct") {
        return Fail("default MV mode should be correct");
    }
    if (frame.context.render_size.width != 1280 || frame.context.render_size.height != 800) {
        return Fail("unexpected render size for quality scale");
    }
    const size_t expected_pixels = 1280ull * 800ull;
    if (frame.color.size() != expected_pixels ||
        frame.depth.size() != expected_pixels ||
        frame.motion_vectors.size() != expected_pixels ||
        frame.reactive_mask.size() != expected_pixels) {
        return Fail("synthetic buffers do not match render size");
    }
    if (!frame.context.flags.reset_history) {
        return Fail("reset flag was not propagated into FrameContext");
    }

    const auto report = osr::core::ValidateFrameContext(frame.context);
    if (report.HasErrors()) {
        return Fail("synthetic FrameContext should validate without errors");
    }

    bool saw_motion = false;
    bool saw_reactive = false;
    bool saw_foreground_depth = false;
    for (size_t i = 0; i < expected_pixels; ++i) {
        const auto mv = frame.motion_vectors[i];
        if (!std::isfinite(mv.x) || !std::isfinite(mv.y) || !std::isfinite(frame.depth[i])) {
            return Fail("synthetic frame contains non-finite values");
        }
        saw_motion = saw_motion || std::abs(mv.x) > 0.01f || std::abs(mv.y) > 0.01f;
        saw_reactive = saw_reactive || frame.reactive_mask[i] > 0.5f;
        saw_foreground_depth = saw_foreground_depth || frame.depth[i] < 0.3f;
    }
    if (!saw_motion) {
        return Fail("synthetic frame did not produce non-zero motion vectors");
    }
    if (!saw_reactive) {
        return Fail("synthetic frame did not produce reactive mask coverage");
    }
    if (!saw_foreground_depth) {
        return Fail("synthetic frame did not produce foreground depth");
    }

    settings.reset_history = false;
    const auto correct = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    if (correct.context.flags.motion_vectors_jittered) {
        return Fail("correct MV mode should not mark motion vectors as jittered");
    }

    settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::Zero;
    const auto zero = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    if (HasNonZeroMotion(zero)) {
        return Fail("zero MV mode should clear all motion vectors");
    }
    if (osr::core::ValidateFrameContext(zero.context).HasErrors()) {
        return Fail("zero MV mode should still produce a valid frame context");
    }

    settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::FlipX;
    const auto flip_x = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    bool checked_flip_x = false;
    for (size_t i = 0; i < correct.motion_vectors.size(); ++i) {
        const auto base = correct.motion_vectors[i];
        if (std::abs(base.x) > 0.01f) {
            if (!Approximately(flip_x.motion_vectors[i].x, -base.x) ||
                !Approximately(flip_x.motion_vectors[i].y, base.y)) {
                return Fail("flip-x MV mode did not invert X only");
            }
            checked_flip_x = true;
            break;
        }
    }
    if (!checked_flip_x) {
        return Fail("flip-x test did not find a non-zero X motion vector");
    }

    settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::FlipY;
    const auto flip_y = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    bool checked_flip_y = false;
    for (size_t i = 0; i < correct.motion_vectors.size(); ++i) {
        const auto base = correct.motion_vectors[i];
        if (std::abs(base.y) > 0.01f) {
            if (!Approximately(flip_y.motion_vectors[i].x, base.x) ||
                !Approximately(flip_y.motion_vectors[i].y, -base.y)) {
                return Fail("flip-y MV mode did not invert Y only");
            }
            checked_flip_y = true;
            break;
        }
    }
    if (!checked_flip_y) {
        return Fail("flip-y test did not find a non-zero Y motion vector");
    }

    settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::HalfScale;
    const auto half = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::DoubleScale;
    const auto twice = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    bool checked_scale = false;
    for (size_t i = 0; i < correct.motion_vectors.size(); ++i) {
        const auto base = correct.motion_vectors[i];
        if (std::abs(base.x) + std::abs(base.y) > 0.01f) {
            if (!Approximately(half.motion_vectors[i].x, base.x * 0.5f) ||
                !Approximately(half.motion_vectors[i].y, base.y * 0.5f) ||
                !Approximately(twice.motion_vectors[i].x, base.x * 2.0f) ||
                !Approximately(twice.motion_vectors[i].y, base.y * 2.0f)) {
                return Fail("scale MV modes did not apply expected factors");
            }
            checked_scale = true;
            break;
        }
    }
    if (!checked_scale) {
        return Fail("scale tests did not find a non-zero motion vector");
    }

    settings.motion_vector_mode = osr::demo::wind_tunnel::MotionVectorMode::JitterContaminated;
    const auto jittered = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
    if (!jittered.context.flags.motion_vectors_jittered) {
        return Fail("jitter-contaminated MV mode should flag jittered motion vectors");
    }
    const auto current_jitter = osr::demo::wind_tunnel::BuildJitterOffset(settings.frame_id, settings.jitter_sequence_length, settings.jitter_enabled);
    const auto previous_jitter = osr::demo::wind_tunnel::BuildJitterOffset(settings.frame_id - 1, settings.jitter_sequence_length, settings.jitter_enabled);
    const float expected_jitter_delta_x = previous_jitter.x - current_jitter.x;
    const float expected_jitter_delta_y = previous_jitter.y - current_jitter.y;
    bool checked_jitter = false;
    for (size_t i = 0; i < correct.motion_vectors.size(); ++i) {
        const auto base = correct.motion_vectors[i];
        if (std::abs(base.x) + std::abs(base.y) > 0.01f) {
            if (!Approximately(jittered.motion_vectors[i].x - base.x, expected_jitter_delta_x) ||
                !Approximately(jittered.motion_vectors[i].y - base.y, expected_jitter_delta_y)) {
                return Fail("jitter-contaminated MV mode did not apply previous-current jitter delta");
            }
            checked_jitter = true;
            break;
        }
    }
    if (!checked_jitter) {
        return Fail("jitter contamination test did not find a non-zero motion vector");
    }

    const auto no_jitter = osr::demo::wind_tunnel::BuildJitterOffset(3, 16, false);
    if (no_jitter.x != 0.0f || no_jitter.y != 0.0f) {
        return Fail("disabled jitter should be zero");
    }
    const auto jitter = osr::demo::wind_tunnel::BuildJitterOffset(3, 16, true);
    if (std::abs(jitter.x) > 0.5f || std::abs(jitter.y) > 0.5f) {
        return Fail("jitter exceeded expected pixel range");
    }

    return 0;
}
