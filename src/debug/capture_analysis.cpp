#include "debug/capture_analysis.h"

#include "demo/wind_tunnel/synthetic_roi.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <locale>
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

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

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
                out << "\\u00" << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(ch) << std::dec;
            } else {
                out << static_cast<char>(ch);
            }
            break;
        }
    }
    return out.str();
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

std::vector<uint32_t> ReadRgba8Raw(const std::filesystem::path& path, uint64_t expected_count) {
    std::vector<uint32_t> values(static_cast<size_t>(expected_count), 0u);
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        return {};
    }
    in.read(reinterpret_cast<char*>(values.data()), static_cast<std::streamsize>(values.size() * sizeof(uint32_t)));
    if (static_cast<uint64_t>(in.gcount()) != expected_count * sizeof(uint32_t)) {
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

double Clamp01(double value) noexcept {
    return std::clamp(value, 0.0, 1.0);
}

float LumaFromRgba8(uint32_t rgba) noexcept {
    const float r = static_cast<float>((rgba >> 16) & 0xffu) / 255.0f;
    const float g = static_cast<float>((rgba >> 8) & 0xffu) / 255.0f;
    const float b = static_cast<float>(rgba & 0xffu) / 255.0f;
    return r * 0.2126f + g * 0.7152f + b * 0.0722f;
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

double TextOutputContrast(const std::vector<uint32_t>& output,
                          core::Dimensions display_size,
                          uint64_t frame_id,
                          std::optional<bool> moving_filter = std::nullopt) noexcept {
    if (output.size() != static_cast<size_t>(display_size.width) * display_size.height ||
        !display_size.IsValid()) {
        return 0.0;
    }
    double glyph_sum = 0.0;
    double panel_sum = 0.0;
    uint64_t glyph_samples = 0;
    uint64_t panel_samples = 0;
    for (uint32_t y = 0; y < display_size.height; ++y) {
        for (uint32_t x = 0; x < display_size.width; ++x) {
            const float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(display_size.width);
            const float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(display_size.height);
            const auto text = ::osr::demo::wind_tunnel::EvaluateSyntheticTextCoverage(u, v, frame_id, true);
            if (!text.panel) {
                continue;
            }
            if (moving_filter.has_value() && text.moving != *moving_filter) {
                continue;
            }
            const float luma = LumaFromRgba8(output[static_cast<size_t>(y) * display_size.width + x]);
            if (text.glyph) {
                glyph_sum += luma;
                ++glyph_samples;
            } else {
                panel_sum += luma;
                ++panel_samples;
            }
        }
    }
    if (glyph_samples == 0 || panel_samples == 0) {
        return 0.0;
    }
    return std::abs(glyph_sum / static_cast<double>(glyph_samples) -
                    panel_sum / static_cast<double>(panel_samples));
}

CaptureLockedDetailStats ComputeLockedDetailStats(const CaptureFrameAnalysis& analysis,
                                                  const std::vector<uint32_t>& output,
                                                  const std::vector<uint32_t>& spatial_baseline) noexcept {
    CaptureLockedDetailStats stats;
    stats.text_output_contrast = TextOutputContrast(output, analysis.display_size, analysis.frame_id);
    stats.text_spatial_contrast = TextOutputContrast(spatial_baseline, analysis.display_size, analysis.frame_id);
    if (analysis.static_text_region.samples > 0) {
        stats.text_output_contrast = TextOutputContrast(output, analysis.display_size, analysis.frame_id, false);
        stats.text_spatial_contrast = TextOutputContrast(spatial_baseline, analysis.display_size, analysis.frame_id, false);
    }
    if (stats.text_spatial_contrast > 0.0001) {
        stats.text_contrast_ratio = stats.text_output_contrast / stats.text_spatial_contrast;
    }
    const auto& detail_region = analysis.static_text_region.samples > 0
        ? analysis.static_text_region
        : analysis.text_region;
    stats.text_lock_signal = Clamp01(detail_region.mean_feature_lock / 0.015);
    const double bad_lock = std::max({analysis.specular_region.mean_feature_lock,
                                      analysis.transparent_region.mean_feature_lock,
                                      analysis.reactive_region.mean_feature_lock});
    stats.bad_lock_signal = Clamp01(bad_lock / 0.04);
    const double contrast_signal = stats.text_contrast_ratio > 0.0
        ? Clamp01((stats.text_contrast_ratio - 0.75) / 0.35)
        : output.empty()
        ? stats.text_lock_signal
        : Clamp01(stats.text_output_contrast / 0.12);
    const double useful_detail = 0.60 * stats.text_lock_signal + 0.40 * contrast_signal;
    stats.score = 100.0 * useful_detail * (1.0 - 0.75 * stats.bad_lock_signal);
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
                                   double color_residual_sum,
                                   double feature_lock_sum) {
    CaptureRegionStats stats;
    stats.samples = samples;
    if (samples == 0) {
        return stats;
    }
    stats.mean_history = history_sum / static_cast<double>(samples);
    stats.mean_color_residual = color_residual_sum / static_cast<double>(samples);
    stats.mean_feature_lock = feature_lock_sum / static_cast<double>(samples);
    stats.history_trusted_pct = static_cast<double>(trusted) * 100.0 / static_cast<double>(samples);
    return stats;
}

void AccumulateRegionSample(float history_weight,
                            float color_residual,
                            float feature_lock,
                            uint64_t& samples,
                            uint64_t& trusted,
                            double& history_sum,
                            double& color_residual_sum,
                            double& feature_lock_sum) {
    ++samples;
    history_sum += history_weight;
    color_residual_sum += color_residual;
    feature_lock_sum += feature_lock;
    if (history_weight > 0.5f) {
        ++trusted;
    }
}

void ComputeSyntheticRoiStats(const std::vector<float>& history,
                              const std::vector<float>& color_residual,
                              const std::vector<float>& feature_lock,
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
    const bool has_feature_lock = feature_lock.size() == history.size();
    uint64_t text_samples = 0;
    uint64_t static_text_samples = 0;
    uint64_t moving_text_samples = 0;
    uint64_t specular_samples = 0;
    uint64_t transparent_samples = 0;
    uint64_t reactive_samples = 0;
    uint64_t text_trusted = 0;
    uint64_t static_text_trusted = 0;
    uint64_t moving_text_trusted = 0;
    uint64_t specular_trusted = 0;
    uint64_t transparent_trusted = 0;
    uint64_t reactive_trusted = 0;
    double text_history_sum = 0.0;
    double static_text_history_sum = 0.0;
    double moving_text_history_sum = 0.0;
    double specular_history_sum = 0.0;
    double transparent_history_sum = 0.0;
    double reactive_history_sum = 0.0;
    double text_color_sum = 0.0;
    double static_text_color_sum = 0.0;
    double moving_text_color_sum = 0.0;
    double specular_color_sum = 0.0;
    double transparent_color_sum = 0.0;
    double reactive_color_sum = 0.0;
    double text_lock_sum = 0.0;
    double static_text_lock_sum = 0.0;
    double moving_text_lock_sum = 0.0;
    double specular_lock_sum = 0.0;
    double transparent_lock_sum = 0.0;
    double reactive_lock_sum = 0.0;

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
            const float lock = has_feature_lock ? feature_lock[display_index] : 0.0f;
            const auto text = ::osr::demo::wind_tunnel::EvaluateSyntheticTextCoverage(u, v, frame_id, true);
            const auto material = ::osr::demo::wind_tunnel::EvaluateSyntheticMaterialCoverage(u, v, frame_id, true);
            if (text.glyph) {
                AccumulateRegionSample(history_weight, color, lock, text_samples, text_trusted, text_history_sum, text_color_sum, text_lock_sum);
                if (text.moving) {
                    AccumulateRegionSample(history_weight, color, lock, moving_text_samples, moving_text_trusted, moving_text_history_sum, moving_text_color_sum, moving_text_lock_sum);
                } else {
                    AccumulateRegionSample(history_weight, color, lock, static_text_samples, static_text_trusted, static_text_history_sum, static_text_color_sum, static_text_lock_sum);
                }
            }
            if (material.specular) {
                AccumulateRegionSample(history_weight, color, lock, specular_samples, specular_trusted, specular_history_sum, specular_color_sum, specular_lock_sum);
            }
            if (material.transparent) {
                AccumulateRegionSample(history_weight, color, lock, transparent_samples, transparent_trusted, transparent_history_sum, transparent_color_sum, transparent_lock_sum);
            }
            if (has_reactive) {
                const uint32_t rx = std::min(render_size.width - 1,
                                             static_cast<uint32_t>((static_cast<uint64_t>(x) * render_size.width) / display_size.width));
                if (reactive_mask[static_cast<size_t>(ry) * render_size.width + rx] > 0.5f) {
                    AccumulateRegionSample(history_weight, color, lock, reactive_samples, reactive_trusted, reactive_history_sum, reactive_color_sum, reactive_lock_sum);
                }
            }
        }
    }

    analysis.text_region = FinalizeRegion(text_samples, text_trusted, text_history_sum, text_color_sum, text_lock_sum);
    analysis.static_text_region = FinalizeRegion(static_text_samples,
                                                 static_text_trusted,
                                                 static_text_history_sum,
                                                 static_text_color_sum,
                                                 static_text_lock_sum);
    analysis.moving_text_region = FinalizeRegion(moving_text_samples,
                                                 moving_text_trusted,
                                                 moving_text_history_sum,
                                                 moving_text_color_sum,
                                                 moving_text_lock_sum);
    analysis.specular_region = FinalizeRegion(specular_samples, specular_trusted, specular_history_sum, specular_color_sum, specular_lock_sum);
    analysis.transparent_region = FinalizeRegion(transparent_samples, transparent_trusted, transparent_history_sum, transparent_color_sum, transparent_lock_sum);
    analysis.reactive_region = FinalizeRegion(reactive_samples, reactive_trusted, reactive_history_sum, reactive_color_sum, reactive_lock_sum);
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

    std::vector<float> feature_lock_values;
    if (const auto feature_lock = FindResource(manifest, frame_dir, "feature_lock_strength")) {
        feature_lock_values = ReadFloatRaw(feature_lock->raw,
                                           static_cast<uint64_t>(feature_lock->size.width) * feature_lock->size.height);
        if (feature_lock_values.empty()) {
            analysis.error = "failed to read raw resource: " + feature_lock->raw.string();
            return analysis;
        }
        analysis.feature_lock_strength = ComputeStats(feature_lock_values, 0.5);
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
    ComputeMotionSplitHistory(history_values, analysis.display_size, magnitudes, analysis.render_size, analysis);
    std::vector<float> reactive_values;
    core::Dimensions reactive_size {};
    if (const auto reactive = FindResource(manifest, frame_dir, "reactive_mask")) {
        reactive_size = reactive->size;
        reactive_values = ReadFloatRaw(reactive->raw, static_cast<uint64_t>(reactive->size.width) * reactive->size.height);
    }
    ComputeSyntheticRoiStats(history_values,
                             color_values,
                             feature_lock_values,
                             analysis.display_size,
                             reactive_values,
                             reactive_size,
                             analysis.frame_id,
                             analysis);
    std::vector<uint32_t> output_values;
    if (const auto output = FindResource(manifest, frame_dir, "color_output")) {
        output_values = ReadRgba8Raw(output->raw,
                                     static_cast<uint64_t>(output->size.width) * output->size.height);
    }
    std::vector<uint32_t> spatial_values;
    if (const auto spatial = FindResource(manifest, frame_dir, "spatial_baseline")) {
        spatial_values = ReadRgba8Raw(spatial->raw,
                                      static_cast<uint64_t>(spatial->size.width) * spatial->size.height);
    }
    analysis.locked_detail = ComputeLockedDetailStats(analysis, output_values, spatial_values);
    analysis.ok = true;
    return analysis;
}

CaptureAnalysisGateThresholds LoadCaptureAnalysisGateThresholds(const std::filesystem::path& path) {
    CaptureAnalysisGateThresholds thresholds;
    std::ifstream file(path);
    if (!file.is_open()) {
        return thresholds;
    }

    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const auto key = Trim(line.substr(0, separator));
        const auto value = Trim(line.substr(separator + 1));
        if (key == "max_motion_history_trusted_pct") {
            thresholds.max_motion_history_trusted_pct = std::stod(value);
        } else if (key == "min_static_history_trusted_pct") {
            thresholds.min_static_history_trusted_pct = std::stod(value);
        } else if (key == "min_text_history_trusted_pct") {
            thresholds.min_text_history_trusted_pct = std::stod(value);
        } else if (key == "max_specular_history_trusted_pct") {
            thresholds.max_specular_history_trusted_pct = std::stod(value);
        } else if (key == "max_transparent_history_trusted_pct") {
            thresholds.max_transparent_history_trusted_pct = std::stod(value);
        } else if (key == "max_reactive_history_trusted_pct") {
            thresholds.max_reactive_history_trusted_pct = std::stod(value);
        } else if (key == "max_color_reject_candidate_pct") {
            thresholds.max_color_reject_candidate_pct = std::stod(value);
        } else if (key == "min_locked_detail_score") {
            thresholds.min_locked_detail_score = std::stod(value);
        } else if (key == "max_bad_lock_signal") {
            thresholds.max_bad_lock_signal = std::stod(value);
        }
    }
    return thresholds;
}

