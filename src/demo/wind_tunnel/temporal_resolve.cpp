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

float ChannelFloat(uint32_t color, uint32_t shift) noexcept {
    return static_cast<float>(Channel(color, shift));
}

struct YCoCg {
    float y = 0.0f;
    float co = 0.0f;
    float cg = 0.0f;
};

YCoCg ToYCoCg(uint32_t color) noexcept {
    const float r = ChannelFloat(color, 16) / 255.0f;
    const float g = ChannelFloat(color, 8) / 255.0f;
    const float b = ChannelFloat(color, 0) / 255.0f;
    return {
        r * 0.25f + g * 0.5f + b * 0.25f,
        r * 0.5f - b * 0.5f,
        -r * 0.25f + g * 0.5f - b * 0.25f,
    };
}

uint32_t FromYCoCg(YCoCg color) noexcept {
    const float r = color.y + color.co - color.cg;
    const float g = color.y + color.cg;
    const float b = color.y - color.co - color.cg;
    const auto pack = [](float value) {
        return static_cast<uint32_t>(std::clamp(std::round(std::clamp(value, 0.0f, 1.0f) * 255.0f), 0.0f, 255.0f));
    };
    return 0xff000000u | (pack(r) << 16) | (pack(g) << 8) | pack(b);
}

uint32_t ApplyDetailRecovery(const std::vector<uint32_t>& current_display,
                             core::Dimensions display_size,
                             uint32_t x,
                             uint32_t y,
                             uint32_t resolved,
                             float history_weight,
                             float reactive,
                             bool disoccluded,
                             const TemporalResolveSettings& settings,
                             float* applied_amount) noexcept {
    if (applied_amount) {
        *applied_amount = 0.0f;
    }
    if (settings.sharpening_amount <= 0.0f || disoccluded ||
        current_display.size() != static_cast<size_t>(display_size.width) * display_size.height) {
        return resolved;
    }

    const float trust_scale = std::lerp(std::clamp(settings.sharpening_low_trust_scale, 0.0f, 1.0f),
                                        1.0f,
                                        std::clamp(history_weight, 0.0f, 1.0f));
    const float reactive_scale = std::lerp(1.0f,
                                           std::clamp(settings.sharpening_reactive_scale, 0.0f, 1.0f),
                                           std::clamp(reactive, 0.0f, 1.0f));
    const float amount = std::clamp(settings.sharpening_amount, 0.0f, 1.0f) * trust_scale * reactive_scale;
    if (amount <= 0.0f) {
        return resolved;
    }

    const auto at = [&](uint32_t sx, uint32_t sy) {
        return current_display[static_cast<size_t>(sy) * display_size.width + sx];
    };
    const uint32_t xl = x == 0 ? x : x - 1;
    const uint32_t xr = std::min(x + 1, display_size.width - 1);
    const uint32_t yu = y == 0 ? y : y - 1;
    const uint32_t yd = std::min(y + 1, display_size.height - 1);
    const uint32_t center = at(x, y);
    const uint32_t left = at(xl, y);
    const uint32_t right = at(xr, y);
    const uint32_t up = at(x, yu);
    const uint32_t down = at(x, yd);
    const auto sharpen_channel = [&](uint32_t shift) {
        const float detail = ChannelFloat(center, shift) -
                             (ChannelFloat(left, shift) +
                              ChannelFloat(right, shift) +
                              ChannelFloat(up, shift) +
                              ChannelFloat(down, shift)) * 0.25f;
        return static_cast<uint32_t>(std::clamp(std::round(ChannelFloat(resolved, shift) + detail * amount), 0.0f, 255.0f));
    };
    if (applied_amount) {
        *applied_amount = amount;
    }
    return 0xff000000u | (sharpen_channel(16) << 16) | (sharpen_channel(8) << 8) | sharpen_channel(0);
}

