#include "demo/wind_tunnel/temporal_resolve.h"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

} // namespace

int main() {
    osr::demo::wind_tunnel::SyntheticFrameSettings frame_settings;
    frame_settings.display_size = {4, 4};
    frame_settings.render_scale = 1.0f;
    frame_settings.frame_id = 4;
    frame_settings.jitter_enabled = false;
    auto frame = osr::demo::wind_tunnel::BuildSyntheticFrame(frame_settings);

    const std::vector<uint32_t> current(16, 0xff202020u);
    const std::vector<uint32_t> history(16, 0xffa0a0a0u);
    osr::demo::wind_tunnel::TemporalResolveSettings settings;
    settings.max_history_weight = 0.5f;
    osr::demo::wind_tunnel::TemporalResolveStats stats;
    const auto blended = osr::demo::wind_tunnel::ResolveTemporalDisplay(current, history, frame, frame_settings.display_size, settings, &stats);
    if (blended.size() != current.size()) {
        return Fail("temporal resolve output size mismatch");
    }
    if (blended[0] != 0xff606060u) {
        return Fail("temporal resolve did not blend expected 50/50 color");
    }
    if (stats.history_weight_mean <= 0.0 || stats.history_weight_max > 0.5) {
        return Fail("temporal resolve stats are outside expected range");
    }

    frame.context.flags.reset_history = true;
    const auto reset = osr::demo::wind_tunnel::ResolveTemporalDisplay(current, history, frame, frame_settings.display_size, settings, &stats);
    if (reset != current) {
        return Fail("history reset should return current frame");
    }

    frame.context.flags.reset_history = false;
    std::fill(frame.reactive_mask.begin(), frame.reactive_mask.end(), 1.0f);
    settings.reactive_penalty = 1.0f;
    const auto reactive = osr::demo::wind_tunnel::ResolveTemporalDisplay(current, history, frame, frame_settings.display_size, settings, &stats);
    if (reactive != current) {
        return Fail("full reactive mask should suppress history");
    }

    return 0;
}