CaptureAnalysisGateResult EvaluateCaptureAnalysisGate(const CaptureFrameAnalysis& analysis) {
    return EvaluateCaptureAnalysisGate(analysis, {});
}

CaptureAnalysisGateResult EvaluateCaptureAnalysisGate(const CaptureFrameAnalysis& analysis,
                                                      const CaptureAnalysisGateThresholds& thresholds) {
    CaptureAnalysisGateResult gate;
    if (!analysis.ok) {
        gate.reason = analysis.error;
        return gate;
    }
    auto fail = [&](const std::string& reason) {
        gate.reason = reason;
        gate.passed = false;
        return gate;
    };
    if (analysis.motion_region_history_trusted_pct > thresholds.max_motion_history_trusted_pct) {
        return fail("motion region trusted history above threshold");
    }
    if (analysis.static_region_history_trusted_pct < thresholds.min_static_history_trusted_pct) {
        return fail("static region trusted history below threshold");
    }
    if (analysis.text_region.samples > 0 && analysis.text_region.history_trusted_pct < thresholds.min_text_history_trusted_pct) {
        return fail("text ROI trusted history below threshold");
    }
    if (analysis.specular_region.samples > 0 && analysis.specular_region.history_trusted_pct > thresholds.max_specular_history_trusted_pct) {
        return fail("specular ROI trusted history above threshold");
    }
    if (analysis.transparent_region.samples > 0 && analysis.transparent_region.history_trusted_pct > thresholds.max_transparent_history_trusted_pct) {
        return fail("transparent ROI trusted history above threshold");
    }
    if (analysis.reactive_region.samples > 0 && analysis.reactive_region.history_trusted_pct > thresholds.max_reactive_history_trusted_pct) {
        return fail("reactive ROI trusted history above threshold");
    }
    if (analysis.color_residual.over_threshold_pct > thresholds.max_color_reject_candidate_pct) {
        return fail("color residual candidate rejection above threshold");
    }
    if (analysis.feature_lock_strength.samples > 0 &&
        analysis.text_region.samples > 0 &&
        analysis.locked_detail.score < thresholds.min_locked_detail_score) {
        return fail("locked detail score below threshold");
    }
    if (analysis.feature_lock_strength.samples > 0 &&
        analysis.locked_detail.bad_lock_signal > thresholds.max_bad_lock_signal) {
        return fail("bad lock signal above threshold");
    }
    gate.passed = true;
    gate.reason = "ok";
    return gate;
}

