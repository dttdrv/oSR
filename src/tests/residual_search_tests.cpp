#include "reconstruction/residual_search.h"

#include <iostream>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        std::exit(1);
    }
}

osr::reconstruction::SearchGrid MakeGrid(uint32_t width, uint32_t height) {
    osr::reconstruction::SearchGrid grid;
    grid.width = width;
    grid.height = height;
    const size_t count = static_cast<size_t>(width) * height;
    grid.current_luma.assign(count, 0.1f);
    grid.history_luma.assign(count, 0.9f);
    grid.current_depth.assign(count, 0.5f);
    grid.history_depth.assign(count, 0.5f);
    return grid;
}

uint32_t Index(uint32_t x, uint32_t y, uint32_t width) {
    return y * width + x;
}

} // namespace

int main() {
    using osr::reconstruction::ResidualSearchSettings;
    using osr::reconstruction::SearchHistoryResidual;

    Require(!osr::reconstruction::IsValidGrid({}), "Empty grid should be invalid.");

    auto grid = MakeGrid(5, 5);
    grid.current_luma[Index(2, 2, 5)] = 0.75f;
    grid.history_luma[Index(3, 2, 5)] = 0.75f;
    auto result = SearchHistoryResidual(grid, 2, 2, 0, 0, ResidualSearchSettings{});
    Require(result.valid, "Search should find a valid candidate.");
    Require(result.offset_x == 1 && result.offset_y == 0, "Search should recover +1 x offset.");

    grid.history_luma[Index(3, 2, 5)] = 0.75f;
    grid.history_depth[Index(3, 2, 5)] = 0.1f;
    grid.history_luma[Index(2, 3, 5)] = 0.76f;
    grid.history_depth[Index(2, 3, 5)] = 0.5f;
    result = SearchHistoryResidual(grid, 2, 2, 0, 0, ResidualSearchSettings{});
    Require(result.offset_x == 0 && result.offset_y == 1, "Depth agreement should beat slightly better luma.");

    result = SearchHistoryResidual(grid, 0, 0, -2, -2, ResidualSearchSettings{});
    Require(!result.valid, "Out-of-bounds predicted region should return invalid.");

    auto tie = MakeGrid(3, 3);
    tie.current_luma[Index(1, 1, 3)] = 0.5f;
    tie.history_luma[Index(1, 1, 3)] = 0.5f;
    tie.history_luma[Index(2, 1, 3)] = 0.5f;
    result = SearchHistoryResidual(tie, 1, 1, 0, 0, ResidualSearchSettings{});
    Require(result.offset_x == 0 && result.offset_y == 0, "Tie should prefer smaller offset.");

    return 0;
}

