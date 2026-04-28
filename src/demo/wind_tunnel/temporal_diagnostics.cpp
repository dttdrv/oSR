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

float SampleBilinearLuma(const std::vector<uint32_t>& image,
                         core::Dimensions size,
                         float x,
                         float y) noexcept {
    x = std::clamp(x, 0.0f, static_cast<float>(size.width - 1));
    y = std::clamp(y, 0.0f, static_cast<float>(size.height - 1));
    const auto x0 = static_cast<uint32_t>(std::floor(x));
    const auto y0 = static_cast<uint32_t>(std::floor(y));
    const auto x1 = std::min(x0 + 1, size.width - 1);
    const auto y1 = std::min(y0 + 1, size.height - 1);
    const float tx = x - static_cast<float>(x0);
    const float ty = y - static_cast<float>(y0);
    const auto at = [&](uint32_t sx, uint32_t sy) {
        return Luma(image[static_cast<size_t>(sy) * size.width + sx]);
    };
    const float top = at(x0, y0) + (at(x1, y0) - at(x0, y0)) * tx;
    const float bottom = at(x0, y1) + (at(x1, y1) - at(x0, y1)) * tx;
    return top + (bottom - top) * ty;
}

float SampleBilinearDepth(const std::vector<float>& image,
                          core::Dimensions size,
                          float x,
                          float y) noexcept {
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
            const float previous_x = static_cast<float>(x) + mv.x;
            const float previous_y = static_cast<float>(y) + mv.y;
            const bool out_of_bounds = previous_x < 0.0f || previous_y < 0.0f ||
                                       previous_x > static_cast<float>(size.width - 1) ||
                                       previous_y > static_cast<float>(size.height - 1);
            const float previous_luma = out_of_bounds ? Luma(previous.color[idx]) : SampleBilinearLuma(previous.color, size, previous_x, previous_y);
            const float previous_depth = out_of_bounds ? previous.depth[idx] : SampleBilinearDepth(previous.depth, size, previous_x, previous_y);

            const float luma_delta = std::abs(Luma(current.color[idx]) - previous_luma);
            const float depth_delta = std::abs(current.depth[idx] - previous_depth);
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

TemporalDiagnosticVerdict AnalyzeTemporalDiagnostics(const TemporalDiagnostics& diagnostics,
                                                     MotionVectorMode mode,
                                                     bool metric_gate_enabled) {
    TemporalDiagnosticVerdict verdict;
    auto add = [&](std::string tag,
                   std::string likely_cause,
                   std::string suggested_action,
                   uint32_t severity,
                   double evidence_value) {
        verdict.findings.push_back({
            std::move(tag),
            std::move(likely_cause),
            std::move(suggested_action),
            severity,
            evidence_value
        });
        verdict.max_severity = std::max(verdict.max_severity, severity);
    };

    if (mode != MotionVectorMode::Correct) {
        add("MVTruthModeActive",
            "intentional_motion_vector_convention_corruption",
            "Use this run for diagnostic sweeps; metric-gated production baselines should use correct MV mode.",
            2,
            diagnostics.mv_luma_residual_mean);
    }
    if (diagnostics.bad_history_trusted_pct > 1.0) {
        add("BadHistoryTrusted",
            "history_trust_too_permissive",
            "Tighten color/depth/disocclusion evidence before allowing accumulation.",
            3,
            diagnostics.bad_history_trusted_pct);
    }
    if (diagnostics.reactive_history_trusted_pct > 1.0) {
        add("ReactiveHistoryTrusted",
            "reactive_mask_not_suppressing_history",
            "Increase reactive penalty or clamp history weight in responsive regions.",
            3,
            diagnostics.reactive_history_trusted_pct);
    }
    if (diagnostics.disocclusion_history_trusted_pct > 1.0) {
        add("DisocclusionHistoryTrusted",
            "disocclusion_rejection_too_weak",
            "Tighten depth consistency or add foreground-depth dilation near silhouettes.",
            3,
            diagnostics.disocclusion_history_trusted_pct);
    }
    if (diagnostics.good_history_rejected_pct > 8.0) {
        add("GoodHistoryRejected",
            "history_trust_too_conservative",
            "Relax thresholds only where residuals and masks agree, or route stable tiles around rejection.",
            2,
            diagnostics.good_history_rejected_pct);
    }

    const bool corrupted_mv_mode = mode != MotionVectorMode::Correct;
    const double residual_warning_threshold = corrupted_mv_mode ? 0.0035 : 0.0060;
    if (diagnostics.mv_luma_residual_mean > residual_warning_threshold) {
        add("MVResidualHigh",
            corrupted_mv_mode ? "intentional_motion_vector_convention_corruption" : "motion_vector_reprojection_error_high",
            corrupted_mv_mode ? "Expected for MV truth-table mode; verify trust rejects bad history." :
                                "Inspect MV sign, scale, jitter inclusion, and depth reprojection.",
            corrupted_mv_mode ? 1u : 2u,
            diagnostics.mv_luma_residual_mean);
    }
    if (mode == MotionVectorMode::JitterContaminated) {
        add("MVJitterContaminated",
            "motion_vectors_include_jitter",
            "Keep app motion vectors in current-to-previous pixel units excluding jitter.",
            2,
            diagnostics.mv_luma_residual_mean);
    }

    verdict.metric_gate_failed = metric_gate_enabled && verdict.max_severity >= 2;
    return verdict;
}

} // namespace osr::demo::wind_tunnel
