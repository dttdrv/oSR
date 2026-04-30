#include "core/quality_mode.h"
#include "debug/capture_analysis.h"
#include "debug/capture_pack.h"
#include "demo/dx12_wind_tunnel/display_upscale.h"
#include "demo/wind_tunnel/debug_dumps.h"
#include "demo/wind_tunnel/sequence_metrics.h"
#include "demo/wind_tunnel/synthetic_frame.h"
#include "demo/wind_tunnel/temporal_diagnostics.h"
#include "demo/wind_tunnel/temporal_resolve.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <locale>
#include <sstream>
#include <string>
#include <vector>

namespace {

struct Options {
    osr::core::Dimensions display_size {1280, 800};
    osr::core::QualityMode quality = osr::core::QualityMode::Quality;
    float render_scale = osr::core::DefaultRenderScale(osr::core::QualityMode::Quality);
    bool custom_render_scale = false;
    uint32_t frames = 16;
    uint64_t start_frame = 1;
    int64_t capture_frame = -1;
    std::filesystem::path capture_root = "build/manual/captures";
    std::string capture_run_name = "portable_wind_tunnel";
    std::filesystem::path thresholds_path = "profiles/capture_gate.cfg";
    bool capture = false;
    bool overwrite = false;
    bool metric_gate = false;
    bool jitter = true;
    osr::demo::wind_tunnel::MotionVectorMode mv_mode = osr::demo::wind_tunnel::MotionVectorMode::Correct;
    osr::demo::wind_tunnel::TemporalResolveSettings temporal {};
};

bool ParseDimensions(const std::string& value, osr::core::Dimensions& dimensions) {
    const auto x = value.find('x');
    if (x == std::string::npos || x == 0 || x + 1 >= value.size()) {
        return false;
    }
    const int width = std::atoi(value.substr(0, x).c_str());
    const int height = std::atoi(value.substr(x + 1).c_str());
    if (width <= 0 || height <= 0) {
        return false;
    }
    dimensions = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)};
    return true;
}

bool ParseMvMode(const std::string& value, osr::demo::wind_tunnel::MotionVectorMode& mode) {
    using osr::demo::wind_tunnel::MotionVectorMode;
    if (value == "correct") mode = MotionVectorMode::Correct;
    else if (value == "zero") mode = MotionVectorMode::Zero;
    else if (value == "flip-x") mode = MotionVectorMode::FlipX;
    else if (value == "flip-y") mode = MotionVectorMode::FlipY;
    else if (value == "half-scale") mode = MotionVectorMode::HalfScale;
    else if (value == "double-scale") mode = MotionVectorMode::DoubleScale;
    else if (value == "jitter-contaminated") mode = MotionVectorMode::JitterContaminated;
    else return false;
    return true;
}

bool ParseQuality(const std::string& value, osr::core::QualityMode& mode) {
    using osr::core::QualityMode;
    if (value == "native" || value == "Native") mode = QualityMode::Native;
    else if (value == "ultra-quality-plus" || value == "ultra_quality_plus" || value == "UltraQualityPlus") mode = QualityMode::UltraQualityPlus;
    else if (value == "ultra-quality" || value == "ultra_quality" || value == "UltraQuality") mode = QualityMode::UltraQuality;
    else if (value == "quality" || value == "Quality") mode = QualityMode::Quality;
    else if (value == "balanced" || value == "Balanced") mode = QualityMode::Balanced;
    else if (value == "performance" || value == "Performance") mode = QualityMode::Performance;
    else if (value == "ultra-performance" || value == "ultra_performance" || value == "UltraPerformance") mode = QualityMode::UltraPerformance;
    else return false;
    return true;
}

std::string CommandLine(int argc, char** argv) {
    std::ostringstream out;
    for (int i = 0; i < argc; ++i) {
        if (i > 0) {
            out << ' ';
        }
        const std::string arg = argv[i] ? argv[i] : "";
        const bool quote = arg.find_first_of(" \t\"") != std::string::npos;
        if (!quote) {
            out << arg;
            continue;
        }
        out << '"';
        for (const char c : arg) {
            if (c == '"') {
                out << "\\\"";
            } else {
                out << c;
            }
        }
        out << '"';
    }
    return out.str();
}

