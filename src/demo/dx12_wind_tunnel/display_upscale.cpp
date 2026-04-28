#include "demo/dx12_wind_tunnel/display_upscale.h"

#include <algorithm>

namespace osr::demo::dx12_wind_tunnel {

std::vector<uint32_t> UpscaleNearest(const std::vector<uint32_t>& src,
                                     core::Dimensions src_size,
                                     core::Dimensions dst_size) {
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

} // namespace osr::demo::dx12_wind_tunnel
