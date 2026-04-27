#include "core/config.h"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace osr::core {

namespace {

std::string Trim(std::string value) {
    const auto not_space = [](unsigned char c) { return c != ' ' && c != '\t' && c != '\r' && c != '\n'; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

std::optional<Float2> ParseFloat2(const std::string& value) {
    std::istringstream stream(value);
    Float2 result {};
    char comma = 0;
    if (stream >> result.x >> comma >> result.y && comma == ',') {
        return result;
    }
    return std::nullopt;
}

} // namespace

RuntimeConfig LoadConfig(const std::filesystem::path& path) {
    RuntimeConfig config;
    std::ifstream file(path);
    if (!file.is_open()) {
        return config;
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

        if (key == "bridge_mode") {
            config.bridge_mode = value;
        } else if (key == "log_level") {
            config.log_level = ParseLogLevel(value);
        } else if (key == "capture_enabled") {
            config.capture_enabled = ParseBool(value);
        } else if (key == "debug_overlay_enabled") {
            config.debug_overlay_enabled = ParseBool(value);
        } else if (key == "trust_field_enabled") {
            config.trust_field_enabled = ParseBool(value);
        } else if (key == "reactive_synthesis_enabled") {
            config.reactive_synthesis_enabled = ParseBool(value);
        } else if (key == "trust_decay_rate") {
            config.trust_decay_rate = std::stof(value);
        } else if (key == "depth_consistency_threshold") {
            config.depth_consistency_threshold = std::stof(value);
        } else if (key == "motion_consistency_threshold_pixels") {
            config.motion_consistency_threshold_pixels = std::stof(value);
        } else if (key == "motion_vector_scale_override") {
            config.motion_vector_scale_override = ParseFloat2(value);
        } else if (key == "depth_inverted_override") {
            config.depth_inverted_override = ParseBool(value);
        }
    }

    return config;
}

bool ParseBool(const std::string& value) {
    return value == "1" || value == "true" || value == "yes" || value == "on";
}

} // namespace osr::core
