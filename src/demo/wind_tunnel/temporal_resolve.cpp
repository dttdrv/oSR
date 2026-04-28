#include "demo/wind_tunnel/temporal_resolve.h"

#include <algorithm>
#include <cmath>

namespace osr::demo::wind_tunnel {

namespace {

uint8_t Channel(uint32_t color, uint32_t shift) noexcept {
    return static_cast<uint8_t>((color >> shift) & 0xffu);
}

uint32_t BlendColor(uint32_t current, uint32_t history, float history_weight) noexcept {
    const float current_weight = 1.0f - history_weight;
    const auto blend = [&](uint32_t shift) {
        const float c = static_cast<float>(Channel(current, shift));
        const float h = static_cast<float>(Channel(history, shift));
        return static_cast<uint32_t>(std::clamp(std::round(c * current_weight + h * history_weight), 0.0f, 255.0f));
    };
    return 0xff000000u | (blend(16) << 16) | (blend(8) << 8) | blend(0);
}

double Percent(uint64_t value, uint64_t total) noexcept {
    return total == 0 ? 0.0 : (static_cast<double>(value) * 100.0) / static_cast<double>(total);
}

} // namespace

std::vector<uint32_t> ResolveTemporalDisplay(const std::vector<uint32_t>& current_display,
                                             const std::vector<uint32_t>& previous_history,
                                             const SyntheticFrame& current_frame,
                                             core::Dimensions display_size,
                                             const TemporalResolveSettings& settings,
                                             TemporalResolveStats* stats) {
    const size_t display_pixels = static_cast<size_t>(display_size.width) * display_size.height;
    if (current_display.size() != display_pixels ||
        previous_history.size() != display_pixels ||
        current_frame.context.flags.reset_history ||
        !current_frame.context.render_size.IsValid()) {
        if (stats) {
            *stats = {};
        }
        return current_display;
    }

    std::vector<uint32_t> output(display_pixels);
    const auto render_size = current_frame.context.render_size;
    double weight_sum = 0.0;
    double weight_min = 1.0;
    double weight_max = 0.0;
    uint64_t reactive_suppressed = 0;
    uint64_t motion_suppressed = 0;
    uint64_t reactive_pixels = 0;
    uint64_t motion_pixels = 0;
    double reactive_weight_sum = 0.0;
    double motion_weight_sum = 0.0;

    for (uint32_t y = 0; y < display_size.height; ++y) {
        const uint32_t ry = std::min(render_size.height - 1,
                                     static_cast<uint32_t>((static_cast<uint64_t>(y) * render_size.height) / display_size.height));
        for (uint32_t x = 0; x < display_size.width; ++x) {
            const uint32_t rx = std::min(render_size.width - 1,
                                         static_cast<uint32_t>((static_cast<uint64_t>(x) * render_size.width) / display_size.width));
            const size_t display_index = static_cast<size_t>(y) * display_size.width + x;
            const size_t render_index = static_cast<size_t>(ry) * render_size.width + rx;
            float history_weight = std::clamp(settings.max_history_weight, 0.0f, 1.0f);

            const float reactive = render_index < current_frame.reactive_mask.size() ? current_frame.reactive_mask[render_index] : 0.0f;
            if (reactive > 0.0f) {
                history_weight *= std::clamp(1.0f - reactive * settings.reactive_penalty, 0.0f, 1.0f);
                ++reactive_suppressed;
                ++reactive_pixels;
            }

            bool motion_pixel = false;
            if (render_index < current_frame.motion_vectors.size()) {
                const auto mv = current_frame.motion_vectors[render_index];
                const float motion_length = std::sqrt(mv.x * mv.x + mv.y * mv.y);
                motion_pixel = motion_length > 0.01f;
                if (motion_length > settings.motion_rejection_pixels) {
                    history_weight = 0.0f;
                    ++motion_suppressed;
                } else if (settings.motion_rejection_pixels > 0.0f) {
                    history_weight *= std::clamp(1.0f - motion_length / settings.motion_rejection_pixels, 0.0f, 1.0f);
                }
            }
            if (reactive > 0.0f) {
                reactive_weight_sum += history_weight;
            }
            if (motion_pixel) {
                ++motion_pixels;
                motion_weight_sum += history_weight;
            }

            output[display_index] = BlendColor(current_display[display_index], previous_history[display_index], history_weight);
            weight_sum += history_weight;
            weight_min = std::min(weight_min, static_cast<double>(history_weight));
            weight_max = std::max(weight_max, static_cast<double>(history_weight));
        }
    }

    if (stats) {
        stats->history_weight_mean = display_pixels == 0 ? 0.0 : weight_sum / static_cast<double>(display_pixels);
        stats->history_weight_min = display_pixels == 0 ? 0.0 : weight_min;
        stats->history_weight_max = display_pixels == 0 ? 0.0 : weight_max;
        stats->reactive_suppressed_pct = Percent(reactive_suppressed, display_pixels);
        stats->motion_suppressed_pct = Percent(motion_suppressed, display_pixels);
        stats->reactive_history_weight_mean = reactive_pixels == 0 ? 0.0 : reactive_weight_sum / static_cast<double>(reactive_pixels);
        stats->motion_history_weight_mean = motion_pixels == 0 ? 0.0 : motion_weight_sum / static_cast<double>(motion_pixels);
    }
    return output;
}

} // namespace osr::demo::wind_tunnel
