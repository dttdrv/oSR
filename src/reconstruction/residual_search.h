#pragma once

#include <cstdint>
#include <vector>

namespace osr::reconstruction {

struct SearchGrid {
    uint32_t width = 0;
    uint32_t height = 0;
    std::vector<float> current_luma;
    std::vector<float> history_luma;
    std::vector<float> current_depth;
    std::vector<float> history_depth;
};

struct ResidualSearchSettings {
    int radius = 1;
    float luma_weight = 1.0f;
    float depth_weight = 4.0f;
    float motion_prior_weight = 0.05f;
};

struct ResidualSearchResult {
    int offset_x = 0;
    int offset_y = 0;
    float score = 0.0f;
    bool valid = false;
};

[[nodiscard]] bool IsValidGrid(const SearchGrid& grid) noexcept;
[[nodiscard]] ResidualSearchResult SearchHistoryResidual(const SearchGrid& grid,
                                                         uint32_t x,
                                                         uint32_t y,
                                                         int predicted_offset_x,
                                                         int predicted_offset_y,
                                                         const ResidualSearchSettings& settings) noexcept;

} // namespace osr::reconstruction

