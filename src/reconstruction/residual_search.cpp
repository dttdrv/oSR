#include "reconstruction/residual_search.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace osr::reconstruction {

namespace {

uint32_t Index(uint32_t x, uint32_t y, uint32_t width) noexcept {
    return y * width + x;
}

bool Contains(const SearchGrid& grid, int x, int y) noexcept {
    return x >= 0 && y >= 0 &&
           x < static_cast<int>(grid.width) &&
           y < static_cast<int>(grid.height);
}

float AtOrZero(const std::vector<float>& values, uint32_t index) noexcept {
    return index < values.size() ? values[index] : 0.0f;
}

} // namespace

bool IsValidGrid(const SearchGrid& grid) noexcept {
    const size_t expected = static_cast<size_t>(grid.width) * static_cast<size_t>(grid.height);
    return grid.width > 0 &&
           grid.height > 0 &&
           grid.current_luma.size() == expected &&
           grid.history_luma.size() == expected &&
           grid.current_depth.size() == expected &&
           grid.history_depth.size() == expected;
}

ResidualSearchResult SearchHistoryResidual(const SearchGrid& grid,
                                           uint32_t x,
                                           uint32_t y,
                                           int predicted_offset_x,
                                           int predicted_offset_y,
                                           const ResidualSearchSettings& settings) noexcept {
    if (!IsValidGrid(grid) || x >= grid.width || y >= grid.height || settings.radius < 0) {
        return {};
    }

    const uint32_t current_index = Index(x, y, grid.width);
    const float current_luma = AtOrZero(grid.current_luma, current_index);
    const float current_depth = AtOrZero(grid.current_depth, current_index);

    ResidualSearchResult best;
    best.score = std::numeric_limits<float>::infinity();

    for (int dy = -settings.radius; dy <= settings.radius; ++dy) {
        for (int dx = -settings.radius; dx <= settings.radius; ++dx) {
            const int candidate_offset_x = predicted_offset_x + dx;
            const int candidate_offset_y = predicted_offset_y + dy;
            const int hx = static_cast<int>(x) + candidate_offset_x;
            const int hy = static_cast<int>(y) + candidate_offset_y;
            if (!Contains(grid, hx, hy)) {
                continue;
            }

            const uint32_t history_index = Index(static_cast<uint32_t>(hx), static_cast<uint32_t>(hy), grid.width);
            const float luma_delta = std::fabs(current_luma - AtOrZero(grid.history_luma, history_index));
            const float depth_delta = std::fabs(current_depth - AtOrZero(grid.history_depth, history_index));
            const float prior_distance = static_cast<float>(dx * dx + dy * dy);
            const float score = luma_delta * settings.luma_weight +
                                depth_delta * settings.depth_weight +
                                prior_distance * settings.motion_prior_weight;

            const bool better = score < best.score;
            const bool tie_breaker = score == best.score &&
                (std::abs(candidate_offset_x) + std::abs(candidate_offset_y) <
                 std::abs(best.offset_x) + std::abs(best.offset_y));

            if (better || tie_breaker) {
                best.offset_x = candidate_offset_x;
                best.offset_y = candidate_offset_y;
                best.score = score;
                best.valid = true;
            }
        }
    }

    if (!best.valid) {
        best.score = 0.0f;
    }
    return best;
}

} // namespace osr::reconstruction

