#include "demo/wind_tunnel/temporal_diagnostics.h"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

osr::demo::wind_tunnel::SyntheticFrame Frame(uint64_t frame_id, osr::demo::wind_tunnel::MotionVectorMode mode) {
    osr::demo::wind_tunnel::SyntheticFrameSettings settings;
    settings.display_size = {960, 600};
    settings.render_scale = 2.0f / 3.0f;
    settings.frame_id = frame_id;
    settings.motion_vector_mode = mode;
    return osr::demo::wind_tunnel::BuildSyntheticFrame(settings);
}

} // namespace

int main() {
    const auto previous = Frame(7, osr::demo::wind_tunnel::MotionVectorMode::Correct);
    const auto current = Frame(8, osr::demo::wind_tunnel::MotionVectorMode::Correct);
    const auto flipped = Frame(8, osr::demo::wind_tunnel::MotionVectorMode::FlipX);
    const auto zero = Frame(8, osr::demo::wind_tunnel::MotionVectorMode::Zero);

    const auto correct_report = osr::demo::wind_tunnel::ComputeTemporalDiagnostics(previous, current);
    const auto flipped_report = osr::demo::wind_tunnel::ComputeTemporalDiagnostics(previous, flipped);
    const auto zero_report = osr::demo::wind_tunnel::ComputeTemporalDiagnostics(previous, zero);

    if (correct_report.sample_count == 0) {
        return Fail("temporal diagnostics did not inspect samples");
    }
    if (flipped_report.mv_luma_residual_mean <= correct_report.mv_luma_residual_mean) {
        return Fail("flipped X motion should increase luma reprojection residual");
    }
    if (zero_report.trust_evidence_agreement_pct <= 0.0) {
        return Fail("zero MV diagnostics should still produce finite agreement");
    }
    if (correct_report.reactive_history_trusted_pct > 50.0) {
        return Fail("reactive samples should not be broadly trusted");
    }
    if (flipped_report.bad_history_trusted_pct >= 100.0) {
        return Fail("bad history should not be universally trusted");
    }

    return 0;
}
