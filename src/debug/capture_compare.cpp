#include "debug/capture_compare.h"

#include "debug/capture_pack.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <locale>
#include <optional>
#include <sstream>
#include <string_view>

namespace osr::debug {

namespace {

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::filesystem::path AnalysisPath(const std::filesystem::path& capture_path) {
    if (capture_path.filename() == "capture_analysis.json") {
        return capture_path;
    }
    return capture_path / "capture_analysis.json";
}

std::optional<std::string> ObjectForKey(const std::string& text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":";
    const size_t key_pos = text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    size_t begin = text.find('{', key_pos + needle.size());
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    int depth = 0;
    for (size_t i = begin; i < text.size(); ++i) {
        if (text[i] == '{') {
            ++depth;
        } else if (text[i] == '}') {
            --depth;
            if (depth == 0) {
                return text.substr(begin, i - begin + 1);
            }
        }
    }
    return std::nullopt;
}

std::optional<double> DoubleValue(const std::string& text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":";
    const size_t key_pos = text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    size_t begin = key_pos + needle.size();
    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    size_t end = begin;
    while (end < text.size() &&
           (std::isdigit(static_cast<unsigned char>(text[end])) ||
            text[end] == '-' || text[end] == '+' || text[end] == '.' ||
            text[end] == 'e' || text[end] == 'E')) {
        ++end;
    }
    if (end == begin) {
        return std::nullopt;
    }
    return std::stod(text.substr(begin, end - begin));
}

std::optional<bool> BoolValue(const std::string& text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":";
    const size_t key_pos = text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    size_t begin = key_pos + needle.size();
    while (begin < text.size() && (text[begin] == ' ' || text[begin] == '\t')) {
        ++begin;
    }
    if (text.compare(begin, 4, "true") == 0) {
        return true;
    }
    if (text.compare(begin, 5, "false") == 0) {
        return false;
    }
    return std::nullopt;
}

std::optional<std::string> StringValue(const std::string& text, std::string_view key) {
    const std::string needle = "\"" + std::string(key) + "\":\"";
    const size_t key_pos = text.find(needle);
    if (key_pos == std::string::npos) {
        return std::nullopt;
    }
    const size_t begin = key_pos + needle.size();
    const size_t end = text.find('"', begin);
    if (end == std::string::npos) {
        return std::nullopt;
    }
    return text.substr(begin, end - begin);
}

double Clamp01(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

double Score(const CaptureComparisonRow& row) {
    if (!row.loaded || !row.analysis_ok) {
        return -1000000.0;
    }
    const double motion_ok = 1.0 - Clamp01(row.motion_history_trusted_pct / 1.0);
    const double static_ok = Clamp01((row.static_history_trusted_pct - 90.0) / 10.0);
    const double text_ok = Clamp01((row.text_history_trusted_pct - 35.0) / 65.0);
    const double specular_ok = 1.0 - Clamp01(row.specular_history_trusted_pct / 2.0);
    const double transparent_ok = 1.0 - Clamp01(row.transparent_history_trusted_pct / 8.0);
    const double reactive_ok = 1.0 - Clamp01(row.reactive_history_trusted_pct / 1.0);
    const double color_ok = 1.0 - Clamp01(row.color_reject_candidate_pct / 3.0);
    const double locked_detail_ok = Clamp01(row.locked_detail_score / 100.0);
    const double native_detail_ok = row.text_native_contrast_ratio > 0.0
        ? Clamp01((row.text_native_contrast_ratio - 0.90) / 0.10)
        : 0.0;
    const double bad_lock_ok = 1.0 - Clamp01(row.bad_lock_signal);
    const double quality =
        100.0 * (0.25 * motion_ok +
                 0.15 * static_ok +
                 0.15 * reactive_ok +
                 0.11 * text_ok +
                 0.10 * specular_ok +
                 0.075 * transparent_ok +
                 0.075 * color_ok +
                 0.05 * locked_detail_ok +
                 0.01 * native_detail_ok +
                 0.03 * bad_lock_ok);
    return row.gate_passed ? quality : quality - 10000.0;
}

double RegionHistoryTrustedPct(const std::string& regions, std::string_view region) {
    const auto object = ObjectForKey(regions, region);
    if (!object) {
        return 0.0;
    }
    return DoubleValue(*object, "history_trusted_pct").value_or(0.0);
}

} // namespace

CaptureComparisonRow LoadCaptureComparisonRow(const std::filesystem::path& capture_path) {
    CaptureComparisonRow row;
    row.path = capture_path;
    const auto path = AnalysisPath(capture_path);
    const auto text = ReadText(path);
    if (text.empty()) {
        row.error = "missing capture_analysis.json";
        row.score = Score(row);
        return row;
    }

    row.loaded = true;
    row.analysis_ok = BoolValue(text, "ok").value_or(false);
    row.error = StringValue(text, "error").value_or("");
    row.frame_id = static_cast<uint64_t>(DoubleValue(text, "frame_id").value_or(0.0));

    const auto gate = ObjectForKey(text, "gate").value_or("{}");
    row.gate_passed = BoolValue(gate, "passed").value_or(false);
    row.gate_reason = StringValue(gate, "reason").value_or("");

    const auto global = ObjectForKey(text, "global").value_or("{}");
    const auto color_residual = ObjectForKey(global, "color_residual").value_or("{}");
    row.color_reject_candidate_pct = DoubleValue(color_residual, "over_threshold_pct").value_or(0.0);
    const auto locked_detail = ObjectForKey(text, "locked_detail").value_or("{}");
    row.text_contrast_ratio = DoubleValue(locked_detail, "text_contrast_ratio").value_or(0.0);
    row.text_native_contrast_ratio = DoubleValue(locked_detail, "text_native_contrast_ratio").value_or(0.0);
    row.bad_lock_signal = DoubleValue(locked_detail, "bad_lock_signal").value_or(0.0);
    row.locked_detail_score = DoubleValue(locked_detail, "score").value_or(0.0);

    const auto split = ObjectForKey(text, "motion_static_split").value_or("{}");
    row.motion_history_trusted_pct = DoubleValue(split, "motion_region_history_trusted_pct").value_or(0.0);
    row.static_history_trusted_pct = DoubleValue(split, "static_region_history_trusted_pct").value_or(0.0);

    const auto regions = ObjectForKey(text, "regions").value_or("{}");
    row.text_history_trusted_pct = RegionHistoryTrustedPct(regions, "text");
    row.specular_history_trusted_pct = RegionHistoryTrustedPct(regions, "specular");
    row.transparent_history_trusted_pct = RegionHistoryTrustedPct(regions, "transparent");
    row.reactive_history_trusted_pct = RegionHistoryTrustedPct(regions, "reactive");

    row.score = Score(row);
    return row;
}

std::vector<CaptureComparisonRow> RankCaptureComparisons(std::vector<CaptureComparisonRow> rows) {
    std::sort(rows.begin(), rows.end(), [](const CaptureComparisonRow& lhs, const CaptureComparisonRow& rhs) {
        if (lhs.score != rhs.score) {
            return lhs.score > rhs.score;
        }
        return lhs.path.string() < rhs.path.string();
    });
    return rows;
}

std::string CaptureComparisonCsvHeader() {
    return "rank,path,loaded,analysis_ok,gate_passed,score,frame_id,gate_reason,error,"
           "motion_history_trusted_pct,static_history_trusted_pct,text_history_trusted_pct,"
           "specular_history_trusted_pct,transparent_history_trusted_pct,reactive_history_trusted_pct,"
           "color_reject_candidate_pct,text_contrast_ratio,text_native_contrast_ratio,bad_lock_signal,locked_detail_score";
}

std::string CaptureComparisonCsvRow(size_t rank, const CaptureComparisonRow& row) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << rank << ","
        << CsvEscape(row.path.string()) << ","
        << (row.loaded ? 1 : 0) << ","
        << (row.analysis_ok ? 1 : 0) << ","
        << (row.gate_passed ? 1 : 0) << ","
        << row.score << ","
        << row.frame_id << ","
        << CsvEscape(row.gate_reason) << ","
        << CsvEscape(row.error) << ","
        << row.motion_history_trusted_pct << ","
        << row.static_history_trusted_pct << ","
        << row.text_history_trusted_pct << ","
        << row.specular_history_trusted_pct << ","
        << row.transparent_history_trusted_pct << ","
        << row.reactive_history_trusted_pct << ","
        << row.color_reject_candidate_pct << ","
        << row.text_contrast_ratio << ","
        << row.text_native_contrast_ratio << ","
        << row.bad_lock_signal << ","
        << row.locked_detail_score;
    return out.str();
}

} // namespace osr::debug