bool WriteCaptureAnalysisJson(const CaptureFrameAnalysis& analysis,
                              const CaptureAnalysisGateResult& gate,
                              const CaptureAnalysisGateThresholds& thresholds,
                              const std::filesystem::path& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out) {
        return false;
    }
    out.imbue(std::locale::classic());
    const auto write_value_stats = [&](const char* name, const CaptureValueStats& stats, bool trailing_comma) {
        out << "    \"" << name << "\": {"
            << "\"samples\":" << stats.samples << ","
            << "\"min\":" << stats.min << ","
            << "\"max\":" << stats.max << ","
            << "\"mean\":" << stats.mean << ","
            << "\"threshold\":" << stats.threshold << ","
            << "\"over_threshold_pct\":" << stats.over_threshold_pct
            << "}" << (trailing_comma ? "," : "") << "\n";
    };
    const auto write_region_stats = [&](const char* name, const CaptureRegionStats& stats, bool trailing_comma) {
        out << "    \"" << name << "\": {"
            << "\"samples\":" << stats.samples << ","
            << "\"mean_history\":" << stats.mean_history << ","
            << "\"history_trusted_pct\":" << stats.history_trusted_pct << ","
            << "\"mean_color_residual\":" << stats.mean_color_residual << ","
            << "\"mean_feature_lock\":" << stats.mean_feature_lock
            << "}" << (trailing_comma ? "," : "") << "\n";
    };

    out << "{\n";
    out << "  \"schema\": \"osr.capture.analysis.v1\",\n";
    out << "  \"ok\": " << (analysis.ok ? "true" : "false") << ",\n";
    out << "  \"error\": \"" << JsonEscape(analysis.error) << "\",\n";
    out << "  \"frame_id\": " << analysis.frame_id << ",\n";
    out << "  \"display_size\": [" << analysis.display_size.width << ", " << analysis.display_size.height << "],\n";
    out << "  \"render_size\": [" << analysis.render_size.width << ", " << analysis.render_size.height << "],\n";
    out << "  \"gate\": {"
        << "\"passed\":" << (gate.passed ? "true" : "false") << ","
        << "\"reason\":\"" << JsonEscape(gate.reason) << "\""
        << "},\n";
    out << "  \"thresholds\": {"
        << "\"max_motion_history_trusted_pct\":" << thresholds.max_motion_history_trusted_pct << ","
        << "\"min_static_history_trusted_pct\":" << thresholds.min_static_history_trusted_pct << ","
        << "\"min_text_history_trusted_pct\":" << thresholds.min_text_history_trusted_pct << ","
        << "\"max_specular_history_trusted_pct\":" << thresholds.max_specular_history_trusted_pct << ","
        << "\"max_transparent_history_trusted_pct\":" << thresholds.max_transparent_history_trusted_pct << ","
        << "\"max_reactive_history_trusted_pct\":" << thresholds.max_reactive_history_trusted_pct << ","
        << "\"max_color_reject_candidate_pct\":" << thresholds.max_color_reject_candidate_pct << ","
        << "\"min_locked_detail_score\":" << thresholds.min_locked_detail_score << ","
        << "\"max_bad_lock_signal\":" << thresholds.max_bad_lock_signal
        << "},\n";
    out << "  \"global\": {\n";
    write_value_stats("history_weight", analysis.history_weight, true);
    write_value_stats("color_residual", analysis.color_residual, true);
    write_value_stats("depth_residual", analysis.depth_residual, true);
    write_value_stats("feature_lock_strength", analysis.feature_lock_strength, true);
    write_value_stats("motion_magnitude", analysis.motion_magnitude, false);
    out << "  },\n";
    out << "  \"motion_static_split\": {"
        << "\"motion_region_history_trusted_pct\":" << analysis.motion_region_history_trusted_pct << ","
        << "\"static_region_history_trusted_pct\":" << analysis.static_region_history_trusted_pct << ","
        << "\"motion_region_mean_history\":" << analysis.motion_region_mean_history << ","
        << "\"static_region_mean_history\":" << analysis.static_region_mean_history
        << "},\n";
    out << "  \"locked_detail\": {"
        << "\"text_output_contrast\":" << analysis.locked_detail.text_output_contrast << ","
        << "\"text_spatial_contrast\":" << analysis.locked_detail.text_spatial_contrast << ","
        << "\"text_contrast_ratio\":" << analysis.locked_detail.text_contrast_ratio << ","
        << "\"text_lock_signal\":" << analysis.locked_detail.text_lock_signal << ","
        << "\"bad_lock_signal\":" << analysis.locked_detail.bad_lock_signal << ","
        << "\"score\":" << analysis.locked_detail.score
        << "},\n";
    out << "  \"regions\": {\n";
    write_region_stats("text", analysis.text_region, true);
    write_region_stats("static_text", analysis.static_text_region, true);
    write_region_stats("moving_text", analysis.moving_text_region, true);
    write_region_stats("specular", analysis.specular_region, true);
    write_region_stats("transparent", analysis.transparent_region, true);
    write_region_stats("reactive", analysis.reactive_region, false);
    out << "  }\n";
    out << "}\n";
    return true;
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
        << " feature_lock_mean=" << analysis.feature_lock_strength.mean
        << " feature_lock_active_pct=" << analysis.feature_lock_strength.over_threshold_pct
        << " motion_mean=" << analysis.motion_magnitude.mean
        << " motion_active_pct=" << analysis.motion_magnitude.over_threshold_pct
        << " motion_history_mean=" << analysis.motion_region_mean_history
        << " motion_history_trusted_pct=" << analysis.motion_region_history_trusted_pct
        << " static_history_mean=" << analysis.static_region_mean_history
        << " static_history_trusted_pct=" << analysis.static_region_history_trusted_pct
        << " text_samples=" << analysis.text_region.samples
        << " text_history_mean=" << analysis.text_region.mean_history
        << " text_history_trusted_pct=" << analysis.text_region.history_trusted_pct
        << " text_feature_lock_mean=" << analysis.text_region.mean_feature_lock
        << " static_text_samples=" << analysis.static_text_region.samples
        << " static_text_history_trusted_pct=" << analysis.static_text_region.history_trusted_pct
        << " static_text_feature_lock_mean=" << analysis.static_text_region.mean_feature_lock
        << " moving_text_samples=" << analysis.moving_text_region.samples
        << " moving_text_history_trusted_pct=" << analysis.moving_text_region.history_trusted_pct
        << " moving_text_feature_lock_mean=" << analysis.moving_text_region.mean_feature_lock
        << " text_output_contrast=" << analysis.locked_detail.text_output_contrast
        << " text_contrast_ratio=" << analysis.locked_detail.text_contrast_ratio
        << " locked_detail_score=" << analysis.locked_detail.score
        << " specular_samples=" << analysis.specular_region.samples
        << " specular_history_mean=" << analysis.specular_region.mean_history
        << " specular_history_trusted_pct=" << analysis.specular_region.history_trusted_pct
        << " transparent_samples=" << analysis.transparent_region.samples
        << " transparent_history_mean=" << analysis.transparent_region.mean_history
        << " transparent_history_trusted_pct=" << analysis.transparent_region.history_trusted_pct
        << " reactive_samples=" << analysis.reactive_region.samples
        << " reactive_history_mean=" << analysis.reactive_region.mean_history
        << " reactive_history_trusted_pct=" << analysis.reactive_region.history_trusted_pct
        << " reactive_feature_lock_mean=" << analysis.reactive_region.mean_feature_lock;
    return out.str();
}

} // namespace osr::debug
