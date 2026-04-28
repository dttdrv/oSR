#include "demo/dx12_wind_tunnel/display_upscale.h"

#include <algorithm>
#include <cmath>

namespace osr::demo::dx12_wind_tunnel {

namespace {

uint8_t Channel(uint32_t color, uint32_t shift) noexcept {
    return static_cast<uint8_t>((color >> shift) & 0xffu);
}

uint32_t BilinearSample(const std::vector<uint32_t>& src,
                        core::Dimensions src_size,
                        float sx,
                        float sy) noexcept {
    sx = std::clamp(sx, 0.0f, static_cast<float>(src_size.width - 1));
    sy = std::clamp(sy, 0.0f, static_cast<float>(src_size.height - 1));
    const auto x0 = static_cast<uint32_t>(sx);
    const auto y0 = static_cast<uint32_t>(sy);
    const auto x1 = std::min(x0 + 1, src_size.width - 1);
    const auto y1 = std::min(y0 + 1, src_size.height - 1);
    const float tx = sx - static_cast<float>(x0);
    const float ty = sy - static_cast<float>(y0);
    const auto at = [&](uint32_t x, uint32_t y) {
        return src[static_cast<size_t>(y) * src_size.width + x];
    };
    const auto sample_channel = [&](uint32_t shift) {
        const float c00 = static_cast<float>(Channel(at(x0, y0), shift));
        const float c10 = static_cast<float>(Channel(at(x1, y0), shift));
        const float c01 = static_cast<float>(Channel(at(x0, y1), shift));
        const float c11 = static_cast<float>(Channel(at(x1, y1), shift));
        const float top = c00 + (c10 - c00) * tx;
        const float bottom = c01 + (c11 - c01) * tx;
        return static_cast<uint32_t>(std::clamp(std::round(top + (bottom - top) * ty), 0.0f, 255.0f));
    };
    return 0xff000000u | (sample_channel(16) << 16) | (sample_channel(8) << 8) | sample_channel(0);
}

bool Invalid(const std::vector<uint32_t>& src, core::Dimensions src_size, core::Dimensions dst_size) noexcept {
    return !src_size.IsValid() ||
           !dst_size.IsValid() ||
           src.size() != static_cast<size_t>(src_size.width) * src_size.height;
}

} // namespace

std::vector<uint32_t> UpscaleNearest(const std::vector<uint32_t>& src,
                                     core::Dimensions src_size,
                                     core::Dimensions dst_size) {
    if (Invalid(src, src_size, dst_size)) {
        return {};
    }
    std::vector<uint32_t> dst(static_cast<size_t>(dst_size.width) * dst_size.height);
    for (uint32_t y = 0; y < dst_size.height; ++y) {
        const uint32_t sy = std::min(src_size.height - 1, static_cast<uint32_t>((static_cast<uint64_t>(y) * src_size.height) / dst_size.height));
        for (uint32_t x = 0; x < dst_size.width; ++x) {
            const uint32_t sx = std::min(src_size.width - 1, static_cast<uint32_t>((static_cast<uint64_t>(x) * src_size.width) / dst_size.width));
            dst[static_cast<size_t>(y) * dst_size.width + x] = src[static_cast<size_t>(sy) * src_size.width + sx];
        }
    }
    return dst;
}

std::vector<uint32_t> UpscaleBilinear(const std::vector<uint32_t>& src,
                                      core::Dimensions src_size,
                                      core::Dimensions dst_size) {
    return UpscaleBilinearJittered(src, src_size, dst_size, {});
}

std::vector<uint32_t> UpscaleBilinearJittered(const std::vector<uint32_t>& src,
                                              core::Dimensions src_size,
                                              core::Dimensions dst_size,
                                              core::Float2 jitter_offset) {
    if (Invalid(src, src_size, dst_size)) {
        return {};
    }
    std::vector<uint32_t> dst(static_cast<size_t>(dst_size.width) * dst_size.height);
    const float scale_x = static_cast<float>(src_size.width) / static_cast<float>(dst_size.width);
    const float scale_y = static_cast<float>(src_size.height) / static_cast<float>(dst_size.height);
    for (uint32_t y = 0; y < dst_size.height; ++y) {
        const float sy = (static_cast<float>(y) + 0.5f) * scale_y - 0.5f - jitter_offset.y;
        for (uint32_t x = 0; x < dst_size.width; ++x) {
            const float sx = (static_cast<float>(x) + 0.5f) * scale_x - 0.5f - jitter_offset.x;
            dst[static_cast<size_t>(y) * dst_size.width + x] = BilinearSample(src, src_size, sx, sy);
        }
    }
    return dst;
}

} // namespace osr::demo::dx12_wind_tunnel
