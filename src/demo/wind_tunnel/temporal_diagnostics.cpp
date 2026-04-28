#include "demo/wind_tunnel/temporal_diagnostics.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

namespace osr::demo::wind_tunnel {

namespace {

float Luma(uint32_t rgba) noexcept {
    const float r = static_cast<float>((rgba >> 16) & 0xffu) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>(rgba & 0xffu) / 255.0f;
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
}

double Percent(uint64_t value, uint64_t total) noexcept {
    return total == 0 ? 0.0 : (static_cast<double>(value) * 100.0) / static_cast<double>(total);
}

double Percentile95(std::vector<float>& values) {
    if (values.empty()) {
        return 0.0;
    }
    const size_t index = static_cast<size_t>(std::floor(static_cast<double>(values.size() - 1) * 0.95));
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(index), values.end());
    return values[index];
}

} // namespace

TemporalDiagnostics ComputeTemporalDiagnostics(const SyntheticFrame& previous,
                                               const SyntheticFrame& current,
                                               const TemporalDiagnosticsSettings& settings) {
    TemporalDiagnostics out;
    const auto size = current.context.render_size;
    if (previous.context.render_size.width != size.width ||
        previous.context.render_size.height != size.height ||
        current.color.size() != current.depth.size() ||
        current.color.size() != current.motion_vectors.size() ||
        previous.color.size() != current.color.size() ||
        previous.depth.size() != current.depth.size()) {
        return out;
    }

    std::vector<float> luma_residuals;
    std::vector<float> depth_residuals;
    luma_residuals.reserve(current.color.size());
    depth_residuals.reserve(current.depth.size());

    uint64_t bad_history = 0;
    uint64_t bad_history_trusted = 0;
    uint64_t good_history = 0;
    uint64_t good_history_rejected = 0;
    uint64_t reactive_samples = 0;
    uint64_t reactive_trusted = 0;
    uint64_t disocclusion_samples = 0;
    uint64_t disocclusion_trusted = 0;
    uint64_t agreement = 0;
    double luma_sum = 0.0;
    double depth_sum = 0.0;
    double trust_sum = 0.0;
    double accumulation_sum = 0.0;

    for (uint32_t y = 0; y < size.height; ++y) {
        for (uint32_t x = 0; x < size.width; ++x) {
            const size_t idx = static_cast<size_t>(y) * size.width + x;
            const auto mv = current.motion_vectors[idx];
            const int previous_x = static_cast<int>(std::lround(static_cast<float>(x) + mv.x));
            const int previous_y = static_cast<int>(std::lround(static_cast<float>(y) + mv.y));
            const bool out_of_bounds = previous_x < 0 || previous_y < 0 ||
                                       previous_x >= static_cast<int>(size.width) ||
                                       previous_y >= static_cast<int>(size.height);
            const size_t previous_idx = out_of_bounds
                ? idx
                : static_cast<size_t>(previous_y) * size.width + static_cast<size_t>(previous_x);

            const float luma_delta = std::abs(Luma(current.color[idx]) - Luma(previous.color[previous_idx]));
            const float depth_delta = std::abs(current.depth[idx] - previous.depth[previous_idx]);
            const float reactive = idx < current.reactive_mask.size() ? current.reactive_mask[idx] : 0.0f;
            const bool disoccluded = out_of_bounds || depth_delta > settings.depth_good_threshold;
            const bool should_trust = !out_of_bounds &&
                                      luma_delta <= settings.luma_good_threshold &&
                                      depth_delta <= settings.depth_good_threshold &&
                                      reactive < settings.reactive_threshold;

            reconstruction::TrustFactors factors;
            factors.previous_trust = 1.0f;
            factors.depth_relative_delta = depth_delta;
            factors.motion_length_pixels = std::sqrt(mv.x * mv.x + mv.y * mv.y);
            factors.color_delta_luma = luma_delta;
            factors.reactive_value = reactive;
            factors.disoccluded = disoccluded;
            factors.reset_history = false;
            const auto trust = reconstruction::ComputeTrustField(factors, settings.trust_settings);
            const bool actually_trusted = trust.history_trust >= settings.trust_accept_threshold;

            luma_residuals.push_back(luma_delta);
            depth_residuals.push_back(depth_delta);
            luma_sum += luma_delta;
            depth_sum += depth_delta;
            trust_sum += trust.history_trust;
            accumulation_sum += trust.accumulation_weight;
            ++out.sample_count;

            if (should_trust) {
                ++good_history;
                if (!actually_trusted) {
                    ++good_history_rejected;
                }
            } else {
                ++bad_history;
                if (actually_trusted) {
                    ++bad_history_trusted;
                }
            }
            if (reactive >= settings.reactive_threshold) {
                ++reactive_samples;
                if (actually_trusted) {
                    ++reactive_trusted;
                }
            }
            if (disoccluded) {
                ++disocclusion_samples;
                if (actually_trusted) {
                    ++disocclusion_trusted;
                }
            }
            if (should_trust == actually_trusted) {
                ++agreement;
            }
        }
    }

    out.mv_luma_residual_mean = out.sample_count == 0 ? 0.0 : luma_sum / static_cast<double>(out.sample_count);
    out.mv_luma_residual_p95 = Percentile95(luma_residuals);
    out.mv_depth_residual_mean = out.sample_count == 0 ? 0.0 : depth_sum / static_cast<double>(out.sample_count);
    out.mv_depth_residual_p95 = Percentile95(depth_residuals);
    out.bad_history_trusted_pct = Percent(bad_history_trusted, bad_history);
    out.good_history_rejected_pct = Percent(good_history_rejected, good_history);
    out.reactive_history_trusted_pct = Percent(reactive_trusted, reactive_samples);
    out.disocclusion_history_trusted_pct = Percent(disocclusion_trusted, disocclusion_samples);
    out.trust_evidence_agreement_pct = Percent(agreement, out.sample_count);
    out.history_trust_mean = out.sample_count == 0 ? 0.0 : trust_sum / static_cast<double>(out.sample_count);
    out.accumulation_weight_mean = out.sample_count == 0 ? 0.0 : accumulation_sum / static_cast<double>(out.sample_count);
    return out;
}

} // namespace osr::demo::wind_tunnel