uint32_t SampleBilinearRgba8(const std::vector<uint32_t>& image,
                             core::Dimensions size,
                             float x,
                             float y) noexcept {
    if (image.size() != static_cast<size_t>(size.width) * size.height || !size.IsValid()) {
        return 0xff000000u;
    }
    x = std::clamp(x, 0.0f, static_cast<float>(size.width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(size.height - 1));
    const auto x0 = static_cast<uint32_t>(std::floor(x));
    const auto y0 = static_cast<uint32_t>(std::floor(y));
    const auto x1 = std::min(x0 + 1, size.width - 1);
    const auto y1 = std::min(y0 + 1, size.height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](uint32_t sx, uint32_t sy) {
        return image[static_cast<size_t>(sy) * size.width + sx];
    };
    const auto sample_channel = [&](uint32_t shift) {
        const float c00 = static_cast<float>(Channel(at(x0, y0), shift));
        const float c10 = static_cast<float>(Channel(at(x1, y0), shift));
        const float c01 = static_cast<float>(Channel(at(x0, y1), shift));
        const float c11 = static_cast<float>(Channel(at(x1, y1), shift));
        const float top = c00 + (c10 - c00) * tx;
        const float bottom = c01 + (c11 - c01) * tx;
        return static_cast<uint32_t>(std::clamp(std::round(top + (bottom - top) * ty), 0.0f, 255.0f));
    };
    return 0xff000000u | (sample_channel(16) << 16) | (sample_channel(8) << 8) | sample_channel(0);
}

uint32_t ClipHistoryToCurrentNeighborhood(const std::vector<uint32_t>& current_display,
                                          core::Dimensions display_size,
                                          uint32_t x,
                                          uint32_t y,
                                          uint32_t history,
                                          float margin) noexcept {
    if (margin <= 0.0f ||
        current_display.size() != static_cast<size_t>(display_size.width) * display_size.height ||
        !display_size.IsValid()) {
        return history;
    }

    const auto at = [&](uint32_t sx, uint32_t sy) {
        return current_display[static_cast<size_t>(sy) * display_size.width + sx];
    };
    const uint32_t xl = x == 0 ? x : x - 1;
    const uint32_t xr = std::min(x + 1, display_size.width - 1);
    const uint32_t yu = y == 0 ? y : y - 1;
    const uint32_t yd = std::min(y + 1, display_size.height - 1);
    const uint32_t samples[5] = {at(x, y), at(xl, y), at(xr, y), at(x, yu), at(x, yd)};
    const float safe_margin = std::clamp(margin, 0.0f, 1.0f);
    YCoCg lo = ToYCoCg(samples[0]);
    YCoCg hi = lo;
    for (uint32_t i = 1; i < 5; ++i) {
        const YCoCg sample = ToYCoCg(samples[i]);
        lo.y = std::min(lo.y, sample.y);
        lo.co = std::min(lo.co, sample.co);
        lo.cg = std::min(lo.cg, sample.cg);
        hi.y = std::max(hi.y, sample.y);
        hi.co = std::max(hi.co, sample.co);
        hi.cg = std::max(hi.cg, sample.cg);
    }
    YCoCg clipped = ToYCoCg(history);
    const auto clamp_component = [safe_margin](float value, float min_value, float max_value) {
        return std::clamp(value, min_value - safe_margin, max_value + safe_margin);
    };
    clipped.y = clamp_component(clipped.y, lo.y, hi.y);
    clipped.co = clamp_component(clipped.co, lo.co, hi.co);
    clipped.cg = clamp_component(clipped.cg, lo.cg, hi.cg);
    return FromYCoCg(clipped);
}

float SampleBilinearFloat(const std::vector<float>& image,
                          core::Dimensions size,
                          float x,
                          float y) noexcept {
    if (image.size() != static_cast<size_t>(size.width) * size.height || !size.IsValid()) {
        return 0.0f;
    }
    x = std::clamp(x, 0.0f, static_cast<float>(size.width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(size.height - 1));
    const auto x0 = static_cast<uint32_t>(std::floor(x));
    const auto y0 = static_cast<uint32_t>(std::floor(y));
    const auto x1 = std::min(x0 + 1, size.width - 1);
    const auto y1 = std::min(y0 + 1, size.height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](uint32_t sx, uint32_t sy) {
        return image[static_cast<size_t>(sy) * size.width + sx];
    };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return top + (bottom - top) * ty;
}

float Luma(uint32_t color) noexcept {
    const float r = static_cast<float>(Channel(color, 16)) / 255.0f;
    const float g = static_cast<float>(Channel(color, 8)) / 255.0f;
    const float b = static_cast<float>(Channel(color, 0)) / 255.0f;
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
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
                                             TemporalResolveStats* stats,
                                             const SyntheticFrame* previous_frame,
                                             TemporalResolveDebugMaps* debug_maps) {
    const size_t display_pixels = static_cast<size_t>(display_size.width) * display_size.height;
    if (current_display.size() != display_pixels ||
        previous_history.size() != display_pixels ||
        current_frame.context.flags.reset_history ||
        !current_frame.context.render_size.IsValid()) {
        if (stats) {
            *stats = {};
        }
        if (debug_maps) {
            *debug_maps = {};
        }
        return current_display;
    }

    std::vector<uint32_t> output(display_pixels);
    if (debug_maps) {
        debug_maps->display_size = display_size;
        debug_maps->history_weight.assign(display_pixels, 0.0f);
        debug_maps->color_residual.assign(display_pixels, 0.0f);
        debug_maps->depth_residual.assign(display_pixels, 0.0f);
    }
    const auto render_size = current_frame.context.render_size;
    double weight_sum = 0.0;
    double weight_min = 1.0;
    double weight_max = 0.0;
    uint64_t reactive_suppressed = 0;
    uint64_t motion_suppressed = 0;
    uint64_t reactive_pixels = 0;
    uint64_t motion_pixels = 0;
    uint64_t reprojected_pixels = 0;
    uint64_t reproject_out_of_bounds = 0;
    uint64_t color_rejected = 0;
    uint64_t depth_rejected = 0;
    uint64_t depth_samples = 0;
    double reactive_weight_sum = 0.0;
    double motion_weight_sum = 0.0;
    double color_residual_sum = 0.0;
    double depth_residual_sum = 0.0;
    double sharpening_amount_sum = 0.0;
    const float display_per_render_x = static_cast<float>(display_size.width) / static_cast<float>(render_size.width);
    const float display_per_render_y = static_cast<float>(display_size.height) / static_cast<float>(render_size.height);
    const bool has_previous_depth = previous_frame &&
                                    previous_frame->context.render_size.width == render_size.width &&
                                    previous_frame->context.render_size.height == render_size.height &&
                                    previous_frame->depth.size() == current_frame.depth.size() &&
                                    current_frame.depth.size() == static_cast<size_t>(render_size.width) * render_size.height;

    for (uint32_t y = 0; y < display_size.height; ++y) {
        const uint32_t ry = std::min(render_size.height - 1,
                                     static_cast<uint32_t>((static_cast<uint64_t>(y) * render_size.height) / display_size.height));
        for (uint32_t x = 0; x < display_size.width; ++x) {
            const uint32_t rx = std::min(render_size.width - 1,
                                         static_cast<uint32_t>((static_cast<uint64_t>(x) * render_size.width) / display_size.width));
            const size_t display_index = static_cast<size_t>(y) * display_size.width + x;
            const size_t render_index = static_cast<size_t>(ry) * render_size.width + rx;
            uint32_t history_sample = previous_history[display_index];
            float previous_depth_sample = 0.0f;
            float history_weight = std::clamp(settings.max_history_weight, 0.0f, 1.0f);
            bool previous_depth_oob = false;
            bool disoccluded = false;

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
                if (motion_pixel) {
                    const float hx = static_cast<float>(x) + mv.x * display_per_render_x;
                    const float hy = static_cast<float>(y) + mv.y * display_per_render_y;
                    if (hx < 0.0f || hy < 0.0f || hx > static_cast<float>(display_size.width - 1) || hy > static_cast<float>(display_size.height - 1)) {
                        history_weight = 0.0f;
                        ++reproject_out_of_bounds;
                    } else {
                        history_sample = SampleBilinearRgba8(previous_history, display_size, hx, hy);
                        ++reprojected_pixels;
                    }
                    const float prx = static_cast<float>(rx) + mv.x;
                    const float pry = static_cast<float>(ry) + mv.y;
                    if (prx < 0.0f || pry < 0.0f || prx > static_cast<float>(render_size.width - 1) || pry > static_cast<float>(render_size.height - 1)) {
                        history_weight = 0.0f;
                        previous_depth_oob = true;
                    } else {
                        previous_depth_sample = SampleBilinearFloat(previous_frame ? previous_frame->depth : current_frame.depth, render_size, prx, pry);
                    }
                }
                if (motion_length > settings.motion_rejection_pixels) {
                    history_weight = 0.0f;
                    ++motion_suppressed;
                } else if (settings.motion_rejection_pixels > 0.0f) {
                    history_weight *= std::clamp(1.0f - motion_length / settings.motion_rejection_pixels, 0.0f, 1.0f);
                }
            }
            if (motion_pixel) {
                ++motion_pixels;
            }

            if (!motion_pixel && has_previous_depth) {
                previous_depth_sample = previous_frame->depth[render_index];
            }

            history_sample = ClipHistoryToCurrentNeighborhood(current_display,
                                                              display_size,
                                                              x,
                                                              y,
                                                              history_sample,
                                                              settings.history_clip_margin);
            const float color_residual = std::abs(Luma(current_display[display_index]) - Luma(history_sample));
            color_residual_sum += color_residual;
            if (settings.color_rejection_threshold > 0.0f && color_residual > settings.color_rejection_threshold) {
                history_weight = 0.0f;
                ++color_rejected;
            } else if (settings.color_rejection_threshold > 0.0f) {
                history_weight *= std::clamp(1.0f - color_residual / settings.color_rejection_threshold, 0.0f, 1.0f);
            }
            if (has_previous_depth) {
                if (previous_depth_oob) {
                    history_weight = 0.0f;
                    disoccluded = true;
                    if (debug_maps) {
                        debug_maps->depth_residual[display_index] = 1.0f;
                    }
                    ++depth_rejected;
                } else {
                    const float depth_residual = std::abs(current_frame.depth[render_index] - previous_depth_sample);
                    if (debug_maps) {
                        debug_maps->depth_residual[display_index] = depth_residual;
                    }
                    depth_residual_sum += depth_residual;
                    ++depth_samples;
                    if (settings.depth_rejection_threshold > 0.0f && depth_residual > settings.depth_rejection_threshold) {
                        history_weight = 0.0f;
                        disoccluded = true;
                        ++depth_rejected;
                    } else if (settings.depth_rejection_threshold > 0.0f) {
                        history_weight *= std::clamp(1.0f - depth_residual / settings.depth_rejection_threshold, 0.0f, 1.0f);
                    }
                }
            }
            if (reactive > 0.0f) {
                reactive_weight_sum += history_weight;
            }
            if (motion_pixel) {
                motion_weight_sum += history_weight;
            }

            if (debug_maps) {
                debug_maps->history_weight[display_index] = history_weight;
                debug_maps->color_residual[display_index] = color_residual;
            }
            const uint32_t blended = BlendColor(current_display[display_index], history_sample, history_weight);
            float sharpening_amount = 0.0f;
            output[display_index] = ApplyDetailRecovery(current_display,
                                                        display_size,
                                                        x,
                                                        y,
                                                        blended,
                                                        history_weight,
                                                        reactive,
                                                        disoccluded,
                                                        settings,
                                                        &sharpening_amount);
            sharpening_amount_sum += sharpening_amount;
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
        stats->reprojected_history_pct = Percent(reprojected_pixels, display_pixels);
        stats->reproject_out_of_bounds_pct = Percent(reproject_out_of_bounds, display_pixels);
        stats->color_rejected_pct = Percent(color_rejected, display_pixels);
        stats->color_residual_mean = display_pixels == 0 ? 0.0 : color_residual_sum / static_cast<double>(display_pixels);
        stats->depth_rejected_pct = Percent(depth_rejected, display_pixels);
        stats->depth_residual_mean = depth_samples == 0 ? 0.0 : depth_residual_sum / static_cast<double>(depth_samples);
        stats->sharpening_amount_mean = display_pixels == 0 ? 0.0 : sharpening_amount_sum / static_cast<double>(display_pixels);
    }
    return output;
}

} // namespace osr::demo::wind_tunnel
