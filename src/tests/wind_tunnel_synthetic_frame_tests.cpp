#include "core/frame_context.h"
#include "demo/wind_tunnel/synthetic_frame.h"

#include <cmath>
#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

} // namespace

int main() {
    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = {1920, 1200};
    settings.render_scale = 2.0f / 3.0f;
    settings.frame_id = 8;
    settings.reset_history = true;

    const auto frame = osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
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
