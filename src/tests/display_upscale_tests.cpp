#include "demo/dx12_wind_tunnel/display_upscale.h"

#include <iostream>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

uint32_t Red(uint32_t color) noexcept {
    return (color >> 16) & 0xffu;
}

} // namespace

int main() {
    const std::vector<uint32_t> src {
        0xff000000u, 0xffffffffu,
        0xffffffffu, 0xff000000u
    };

    const auto nearest = osr::demo::dx12_wind_tunnel::UpscaleNearest(src, {2, 2}, {4, 4});
    if (nearest.size() != 16) {
        return Fail("nearest upscale output size mismatch");
    }
    if (nearest[0] != 0xff000000u || nearest[1] != 0xff000000u || nearest[2] != 0xffffffffu) {
        return Fail("nearest upscale should preserve blocky source samples");
    }

    const auto bilinear = osr::demo::dx12_wind_tunnel::UpscaleBilinear(src, {2, 2}, {3, 3});
    if (bilinear.size() != 9) {
        return Fail("bilinear upscale output size mismatch");
    }
    const uint32_t center = Red(bilinear[4]);
    if (center < 126u || center > 129u) {
        return Fail("bilinear upscale center should average diagonal source contrast");
    }
    if (!osr::demo::dx12_wind_tunnel::UpscaleBilinear(src, {0, 2}, {3, 3}).empty()) {
        return Fail("bilinear upscale should reject invalid dimensions");
    }

    const std::vector<uint32_t> ramp {
        0xff000000u, 0xff400000u, 0xff800000u, 0xffc00000u
    };
    const auto unjittered = osr::demo::dx12_wind_tunnel::UpscaleBilinearJittered(ramp, {4, 1}, {4, 1}, {});
    const auto shifted = osr::demo::dx12_wind_tunnel::UpscaleBilinearJittered(ramp, {4, 1}, {4, 1}, {0.5f, 0.0f});
    if (unjittered.size() != 4 || shifted.size() != 4) {
        return Fail("jitter-aware upscale output size mismatch");
    }
    if (Red(shifted[2]) >= Red(unjittered[2])) {
        return Fail("positive jitter should sample earlier source coordinates for unjittered reconstruction");
    }

    return 0;
}
