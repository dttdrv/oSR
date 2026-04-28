#include "debug/capture_pack.h"

#include <chrono>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace osr::debug {

namespace {

constexpr const char* kFrameHeader =
    "frame_id,scenario_time_ms,render_w,render_h,display_w,display_h,jitter_x,jitter_y,"
    "mv_scale_x,mv_scale_y,reset_history,gpu_upload_ms,gpu_reconstruct_ms,gpu_present_ms,"
    "cpu_frame_ms,validation_errors,validation_warnings";

constexpr const char* kMetricHeader =
    "frame_id,ghost_score,shimmer_score,disocclusion_leak,reactive_trail_score,"
    "edge_preservation,thin_feature_contrast,text_contrast,text_readability_contrast,"
    "specular_history_leak,transparent_history_leak,"
    "debug_history_weight_max_abs,debug_history_weight_mean_abs,"
    "debug_color_residual_max_abs,debug_color_residual_mean_abs,"
    "debug_depth_residual_max_abs,debug_depth_residual_mean_abs,"
    "history_reject_pct,residual_search_pct,"
    "mv_luma_residual_mean,mv_luma_residual_p95,mv_depth_residual_mean,mv_depth_residual_p95,"
    "bad_history_trusted_pct,good_history_rejected_pct,reactive_history_trusted_pct,"
    "disocclusion_history_trusted_pct,trust_evidence_agreement_pct,history_trust_mean,"
    "accumulation_weight_mean";

std::string TimestampUtc() {
    const auto now = std::chrono::system_clock::now();
    const auto time = std::chrono::system_clock::to_time_t(now);
    std::tm utc {};
#if defined(_WIN32)
    gmtime_s(&utc, &time);
#else
    gmtime_r(&time, &utc);
#endif
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::put_time(&utc, "%Y-%m-%dT%H-%M-%SZ");
    return out.str();
}

std::string RunName(const CapturePackConfig& config) {
    if (!config.run_name.empty()) {
        return config.run_name;
    }
    return TimestampUtc() + "_" + config.scenario + "_" + config.mode;
}

void CountValidation(const core::ValidationReport& report, uint32_t& errors, uint32_t& warnings) {
    errors = 0;
    warnings = 0;
    for (const auto& message : report.messages) {
        if (message.severity == core::ValidationSeverity::Error) {
            ++errors;
        } else if (message.severity == core::ValidationSeverity::Warning) {
            ++warnings;
        }
    }
}

bool WriteText(const std::filesystem::path& path, const std::string& text, std::ios::openmode mode = std::ios::trunc) {
    std::ofstream out(path, mode);
    if (!out) {
        return false;
    }
    out.imbue(std::locale::classic());
    out << text;
    return true;
}

std::string ResourceJson(const char* label, const core::ResourceDesc& resource) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "    \"" << label << "\": {"
        << "\"kind\":\"" << core::ToString(resource.kind) << "\","
        << "\"debug_id\":" << resource.debug_id << ","
        << "\"extent\":[" << resource.extent.width << "," << resource.extent.height << "],"
        << "\"api_format\":" << resource.api_format << ","
        << "\"debug_name\":\"" << JsonEscape(resource.debug_name) << "\""
        << "}";
    return out.str();
}

} // namespace

