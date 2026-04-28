#include "debug/capture_analysis.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iterator>
#include <optional>
#include <sstream>
#include <vector>

namespace osr::debug {

namespace {

struct ResourceArtifact {
    std::filesystem::path raw;
    core::Dimensions size {};
};

std::string ReadText(const std::filesystem::path& path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

std::optional<std::string> JsonStringValue(const std::string& object, const std::string& key) {
    const std::string needle = "\"" + key + "\":\"";
    const size_t begin = object.find(needle);
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    const size_t value_begin = begin + needle.size();
    const size_t value_end = object.find('"', value_begin);
    if (value_end == std::string::npos) {
        return std::nullopt;
    }
    return object.substr(value_begin, value_end - value_begin);
}

std::optional<uint32_t> JsonUintValue(const std::string& object, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const size_t begin = object.find(needle);
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    const size_t value_begin = begin + needle.size();
    size_t value_end = value_begin;
    while (value_end < object.size() && object[value_end] >= '0' && object[value_end] <= '9') {
        ++value_end;
    }
    if (value_end == value_begin) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(std::stoul(object.substr(value_begin, value_end - value_begin)));
}

std::optional<ResourceArtifact> FindResource(const std::string& manifest,
                                             const std::filesystem::path& frame_dir,
                                             const std::string& name) {
    const std::string name_needle = "\"name\":\"" + name + "\"";
    const size_t name_pos = manifest.find(name_needle);
    if (name_pos == std::string::npos) {
        return std::nullopt;
    }
    const size_t object_begin = manifest.rfind('{', name_pos);
    const size_t object_end = manifest.find('}', name_pos);
    if (object_begin == std::string::npos || object_end == std::string::npos || object_end <= object_begin) {
        return std::nullopt;
    }
    const std::string object = manifest.substr(object_begin, object_end - object_begin + 1);
    const auto raw = JsonStringValue(object, "raw");
    const auto width = JsonUintValue(object, "width");
    const auto height = JsonUintValue(object, "height");
    if (!raw || !width || !height) {
        return std::nullopt;
    }
    ResourceArtifact artifact;
    artifact.raw = frame_dir / *raw;
    artifact.size = {*width, *height};
    return artifact;
}

std::vector<float> ReadFloatRaw(const std::filesystem::path& path, uint64_t expected_count) {
    std::vector<float> values(static_cast<size_t>(expected_count), 0.0f);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(float)));
    if (static_cast<uint64_t>(in.gcount()) != expected_count * sizeof(float)) {
        return {};
    }
    return values;
}

std::vector<float> ReadMotionMagnitudeRaw(const std::filesystem::path& path, uint64_t expected_count) {
    struct Float2 {
        float x;
        float y;
    };
    std::vector<Float2> values(static_cast<size_t>(expected_count));
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(Float2)));
    if (static_cast<uint64_t>(in.gcount()) != expected_count * sizeof(Float2)) {
        return {};
    }
    std::vector<float> magnitudes(values.size(), 0.0f);
    for (size_t i = 0; i < values.size(); ++i) {
        magnitudes[i] = std::hypot(values[i].x, values[i].y);
    }
    return magnitudes;
}

CaptureValueStats ComputeStats(const std::vector<float>& values, double threshold) {
    CaptureValueStats stats;
    stats.samples = values.size();
    stats.threshold = threshold;
    if (values.empty()) {
        return stats;
    }
    double sum = 0.0;
    uint64_t over = 0;
    stats.min = values[0];
    stats.max = values[0];
    for (float value : values) {
        stats.min = std::min(stats.min, static_cast<double>(value));
        stats.max = std::max(stats.max, static_cast<double>(value));
        sum += value;
        if (value > threshold) {
            ++over;
        }
    }
    stats.mean = sum / static_cast<double>(values.size());
    stats.over_threshold_pct = static_cast<double>(over) * 100.0 / static_cast<double>(values.size());
    return stats;
}

bool LoadFloatResource(const std::string& manifest,
                       const std::filesystem::path& frame_dir,
                       const std::string& name,
                       double threshold,
                       core::Dimensions* size,
                       CaptureValueStats* stats,
                       std::string* error) {
    const auto artifact = FindResource(manifest, frame_dir, name);
    if (!artifact) {
        *error = "missing resource: " + name;
        return false;
    }
    const uint64_t count = static_cast<uint64_t>(artifact->size.width) * artifact->size.height;
    const auto values = ReadFloatRaw(artifact->raw, count);
    if (values.empty()) {
        *error = "failed to read raw resource: " + artifact->raw.string();
        return false;
    }
    *size = artifact->size;
    *stats = ComputeStats(values, threshold);
    return true;
}

} // namespace

CaptureFrameAnalysis AnalyzeCaptureFrame(const std::filesystem::path& frame_dir) {
    CaptureFrameAnalysis analysis;
    const auto manifest_path = frame_dir / "artifacts.json";
    const std::string manifest = ReadText(manifest_path);
    if (manifest.empty()) {
        analysis.error = "missing or empty artifacts.json";
        return analysis;
    }

    if (!LoadFloatResource(manifest, frame_dir, "history_weight", 0.5, &analysis.display_size, &analysis.history_weight, &analysis.error) ||
        !LoadFloatResource(manifest, frame_dir, "color_residual", 0.16, &analysis.display_size, &analysis.color_residual, &analysis.error) ||
        !LoadFloatResource(manifest, frame_dir, "depth_residual", 0.035, &analysis.display_size, &analysis.depth_residual, &analysis.error)) {
        return analysis;
    }

    const auto motion = FindResource(manifest, frame_dir, "motion_vectors");
    if (!motion) {
        analysis.error = "missing resource: motion_vectors";
        return analysis;
    }
    const uint64_t motion_count = static_cast<uint64_t>(motion->size.width) * motion->size.height;
    const auto magnitudes = ReadMotionMagnitudeRaw(motion->raw, motion_count);
    if (magnitudes.empty()) {
        analysis.error = "failed to read raw resource: " + motion->raw.string();
        return analysis;
    }
    analysis.render_size = motion->size;
    analysis.motion_magnitude = ComputeStats(magnitudes, 0.01);
    analysis.ok = true;
    return analysis;
}

std::string SummarizeCaptureAnalysis(const CaptureFrameAnalysis& analysis) {
    if (!analysis.ok) {
        return "capture_analysis_failed: " + analysis.error;
    }
    std::ostringstream out;
    out << "capture_analysis"
        << " display=" << analysis.display_size.width << "x" << analysis.display_size.height
        << " render=" << analysis.render_size.width << "x" << analysis.render_size.height
        << " history_mean=" << analysis.history_weight.mean
        << " history_trusted_pct=" << analysis.history_weight.over_threshold_pct
        << " color_residual_mean=" << analysis.color_residual.mean
        << " color_reject_candidate_pct=" << analysis.color_residual.over_threshold_pct
        << " depth_residual_mean=" << analysis.depth_residual.mean
        << " depth_reject_candidate_pct=" << analysis.depth_residual.over_threshold_pct
        << " motion_mean=" << analysis.motion_magnitude.mean
        << " motion_active_pct=" << analysis.motion_magnitude.over_threshold_pct;
    return out.str();
}

} // namespace osr::debug
