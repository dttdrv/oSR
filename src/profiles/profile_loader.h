#pragma once

#include "profiles/game_profile.h"

#include <filesystem>
#include <optional>

namespace osr::profiles {

[[nodiscard]] std::optional<GameProfile> LoadProfile(const std::filesystem::path& path);

} // namespace osr::profiles