std::string JsonEscape(std::string_view value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    for (const unsigned char ch : value) {
        switch (ch) {
        case '"': out << "\\\""; break;
        case '\\': out << "\\\\"; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (ch < 0x20) {
                out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch);
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
}

std::string CsvEscape(std::string_view value) {
    bool quote = false;
    for (const char ch : value) {
        quote = quote || ch == ',' || ch == '"' || ch == '\r' || ch == '\n';
    }
    if (!quote) {
        return std::string(value);
    }
    std::string out = "\"";
    for (const char ch : value) {
        if (ch == '"') {
            out += "\"\"";
        } else {
            out += ch;
        }
    }
    out += "\"";
    return out;
}

uint64_t HashBytes(const void* data, size_t size) noexcept {
    constexpr uint64_t kOffset = 14695981039346656037ull;
    constexpr uint64_t kPrime = 1099511628211ull;
    uint64_t hash = kOffset;
    const auto* bytes = static_cast<const uint8_t*>(data);
    for (size_t i = 0; i < size; ++i) {
        hash ^= bytes[i];
        hash *= kPrime;
    }
    return hash;
}

const char* ToString(core::ValidationSeverity severity) noexcept {
    switch (severity) {
    case core::ValidationSeverity::Info: return "Info";
    case core::ValidationSeverity::Warning: return "Warning";
    case core::ValidationSeverity::Error: return "Error";
    }
    return "Unknown";
}

bool CapturePackWriter::BeginSession(const CapturePackConfig& config) {
    config_ = config;
    const auto base_name = RunName(config_);
    session_path_ = config_.root / base_name;
    if (config_.run_name.empty() && std::filesystem::exists(session_path_)) {
        for (uint32_t i = 1; i < 1000 && std::filesystem::exists(session_path_); ++i) {
            std::ostringstream suffix;
            suffix.imbue(std::locale::classic());
            suffix << base_name << "_" << std::setw(3) << std::setfill('0') << i;
            session_path_ = config_.root / suffix.str();
        }
    }
    if (std::filesystem::exists(session_path_)) {
        if (!config_.overwrite_existing) {
            return false;
        }
        std::filesystem::remove_all(session_path_);
    }
    std::filesystem::create_directories(session_path_);
    frames_csv_ = session_path_ / "frames.csv";
    metrics_csv_ = session_path_ / "metrics.csv";
    warnings_jsonl_ = session_path_ / "warnings.jsonl";
    bookmarks_jsonl_ = session_path_ / "bookmarks.jsonl";
    return WriteText(frames_csv_, std::string(kFrameHeader) + "\n") &&
           WriteText(metrics_csv_, std::string(kMetricHeader) + "\n") &&
           WriteText(warnings_jsonl_, "") &&
           WriteText(bookmarks_jsonl_, "");
}

bool CapturePackWriter::WriteSessionManifest(const core::FrameContext& first_frame, const std::string& command_line) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\n";
    out << "  \"schema\": \"osr.capture.session.v1\",\n";
    out << "  \"run_name\": \"" << JsonEscape(session_path_.filename().string()) << "\",\n";
    out << "  \"scenario\": \"" << JsonEscape(config_.scenario) << "\",\n";
    out << "  \"mode\": \"" << JsonEscape(config_.mode) << "\",\n";
    out << "  \"algorithm\": \"" << JsonEscape(config_.algorithm) << "\",\n";
    out << "  \"source_api\": \"" << JsonEscape(first_frame.source_api) << "\",\n";
    out << "  \"render_size\": [" << first_frame.render_size.width << ", " << first_frame.render_size.height << "],\n";
    out << "  \"display_size\": [" << first_frame.display_size.width << ", " << first_frame.display_size.height << "],\n";
    out << "  \"color_space\": \"" << core::ToString(first_frame.color_space) << "\",\n";
    out << "  \"motion_vector_space\": \"" << core::ToString(first_frame.motion_vector_space) << "\",\n";
    out << "  \"created_utc\": \"" << TimestampUtc() << "\",\n";
    out << "  \"analysis_gate_thresholds_path\": \"" << JsonEscape(config_.analysis_gate_thresholds_path) << "\",\n";
    out << "  \"analysis_gate_thresholds_snapshot\": \"" << JsonEscape(config_.analysis_gate_thresholds_snapshot) << "\",\n";
    out << "  \"command_line\": \"" << JsonEscape(command_line) << "\"\n";
    out << "}\n";
    return WriteText(session_path_ / "session.json", out.str());
}

bool CapturePackWriter::WriteFrameRow(const HarnessFrameRow& row) {
    std::ofstream out(frames_csv_, std::ios::app);
    if (!out) {
        return false;
    }
    out.imbue(std::locale::classic());
    out << row.frame_id << ","
        << row.scenario_time_ms << ","
        << row.render_size.width << "," << row.render_size.height << ","
        << row.display_size.width << "," << row.display_size.height << ","
        << row.jitter.x << "," << row.jitter.y << ","
        << row.motion_vector_scale.x << "," << row.motion_vector_scale.y << ","
        << (row.reset_history ? 1 : 0) << ","
        << row.gpu_upload_ms << "," << row.gpu_reconstruct_ms << "," << row.gpu_present_ms << ","
        << row.cpu_frame_ms << ","
        << row.validation_errors << "," << row.validation_warnings << "\n";
    return true;
}

bool CapturePackWriter::WriteMetricRow(const HarnessMetricRow& row) {
    std::ofstream out(metrics_csv_, std::ios::app);
    if (!out) {
        return false;
    }
    out.imbue(std::locale::classic());
    out << row.frame_id << ","
        << row.ghost_score << ","
        << row.shimmer_score << ","
        << row.disocclusion_leak << ","
        << row.reactive_trail_score << ","
        << row.edge_preservation << ","
        << row.thin_feature_contrast << ","
        << row.text_contrast << ","
        << row.text_readability_contrast << ","
        << row.specular_history_leak << ","
        << row.transparent_history_leak << ","
        << row.debug_history_weight_max_abs << ","
        << row.debug_history_weight_mean_abs << ","
        << row.debug_color_residual_max_abs << ","
        << row.debug_color_residual_mean_abs << ","
        << row.debug_depth_residual_max_abs << ","
        << row.debug_depth_residual_mean_abs << ","
        << row.history_reject_pct << ","
        << row.residual_search_pct << ","
        << row.mv_luma_residual_mean << ","
        << row.mv_luma_residual_p95 << ","
        << row.mv_depth_residual_mean << ","
        << row.mv_depth_residual_p95 << ","
        << row.bad_history_trusted_pct << ","
        << row.good_history_rejected_pct << ","
        << row.reactive_history_trusted_pct << ","
        << row.disocclusion_history_trusted_pct << ","
        << row.trust_evidence_agreement_pct << ","
        << row.history_trust_mean << ","
        << row.accumulation_weight_mean << "\n";
    return true;
}

bool CapturePackWriter::WriteValidationWarnings(uint64_t frame_id, const core::ValidationReport& report) {
    std::ofstream out(warnings_jsonl_, std::ios::app);
    if (!out) {
        return false;
    }
    for (const auto& message : report.messages) {
        out << "{\"frame_id\":" << frame_id
            << ",\"severity\":\"" << ToString(message.severity)
            << "\",\"code\":\"" << JsonEscape(message.code)
            << "\",\"message\":\"" << JsonEscape(message.message) << "\"}\n";
    }
    return true;
}

bool CapturePackWriter::WriteDiagnosticWarning(uint64_t frame_id,
                                               std::string_view tag,
                                               std::string_view likely_cause,
                                               std::string_view suggested_action,
                                               uint32_t severity,
                                               double evidence_value) {
    std::ofstream out(warnings_jsonl_, std::ios::app);
    if (!out) {
        return false;
    }
    out.imbue(std::locale::classic());
    out << "{\"frame_id\":" << frame_id
        << ",\"tag\":\"" << JsonEscape(tag)
        << "\",\"likely_cause\":\"" << JsonEscape(likely_cause)
        << "\",\"suggested_action\":\"" << JsonEscape(suggested_action)
        << "\",\"severity\":" << severity
        << ",\"evidence_value\":" << evidence_value
        << "}\n";
    return true;
}

bool CapturePackWriter::WriteFrameContextJson(const core::FrameContext& frame) {
    std::ostringstream name;
    name.imbue(std::locale::classic());
    name << "frame_" << std::setw(6) << std::setfill('0') << frame.frame_id;
    const auto dir = session_path_ / name.str();
    std::filesystem::create_directories(dir);

    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << "{\n";
    out << "  \"frame_id\": " << frame.frame_id << ",\n";
    out << "  \"source_api\": \"" << JsonEscape(frame.source_api) << "\",\n";
    out << "  \"render_size\": [" << frame.render_size.width << ", " << frame.render_size.height << "],\n";
    out << "  \"display_size\": [" << frame.display_size.width << ", " << frame.display_size.height << "],\n";
    out << "  \"jitter\": [" << frame.jitter_offset.x << ", " << frame.jitter_offset.y << "],\n";
    out << "  \"motion_vector_scale\": [" << frame.motion_vector_scale.x << ", " << frame.motion_vector_scale.y << "],\n";
    out << "  \"motion_vector_space\": \"" << core::ToString(frame.motion_vector_space) << "\",\n";
    out << "  \"color_space\": \"" << core::ToString(frame.color_space) << "\",\n";
    out << "  \"flags\": {"
        << "\"reset_history\":" << (frame.flags.reset_history ? "true" : "false") << ","
        << "\"depth_inverted\":" << (frame.flags.depth_inverted ? "true" : "false") << ","
        << "\"motion_vectors_jittered\":" << (frame.flags.motion_vectors_jittered ? "true" : "false")
        << "},\n";
    out << "  \"resources\": {\n";
    out << ResourceJson("color_input", frame.color_input) << ",\n";
    out << ResourceJson("color_output", frame.color_output) << ",\n";
    out << ResourceJson("depth", frame.depth) << ",\n";
    out << ResourceJson("motion_vectors", frame.motion_vectors);
    if (frame.reactive_mask.has_value()) {
        out << ",\n" << ResourceJson("reactive_mask", *frame.reactive_mask);
    }
    out << "\n  },\n";
    out << "  \"notes\": [";
    for (size_t i = 0; i < frame.notes.size(); ++i) {
        out << (i == 0 ? "" : ", ") << "\"" << JsonEscape(frame.notes[i]) << "\"";
    }
    out << "]\n";
    out << "}\n";
    return WriteText(dir / "frame_context.json", out.str());
}

const std::filesystem::path& CapturePackWriter::SessionPath() const noexcept {
    return session_path_;
}

} // namespace osr::debug