void PrintUsage() {
    std::cout
        << "oSR portable wind tunnel\n"
        << "  --frames N\n"
        << "  --display-size WIDTHxHEIGHT\n"
        << "  --quality native|ultra-quality-plus|ultra-quality|quality|balanced|performance|ultra-performance\n"
        << "  --render-scale FLOAT\n"
        << "  --capture-run-name NAME\n"
        << "  --capture-frame N\n"
        << "  --capture-root PATH\n"
        << "  --thresholds PATH\n"
        << "  --metric-gate\n"
        << "  --overwrite\n"
        << "  --mv-mode correct|zero|flip-x|flip-y|half-scale|double-scale|jitter-contaminated\n"
        << "  --no-jitter\n";
}

bool ParseArgs(int argc, char** argv, Options& options) {
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            PrintUsage();
            std::exit(0);
        } else if (arg == "--frames" && i + 1 < argc) {
            options.frames = static_cast<uint32_t>(std::max(0, std::atoi(argv[++i])));
        } else if (arg == "--start-frame" && i + 1 < argc) {
            options.start_frame = static_cast<uint64_t>(std::max(0, std::atoi(argv[++i])));
        } else if (arg == "--display-size" && i + 1 < argc) {
            if (!ParseDimensions(argv[++i], options.display_size)) {
                std::cerr << "Invalid --display-size. Use WIDTHxHEIGHT.\n";
                return false;
            }
        } else if (arg == "--quality" && i + 1 < argc) {
            if (!ParseQuality(argv[++i], options.quality)) {
                std::cerr << "Invalid --quality. Use a named preset, or --render-scale for custom.\n";
                return false;
            }
            options.render_scale = osr::core::DefaultRenderScale(options.quality);
            options.custom_render_scale = false;
        } else if (arg == "--render-scale" && i + 1 < argc) {
            options.render_scale = osr::core::ClampRenderScale(static_cast<float>(std::atof(argv[++i])));
            options.quality = osr::core::QualityMode::Custom;
            options.custom_render_scale = true;
        } else if (arg == "--capture-run-name" && i + 1 < argc) {
            options.capture_run_name = argv[++i];
            options.capture = true;
        } else if (arg == "--capture-frame" && i + 1 < argc) {
            options.capture_frame = std::atoll(argv[++i]);
            options.capture = true;
        } else if (arg == "--capture-root" && i + 1 < argc) {
            options.capture_root = argv[++i];
            options.capture = true;
        } else if (arg == "--thresholds" && i + 1 < argc) {
            options.thresholds_path = argv[++i];
        } else if (arg == "--metric-gate") {
            options.metric_gate = true;
        } else if (arg == "--overwrite") {
            options.overwrite = true;
        } else if (arg == "--no-jitter") {
            options.jitter = false;
        } else if (arg == "--mv-mode" && i + 1 < argc) {
            if (!ParseMvMode(argv[++i], options.mv_mode)) {
                std::cerr << "Invalid --mv-mode.\n";
                return false;
            }
        } else if (arg == "--history-weight" && i + 1 < argc) {
            options.temporal.max_history_weight = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--motion-rejection" && i + 1 < argc) {
            options.temporal.motion_rejection_pixels = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--color-rejection" && i + 1 < argc) {
            options.temporal.color_rejection_threshold = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--depth-rejection" && i + 1 < argc) {
            options.temporal.depth_rejection_threshold = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--history-clip-margin" && i + 1 < argc) {
            options.temporal.history_clip_margin = static_cast<float>(std::atof(argv[++i]));
        } else if (arg == "--sharpening" && i + 1 < argc) {
            options.temporal.sharpening_amount = static_cast<float>(std::atof(argv[++i]));
        } else {
            std::cerr << "Unknown or incomplete argument: " << arg << "\n";
            return false;
        }
    }
    if (!options.display_size.IsValid() || options.frames < 2) {
        std::cerr << "Portable wind tunnel requires a valid display size and at least two frames.\n";
        return false;
    }
    if (options.capture_frame < 0) {
        options.capture_frame = static_cast<int64_t>(options.start_frame + options.frames - 1);
    }
    return true;
}

