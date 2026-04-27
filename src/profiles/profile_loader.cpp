#include "profiles/profile_loader.h"

#include <fstream>
#include <string>

namespace osr::profiles {

std::optional<GameProfile> LoadProfile(const std::filesystem::path& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        return std::nullopt;
    }

    GameProfile profile;
    std::string line;
    while (std::getline(file, line)) {
        const auto separator = line.find('=');
        if (separator == std::string::npos) {
            continue;
        }
        const auto key = line.substr(0, separator);
        const auto value = line.substr(separator + 1);
        if (key == "name") {
            profile.name = value;
        } else if (key == "executable") {
            profile.executable = value;
        } else if (key == "capture_enabled") {
            profile.capture_enabled = value == "true" || value == "1";
        }
    }

    return profile;
}

} // namespace osr::profiles

