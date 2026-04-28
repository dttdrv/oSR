#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <vector>

namespace osr::demo::dx12_wind_tunnel {

[[nodiscard]] std::vector<uint32_t> UpscaleNearest(const std::vector<uint32_t>& src,
                                                   core::Dimensions src_size,
                                                   core::Dimensions dst_size);

[[nodiscard]] std::vector<uint32_t> UpscaleBilinear(const std::vector<uint32_t>& src,
                                                    core::Dimensions src_size,
                                                    core::Dimensions dst_size);

[[nodiscard]] std::vector<uint32_t> UpscaleBilinearJittered(const std::vector<uint32_t>& src,
                                                            core::Dimensions src_size,
                                                            core::Dimensions dst_size,
                                                            core::Float2 jitter_offset);

} // namespace osr::demo::dx12_wind_tunnel