void CountValidation(const osr::core::ValidationReport& report, uint32_t& errors, uint32_t& warnings) {
    errors = 0;
    warnings = 0;
    for (const auto& message : report.messages) {
        if (message.severity == osr::core::ValidationSeverity::Error) {
            ++errors;
        } else if (message.severity == osr::core::ValidationSeverity::Warning) {
            ++warnings;
        }
    }
}

bool WriteSequenceMetricsCsv(const osr::demo::wind_tunnel::SequenceMetricsResult& result,
                             const std::filesystem::path& path) {
    std::ofstream csv(path, std::ios::trunc);
    if (!csv) {
        return false;
    }
    csv.imbue(std::locale::classic());
    csv << "frames,spatial_frame_delta_mean,temporal_frame_delta_mean,temporal_delta_ratio,"
           "stability_improvement_pct,ghost_score,reactive_trail_score,"
           "edge_preservation,thin_feature_contrast,text_readability_contrast,"
           "specular_history_leak,transparent_history_leak,"
           "reprojected_history_pct,reproject_out_of_bounds_pct,"
           "color_rejected_pct,color_residual_mean,depth_rejected_pct,depth_residual_mean,"
           "sharpening_amount_mean,temporal_history_weight_mean,"
           "temporal_reactive_suppressed_pct,temporal_motion_suppressed_pct\n";
    csv << result.frames << ","
        << result.spatial_frame_delta_mean << ","
        << result.temporal_frame_delta_mean << ","
        << result.temporal_delta_ratio << ","
        << result.stability_improvement_pct << ","
        << result.ghost_score << ","
        << result.reactive_trail_score << ","
        << result.edge_preservation << ","
        << result.thin_feature_contrast << ","
        << result.text_readability_contrast << ","
        << result.specular_history_leak << ","
        << result.transparent_history_leak << ","
        << result.reprojected_history_pct << ","
        << result.reproject_out_of_bounds_pct << ","
        << result.color_rejected_pct << ","
        << result.color_residual_mean << ","
        << result.depth_rejected_pct << ","
        << result.depth_residual_mean << ","
        << result.sharpening_amount_mean << ","
        << result.temporal_history_weight_mean << ","
        << result.temporal_reactive_suppressed_pct << ","
        << result.temporal_motion_suppressed_pct << "\n";
    return true;
}

bool SequenceMetricGatePassed(const osr::demo::wind_tunnel::SequenceMetricsResult& result) {
    return result.temporal_delta_ratio <= 0.80 &&
           result.ghost_score <= 0.45 &&
           result.reactive_trail_score <= 0.12 &&
           result.edge_preservation >= 0.72 &&
           result.thin_feature_contrast >= 0.82 &&
           result.thin_feature_contrast <= 1.45 &&
           result.text_readability_contrast >= 0.72 &&
           result.text_readability_contrast <= 1.35 &&
           result.specular_history_leak <= 0.12 &&
           result.transparent_history_leak <= 0.32;
}

} // namespace

