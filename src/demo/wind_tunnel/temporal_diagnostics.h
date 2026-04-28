#pragma once

#include "demo/wind_tunnel/synthetic_frame.h"
#include "reconstruction/trust_field.h"

#include <cstdint>

namespace osr::demo::wind_tunnel {

struct TemporalDiagnosticsSettings {
    reconstruction::TrustFieldSettings trust_settings {};
    float luma_good_threshold = 0.08f;
    float depth_good_threshold = 0.025f;
    float reactive_threshold = 0.5f;
    float trust_accept_threshold = 0.45f;
};

struct TemporalDiagnostics {
    uint64_t sample_count = 0;
    double mv_luma_residual_mean = 0.0;
    double mv_luma_residual_p95 = 0.0;
    double mv_depth_residual_mean = 0.0;
    double mv_depth_residual_p95 = 0.0;
    double bad_history_trusted_pct = 0.0;
    double good_history_rejected_pct = 0.0;
    double reactive_history_trusted_pct = 0.0;
    double disocclusion_history_trusted_pct = 0.0;
    double trust_evidence_agreement_pct = 0.0;
    double history_trust_mean = 0.0;
    double accumulation_weight_mean = 0.0;
};

[[nodiscard]] TemporalDiagnostics ComputeTemporalDiagnostics(const SyntheticFrame& previous,
                                                             const SyntheticFrame& current,
                                                             const TemporalDiagnosticsSettings& settings = {});

} // namespace osr::demo::wind_tunnel
