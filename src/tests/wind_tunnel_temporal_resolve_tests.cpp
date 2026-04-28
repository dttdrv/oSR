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
    settings.color_rejection_threshold = 0.0f;
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

    osr::demo::wind_tunnel::SyntheticFrame custom;
    custom.context.render_size = {4, 4};
    custom.context.display_size = {4, 4};
    custom.motion_vectors.resize(16);
    custom.reactive_mask.resize(16, 0.0f);
    std::vector<uint32_t> repro_current(16, 0xff000000u);
    std::vector<uint32_t> repro_history(16, 0xff000000u);
    repro_history[static_cast<size_t>(1) * 4 + 2] = 0xffffffffu;
    settings.max_history_weight = 1.0f;
    settings.motion_rejection_pixels = 1000000.0f;
    custom.motion_vectors[static_cast<size_t>(1) * 4 + 1] = {1.0f, 0.0f};
    const auto repro = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current, repro_history, custom, custom.context.display_size, settings, &stats);
    if (repro[static_cast<size_t>(1) * 4 + 1] != 0xffffffffu) {
        return Fail("positive X motion should sample history to the right");
    }
    if (stats.reprojected_history_pct <= 0.0) {
        return Fail("reprojection stats should count reprojected pixels");
    }

    custom.motion_vectors.assign(16, {});
    custom.reactive_mask.assign(16, 0.0f);
    repro_current.assign(16, 0xff000000u);
    repro_history.assign(16, 0xff000000u);
    repro_history[static_cast<size_t>(1) * 4 + 2] = 0xffffffffu;
    custom.motion_vectors[static_cast<size_t>(1) * 4 + 1] = {0.5f, 0.0f};
    const auto subpixel = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current, repro_history, custom, custom.context.display_size, settings, &stats);
    const uint32_t subpixel_red = (subpixel[static_cast<size_t>(1) * 4 + 1] >> 16) & 0xffu;
    if (subpixel_red < 126u || subpixel_red > 129u) {
        return Fail("subpixel reprojection should bilinearly sample previous history");
    }

    custom.context.render_size = {3, 1};
    custom.context.display_size = {6, 2};
    custom.motion_vectors.assign(3, {});
    custom.reactive_mask.assign(3, 0.0f);
    repro_current.assign(12, 0xff000000u);
    repro_history.assign(12, 0xff000000u);
    repro_history[static_cast<size_t>(1) * 6 + 4] = 0xff00ff00u;
    custom.motion_vectors[static_cast<size_t>(0) * 3 + 1] = {1.0f, 0.0f};
    const auto scaled = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current, repro_history, custom, custom.context.display_size, settings, &stats);
    if (scaled[static_cast<size_t>(1) * 6 + 2] != 0xff00ff00u) {
        return Fail("render-space motion should scale to display-space history lookup");
    }

    custom.context.render_size = {4, 4};
    custom.context.display_size = {4, 4};
    custom.motion_vectors.assign(16, {});
    custom.reactive_mask.assign(16, 0.0f);
    repro_current.assign(16, 0xff101010u);
    repro_history.assign(16, 0xffffffffu);
    custom.motion_vectors[0] = {-4.0f, 0.0f};
    const auto oob = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current, repro_history, custom, custom.context.display_size, settings, &stats);
    if (oob[0] != repro_current[0]) {
        return Fail("out-of-bounds reprojection should fall back to current color");
    }
    if (stats.reproject_out_of_bounds_pct <= 0.0) {
        return Fail("out-of-bounds reprojection should be counted");
    }

    custom.motion_vectors.assign(16, {});
    custom.reactive_mask.assign(16, 0.0f);
    repro_current.assign(16, 0xff000000u);
    repro_history.assign(16, 0xffffffffu);
    settings.color_rejection_threshold = 0.1f;
    const auto rejected = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current, repro_history, custom, custom.context.display_size, settings, &stats);
    if (rejected[5] != repro_current[5]) {
        return Fail("large color residual should reject history");
    }
    if (stats.color_rejected_pct <= 0.0 || stats.color_residual_mean <= 0.0) {
        return Fail("color residual rejection stats should be populated");
    }

    osr::demo::wind_tunnel::SyntheticFrame previous_depth = custom;
    osr::demo::wind_tunnel::SyntheticFrame current_depth = custom;
    previous_depth.depth.assign(16, 0.9f);
    current_depth.depth.assign(16, 0.2f);
    current_depth.motion_vectors.assign(16, {});
    current_depth.reactive_mask.assign(16, 0.0f);
    repro_current.assign(16, 0xff000000u);
    repro_history.assign(16, 0xffffffffu);
    settings.color_rejection_threshold = 0.0f;
    settings.depth_rejection_threshold = 0.1f;
    const auto depth_rejected = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current,
                                                                               repro_history,
                                                                               current_depth,
                                                                               current_depth.context.display_size,
                                                                               settings,
                                                                               &stats,
                                                                               &previous_depth);
    if (depth_rejected[5] != repro_current[5]) {
        return Fail("large depth residual should reject history");
    }
    if (stats.depth_rejected_pct <= 0.0 || stats.depth_residual_mean <= 0.0) {
        return Fail("depth residual rejection stats should be populated");
    }

    previous_depth.depth.assign(16, 0.9f);
    previous_depth.depth[static_cast<size_t>(1) * 4 + 2] = 0.2f;
    current_depth.depth.assign(16, 0.2f);
    current_depth.motion_vectors.assign(16, {});
    current_depth.motion_vectors[static_cast<size_t>(1) * 4 + 1] = {1.0f, 0.0f};
    repro_current.assign(16, 0xff000000u);
    repro_history.assign(16, 0xffffffffu);
    const auto depth_reprojected = osr::demo::wind_tunnel::ResolveTemporalDisplay(repro_current,
                                                                                  repro_history,
                                                                                  current_depth,
                                                                                  current_depth.context.display_size,
                                                                                  settings,
                                                                                  &stats,
                                                                                  &previous_depth);
    if (depth_reprojected[static_cast<size_t>(1) * 4 + 1] != 0xffffffffu) {
        return Fail("depth residual should use reprojected render-space coordinates");
    }

    return 0;
}