int main(int argc, char** argv) {
    Options options;
    if (!ParseArgs(argc, argv, options)) {
        PrintUsage();
        return 2;
    }

    osr::demo::wind_tunnel::SequenceMetricsSettings sequence_settings;
    sequence_settings.display_size = options.display_size;
    sequence_settings.render_scale = options.render_scale;
    sequence_settings.start_frame = options.start_frame;
    sequence_settings.frame_count = options.frames;
    sequence_settings.temporal_settings = options.temporal;
    const auto sequence = osr::demo::wind_tunnel::RunSequenceMetrics(sequence_settings);
    osr::demo::wind_tunnel::SequenceMetricsSettings gate_sequence_settings;
    gate_sequence_settings.display_size = {320, 200};
    gate_sequence_settings.render_scale = options.render_scale;
    gate_sequence_settings.start_frame = options.start_frame;
    gate_sequence_settings.frame_count = std::min<uint32_t>(std::max<uint32_t>(options.frames, 16), 16);
    gate_sequence_settings.temporal_settings = options.temporal;
    const auto gate_sequence = osr::demo::wind_tunnel::RunSequenceMetrics(gate_sequence_settings);
    const bool sequence_gate_ok = SequenceMetricGatePassed(gate_sequence);

    std::vector<uint32_t> previous_temporal;
    osr::demo::wind_tunnel::SyntheticFrame previous_frame;
    bool has_previous_frame = false;
    osr::debug::CapturePackWriter capture;
    bool capture_started = false;
    std::filesystem::path capture_path;
    osr::debug::CaptureAnalysisGateResult analysis_gate;
    osr::debug::CaptureFrameAnalysis analysis;
    osr::demo::wind_tunnel::TemporalDiagnostics last_diagnostics;
    osr::demo::wind_tunnel::TemporalDiagnosticVerdict last_verdict;
    uint32_t max_diagnostic_severity = 0;

    for (uint32_t i = 0; i < options.frames; ++i) {
        osr::demo::wind_tunnel::SyntheticFrameSettings frame_settings;
        frame_settings.display_size = options.display_size;
        frame_settings.render_scale = options.render_scale;
        frame_settings.frame_id = options.start_frame + i;
        frame_settings.reset_history = i == 0;
        frame_settings.jitter_enabled = options.jitter;
        frame_settings.motion_vector_mode = options.mv_mode;
        auto frame = osr::demo::wind_tunnel::BuildSyntheticFrame(frame_settings);
        auto spatial = osr::demo::dx12_wind_tunnel::UpscaleBilinearJittered(frame.color,
                                                                            frame.context.render_size,
                                                                            frame.context.display_size,
                                                                            frame.context.jitter_offset);
        osr::demo::wind_tunnel::TemporalResolveStats stats;
        osr::demo::wind_tunnel::TemporalResolveDebugMaps debug_maps;
        std::vector<uint32_t> temporal = spatial;
        if (has_previous_frame && !previous_temporal.empty()) {
            last_diagnostics = osr::demo::wind_tunnel::ComputeTemporalDiagnostics(previous_frame, frame);
            last_verdict = osr::demo::wind_tunnel::AnalyzeTemporalDiagnostics(last_diagnostics,
                                                                              options.mv_mode,
                                                                              options.metric_gate);
            max_diagnostic_severity = std::max(max_diagnostic_severity, last_verdict.max_severity);
            temporal = osr::demo::wind_tunnel::ResolveTemporalDisplay(spatial,
                                                                      previous_temporal,
                                                                      frame,
                                                                      frame.context.display_size,
                                                                      options.temporal,
                                                                      &stats,
                                                                      &previous_frame,
                                                                      &debug_maps);
        }

        if (options.capture && static_cast<int64_t>(frame.context.frame_id) == options.capture_frame) {
            osr::debug::CapturePackConfig config;
            config.root = options.capture_root;
            config.run_name = options.capture_run_name;
            config.scenario = "portable_wind_tunnel";
            config.mode = std::string("cpu_temporal_") + osr::demo::wind_tunnel::ToString(options.mv_mode);
            config.algorithm = "cpu_temporal_resolve";
            config.analysis_gate_thresholds_path = options.thresholds_path.string();
            config.analysis_gate_thresholds_snapshot = options.thresholds_path.empty() ? "" : "capture_gate_thresholds.cfg";
            config.overwrite_existing = options.overwrite;
            capture_started = capture.BeginSession(config) &&
                              capture.WriteSessionManifest(frame.context, CommandLine(argc, argv));
            if (!capture_started) {
                std::cerr << "Failed to create capture pack. Use --overwrite or a new --capture-run-name.\n";
                return 4;
            }
            capture_path = capture.SessionPath();
            if (!options.thresholds_path.empty() && std::filesystem::exists(options.thresholds_path)) {
                std::filesystem::copy_file(options.thresholds_path,
                                           capture_path / "capture_gate_thresholds.cfg",
                                           std::filesystem::copy_options::overwrite_existing);
            }
            if (!WriteSequenceMetricsCsv(gate_sequence, capture_path / "sequence_gate_metrics.csv") ||
                !WriteSequenceMetricsCsv(sequence, capture_path / "portable_run_metrics.csv")) {
                std::cerr << "Failed to write sequence_gate_metrics.csv.\n";
                return 4;
            }

            const auto report = osr::core::ValidateFrameContext(frame.context);
            uint32_t validation_errors = 0;
            uint32_t validation_warnings = 0;
            CountValidation(report, validation_errors, validation_warnings);
            osr::debug::HarnessFrameRow row;
            row.frame_id = frame.context.frame_id;
            row.scenario_time_ms = static_cast<double>(i) * frame.context.frame_time_delta_ms;
            row.render_size = frame.context.render_size;
            row.display_size = frame.context.display_size;
            row.jitter = frame.context.jitter_offset;
            row.motion_vector_scale = frame.context.motion_vector_scale;
            row.reset_history = frame.context.flags.reset_history;
            row.validation_errors = validation_errors;
            row.validation_warnings = validation_warnings;
            capture.WriteFrameRow(row);

            osr::debug::HarnessMetricRow metrics;
            metrics.frame_id = frame.context.frame_id;
            metrics.ghost_score = stats.motion_history_weight_mean;
            metrics.reactive_trail_score = stats.reactive_history_weight_mean;
            metrics.edge_preservation = sequence.edge_preservation;
            metrics.thin_feature_contrast = sequence.thin_feature_contrast;
            metrics.text_readability_contrast = sequence.text_readability_contrast;
            metrics.specular_history_leak = osr::demo::wind_tunnel::MeanMaterialHistoryLeak(debug_maps,
                                                                                            frame.context.display_size,
                                                                                            frame.context.frame_id,
                                                                                            true);
            metrics.transparent_history_leak = osr::demo::wind_tunnel::MeanMaterialHistoryLeak(debug_maps,
                                                                                               frame.context.display_size,
                                                                                               frame.context.frame_id,
                                                                                               false);
            metrics.history_reject_pct = 100.0 - stats.history_weight_mean * 100.0;
            metrics.mv_luma_residual_mean = last_diagnostics.mv_luma_residual_mean;
            metrics.mv_luma_residual_p95 = last_diagnostics.mv_luma_residual_p95;
            metrics.mv_depth_residual_mean = last_diagnostics.mv_depth_residual_mean;
            metrics.mv_depth_residual_p95 = last_diagnostics.mv_depth_residual_p95;
            metrics.bad_history_trusted_pct = last_diagnostics.bad_history_trusted_pct;
            metrics.good_history_rejected_pct = last_diagnostics.good_history_rejected_pct;
            metrics.reactive_history_trusted_pct = last_diagnostics.reactive_history_trusted_pct;
            metrics.disocclusion_history_trusted_pct = last_diagnostics.disocclusion_history_trusted_pct;
            metrics.trust_evidence_agreement_pct = last_diagnostics.trust_evidence_agreement_pct;
            metrics.history_trust_mean = stats.history_weight_mean;
            metrics.accumulation_weight_mean = stats.history_weight_mean;
            capture.WriteMetricRow(metrics);
            capture.WriteValidationWarnings(frame.context.frame_id, report);
            for (const auto& finding : last_verdict.findings) {
                capture.WriteDiagnosticWarning(frame.context.frame_id,
                                               finding.tag,
                                               finding.likely_cause,
                                               finding.suggested_action,
                                               finding.severity,
                                               finding.evidence_value);
            }
            capture.WriteFrameContextJson(frame.context);

            std::ostringstream frame_dir_name;
            frame_dir_name << "frame_" << std::setw(6) << std::setfill('0') << frame.context.frame_id;
            auto native_settings = frame_settings;
            native_settings.render_scale = 1.0f;
            const auto native_reference = osr::demo::wind_tunnel::BuildSyntheticFrame(native_settings).color;
            const auto dump = osr::demo::wind_tunnel::WriteSyntheticFrameDebugDumps(capture_path / frame_dir_name.str(),
                                                                                    frame,
                                                                                    temporal,
                                                                                    osr::debug::HashBytes(frame.color.data(), frame.color.size() * sizeof(uint32_t)),
                                                                                    osr::debug::HashBytes(temporal.data(), temporal.size() * sizeof(uint32_t)),
                                                                                    osr::debug::HashBytes(frame.depth.data(), frame.depth.size() * sizeof(float)),
                                                                                    osr::debug::HashBytes(frame.motion_vectors.data(), frame.motion_vectors.size() * sizeof(osr::demo::wind_tunnel::Float2Buffer)),
                                                                                    osr::debug::HashBytes(frame.reactive_mask.data(), frame.reactive_mask.size() * sizeof(float)),
                                                                                    &debug_maps,
                                                                                    &spatial,
                                                                                    &native_reference);
            if (!dump.AllRequired()) {
                std::cerr << "Warning: capture debug dump is incomplete.\n";
            }
            analysis = osr::debug::AnalyzeCaptureFrame(capture_path / frame_dir_name.str());
            const auto thresholds = osr::debug::LoadCaptureAnalysisGateThresholds(options.thresholds_path);
            analysis_gate = osr::debug::EvaluateCaptureAnalysisGate(analysis, thresholds);
            if (!osr::debug::WriteCaptureAnalysisJson(analysis,
                                                      analysis_gate,
                                                      thresholds,
                                                      capture_path / "capture_analysis.json")) {
                std::cerr << "Warning: failed to write capture_analysis.json.\n";
            }
        }

        previous_temporal = std::move(temporal);
        previous_frame = std::move(frame);
        has_previous_frame = true;
    }

    std::filesystem::create_directories("build/manual");
    WriteSequenceMetricsCsv(sequence, "build/manual/osr_portable_wind_tunnel_metrics.csv");

    const auto render_size = osr::demo::wind_tunnel::BuildRenderSize(options.display_size, options.render_scale);
    std::cout << "oSR portable wind tunnel\n";
    std::cout << "Display: " << options.display_size.width << "x" << options.display_size.height
              << "  Render: " << render_size.width << "x" << render_size.height
              << "  Scale: " << options.render_scale << "\n";
    std::cout << "Quality: " << (options.custom_render_scale ? "Custom" : osr::core::ToString(options.quality))
              << "  Frames: " << sequence.frames
              << "  MV: " << osr::demo::wind_tunnel::ToString(options.mv_mode) << "\n";
    std::cout << "Temporal/spatial delta ratio: " << sequence.temporal_delta_ratio
              << "  Stability improvement: " << sequence.stability_improvement_pct << "%\n";
    std::cout << "Gate temporal/spatial delta ratio: " << gate_sequence.temporal_delta_ratio
              << "  Gate stability improvement: " << gate_sequence.stability_improvement_pct << "%\n";
    std::cout << "Thin/text contrast: " << sequence.thin_feature_contrast
              << " / " << sequence.text_readability_contrast
              << "  Material leak spec/trans: " << sequence.specular_history_leak
              << " / " << sequence.transparent_history_leak << "\n";
    std::cout << "Temporal diagnostics: luma_mean=" << last_diagnostics.mv_luma_residual_mean
              << " bad_trusted=" << last_diagnostics.bad_history_trusted_pct
              << "% agreement=" << last_diagnostics.trust_evidence_agreement_pct
              << "% max_severity=" << max_diagnostic_severity << "\n";
    std::cout << "Sequence metrics: build/manual/osr_portable_wind_tunnel_metrics.csv\n";
    if (capture_started) {
        std::cout << "Capture: " << capture_path.string() << "\n";
        std::cout << osr::debug::SummarizeCaptureAnalysis(analysis) << "\n";
        std::cout << "Capture analysis gate: " << (analysis_gate.passed ? "ok" : "FAILED")
                  << " reason=" << analysis_gate.reason << "\n";
    }

    if (options.metric_gate &&
        (!sequence_gate_ok ||
         max_diagnostic_severity >= 2 ||
         (capture_started && !analysis_gate.passed))) {
        std::cerr << "Metric gate failed.\n";
        return 3;
    }
    return 0;
}
