#include "debug/capture_analysis.h"

#include "demo/wind_tunnel/synthetic_roi.h"

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

std::optional<uint64_t> JsonUint64Value(const std::string& object, const std::string& key) {
    const std::string needle = "\"" + key + "\":";
    const size_t begin = object.find(needle);
    if (begin == std::string::npos) {
        return std::nullopt;
    }
    size_t value_begin = begin + needle.size();
    while (value_begin < object.size() && (object[value_begin] == ' ' || object[value_begin] == '\t')) {
        ++value_begin;
    }
    size_t value_end = value_begin;
    while (value_end < object.size() && object[value_end] >= '0' && object[value_end] <= '9') {
        ++value_end;
    }
    if (value_end == value_begin) {
        return std::nullopt;
    }
    return static_cast<uint64_t>(std::stoull(object.substr(value_begin, value_end - value_begin)));
}

std::optional<uint32_t> JsonUintValue(const std::string& object, const std::string& key) {
    const auto value = JsonUint64Value(object, key);
    if (!value) {
        return std::nullopt;
    }
    return static_cast<uint32_t>(*value);
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

void ComputeMotionSplitHistory(const std::vector<float>& history,
                               core::Dimensions display_size,
                               const std::vector<float>& motion,
                               core::Dimensions render_size,
                               CaptureFrameAnalysis& analysis) {
    if (history.size() != static_cast<size_t>(display_size.width) * display_size.height ||
        motion.size() != static_cast<size_t>(render_size.width) * render_size.height ||
        !display_size.IsValid() ||
        !render_size.IsValid()) {
        return;
    }
    double motion_history_sum = 0.0;
    double static_history_sum = 0.0;
    uint64_t motion_count = 0;
    uint64_t static_count = 0;
    uint64_t motion_trusted = 0;
    uint64_t static_trusted = 0;
    for (uint32_t y = 0; y < display_size.height; ++y) {
        const uint32_t ry = std::min(render_size.height - 1,
                                     static_cast<uint32_t>((static_cast<uint64_t>(y) * render_size.height) / display_size.height));
        for (uint32_t x = 0; x < display_size.width; ++x) {
            const uint32_t rx = std::min(render_size.width - 1,
                                         static_cast<uint32_t>((static_cast<uint64_t>(x) * render_size.width) / display_size.width));
            const float history_weight = history[static_cast<size_t>(y) * display_size.width + x];
            const bool motion_region = motion[static_cast<size_t>(ry) * render_size.width + rx] > 0.01f;
            if (motion_region) {
                motion_history_sum += history_weight;
                ++motion_count;
                if (history_weight > 0.5f) {
                    ++motion_trusted;
                }
            } else {
                static_history_sum += history_weight;
                ++static_count;
                if (history_weight > 0.5f) {
                    ++static_trusted;
                }
            }
        }
    }
    analysis.motion_region_mean_history = motion_count == 0 ? 0.0 : motion_history_sum / static_cast<double>(motion_count);
    analysis.static_region_mean_history = static_count == 0 ? 0.0 : static_history_sum / static_cast<double>(static_count);
    analysis.motion_region_history_trusted_pct = motion_count == 0 ? 0.0 : static_cast<double>(motion_trusted) * 100.0 / static_cast<double>(motion_count);
    analysis.static_region_history_trusted_pct = static_count == 0 ? 0.0 : static_cast<double>(static_trusted) * 100.0 / static_cast<double>(static_count);
}

CaptureRegionStats FinalizeRegion(uint64_t samples,
                                   uint64_t trusted,
                                   double history_sum,
                                   double color_residual_sum) {
    CaptureRegionStats stats;
    stats.samples = samples;
    if (samples == 0) {
        return stats;
    }
    stats.mean_history = history_sum / static_cast<double>(samples);
    stats.mean_color_residual = color_residual_sum / static_cast<double>(samples);
    stats.history_trusted_pct = static_cast<double>(trusted) * 100.0 / static_cast<double>(samples);
    return stats;
}

void AccumulateRegionSample(float history_weight,
                            float color_residual,
                            uint64_t& samples,
                            uint64_t& trusted,
                            double& history_sum,
                            double& color_residual_sum) {
    ++samples;
    history_sum += history_weight;
    color_residual_sum += color_residual;
    if (history_weight > 0.5f) {
        ++trusted;
    }
}

void ComputeSyntheticRoiStats(const std::vector<float>& history,
                              const std::vector<float>& color_residual,
                              core::Dimensions display_size,
                              const std::vector<float>& reactive_mask,
                              core::Dimensions render_size,
                              uint64_t frame_id,
                              CaptureFrameAnalysis& analysis) {
    if (history.size() != static_cast<size_t>(display_size.width) * display_size.height ||
        color_residual.size() != history.size() ||
        !display_size.IsValid()) {
        return;
    }
    const bool has_reactive = reactive_mask.size() == static_cast<size_t>(render_size.width) * render_size.height &&
                              render_size.IsValid();
    uint64_t text_samples = 0;
    uint64_t specular_samples = 0;
    uint64_t transparent_samples = 0;
    uint64_t reactive_samples = 0;
    uint64_t text_trusted = 0;
    uint64_t specular_trusted = 0;
    uint64_t transparent_trusted = 0;
    uint64_t reactive_trusted = 0;
    double text_history_sum = 0.0;
    double specular_history_sum = 0.0;
    double transparent_history_sum = 0.0;
    double reactive_history_sum = 0.0;
    double text_color_sum = 0.0;
    double specular_color_sum = 0.0;
    double transparent_color_sum = 0.0;
    double reactive_color_sum = 0.0;

    for (uint32_t y = 0; y < display_size.height; ++y) {
        const uint32_t ry = has_reactive
            ? std::min(render_size.height - 1,
                       static_cast<uint32_t>((static_cast<uint64_t>(y) * render_size.height) / display_size.height))
            : 0;
        for (uint32_t x = 0; x < display_size.width; ++x) {
            const size_t display_index = static_cast<size_t>(y) * display_size.width + x;
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(display_size.width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(display_size.height);
            const float history_weight = history[display_index];
            const float color = color_residual[display_index];
            const auto text = ::osr::demo::wind_tunnel::EvaluateSyntheticTextCoverage(u, v, frame_id, true);
            const auto material = ::osr::demo::wind_tunnel::EvaluateSyntheticMaterialCoverage(u, v, frame_id, true);
            if (text.glyph) {
                AccumulateRegionSample(history_weight, color, text_samples, text_trusted, text_history_sum, text_color_sum);
            }
            if (material.specular) {
                AccumulateRegionSample(history_weight, color, specular_samples, specular_trusted, specular_history_sum, specular_color_sum);
            }
            if (material.transparent) {
                AccumulateRegionSample(history_weight, color, transparent_samples, transparent_trusted, transparent_history_sum, transparent_color_sum);
            }
            if (has_reactive) {
                const uint32_t rx = std::min(render_size.width - 1,
                                             static_cast<uint32_t>((static_cast<uint64_t>(x) * render_size.width) / display_size.width));
                if (reactive_mask[static_cast<size_t>(ry) * render_size.width + rx] > 0.5f) {
                    AccumulateRegionSample(history_weight, color, reactive_samples, reactive_trusted, reactive_history_sum, reactive_color_sum);
                }
            }
        }
    }

    analysis.text_region = FinalizeRegion(text_samples, text_trusted, text_history_sum, text_color_sum);
    analysis.specular_region = FinalizeRegion(specular_samples, specular_trusted, specular_history_sum, specular_color_sum);
    analysis.transparent_region = FinalizeRegion(transparent_samples, transparent_trusted, transparent_history_sum, transparent_color_sum);
    analysis.reactive_region = FinalizeRegion(reactive_samples, reactive_trusted, reactive_history_sum, reactive_color_sum);
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
    const std::string frame_context = ReadText(frame_dir / "frame_context.json");
    if (!frame_context.empty()) {
        analysis.frame_id = JsonUint64Value(frame_context, "frame_id").value_or(0);
    }

    const auto history_artifact = FindResource(manifest, frame_dir, "history_weight");
    if (!history_artifact) {
        analysis.error = "missing resource: history_weight";
        return analysis;
    }
    const auto history_values = ReadFloatRaw(history_artifact->raw,
                                             static_cast<uint64_t>(history_artifact->size.width) * history_artifact->size.height);
    if (history_values.empty()) {
        analysis.error = "failed to read raw resource: " + history_artifact->raw.string();
        return analysis;
    }
    analysis.display_size = history_artifact->size;
    analysis.history_weight = ComputeStats(history_values, 0.5);

    const auto color_artifact = FindResource(manifest, frame_dir, "color_residual");
    if (!color_artifact) {
        analysis.error = "missing resource: color_residual";
        return analysis;
    }
    const auto color_values = ReadFloatRaw(color_artifact->raw,
                                           static_cast<uint64_t>(color_artifact->size.width) * color_artifact->size.height);
    if (color_values.empty()) {
        analysis.error = "failed to read raw resource: " + color_artifact->raw.string();
        return analysis;
    }
    analysis.color_residual = ComputeStats(color_values, 0.16);

    const auto depth_artifact = FindResource(manifest, frame_dir, "depth_residual");
    if (!depth_artifact) {
        analysis.error = "missing resource: depth_residual";
        return analysis;
    }
    const auto depth_values = ReadFloatRaw(depth_artifact->raw,
                                           static_cast<uint64_t>(depth_artifact->size.width) * depth_artifact->size.height);
    if (depth_values.empty()) {
        analysis.error = "failed to read raw resource: " + depth_artifact->raw.string();
        return analysis;
    }
    analysis.depth_residual = ComputeStats(depth_values, 0.035);

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
    ComputeMotionSplitHistory(history_values, analysis.display_size, magnitudes, analysis.render_size, analysis);
    std::vector<float> reactive_values;
    core::Dimensions reactive_size {};
    if (const auto reactive = FindResource(manifest, frame_dir, "reactive_mask")) {
        reactive_size = reactive->size;
        reactive_values = ReadFloatRaw(reactive->raw, static_cast<uint64_t>(reactive->size.width) * reactive->size.height);
    }
    ComputeSyntheticRoiStats(history_values,
                             color_values,
                             analysis.display_size,
                             reactive_values,
                             reactive_size,
                             analysis.frame_id,
                             analysis);
    analysis.ok = true;
    return analysis;
}

std::string SummarizeCaptureAnalysis(const CaptureFrameAnalysis& analysis) {
    if (!analysis.ok) {
        return "capture_analysis_failed: " + analysis.error;
    }
    std::ostringstream out;
    out << "capture_analysis"
        << " frame_id=" << analysis.frame_id
        << " display=" << analysis.display_size.width << "x" << analysis.display_size.height
        << " render=" << analysis.render_size.width << "x" << analysis.render_size.height
        << " history_mean=" << analysis.history_weight.mean
        << " history_trusted_pct=" << analysis.history_weight.over_threshold_pct
        << " color_residual_mean=" << analysis.color_residual.mean
        << " color_reject_candidate_pct=" << analysis.color_residual.over_threshold_pct
        << " depth_residual_mean=" << analysis.depth_residual.mean
        << " depth_reject_candidate_pct=" << analysis.depth_residual.over_threshold_pct
        << " motion_mean=" << analysis.motion_magnitude.mean
        << " motion_active_pct=" << analysis.motion_magnitude.over_threshold_pct
        << " motion_history_mean=" << analysis.motion_region_mean_history
        << " motion_history_trusted_pct=" << analysis.motion_region_history_trusted_pct
        << " static_history_mean=" << analysis.static_region_mean_history
        << " static_history_trusted_pct=" << analysis.static_region_history_trusted_pct
        << " text_samples=" << analysis.text_region.samples
        << " text_history_mean=" << analysis.text_region.mean_history
        << " text_history_trusted_pct=" << analysis.text_region.history_trusted_pct
        << " specular_samples=" << analysis.specular_region.samples
        << " specular_history_mean=" << analysis.specular_region.mean_history
        << " specular_history_trusted_pct=" << analysis.specular_region.history_trusted_pct
        << " transparent_samples=" << analysis.transparent_region.samples
        << " transparent_history_mean=" << analysis.transparent_region.mean_history
        << " transparent_history_trusted_pct=" << analysis.transparent_region.history_trusted_pct
        << " reactive_samples=" << analysis.reactive_region.samples
        << " reactive_history_mean=" << analysis.reactive_region.mean_history
        << " reactive_history_trusted_pct=" << analysis.reactive_region.history_trusted_pct;
    return out.str();
}

} // namespace osr::debug
