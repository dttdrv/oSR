#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <string>

namespace osr::interop::xess_bridge {

constexpr uint32_t kXessQualityUltraPerformance = 100;
constexpr uint32_t kXessQualityPerformance = 101;
constexpr uint32_t kXessQualityBalanced = 102;
constexpr uint32_t kXessQualityQuality = 103;
constexpr uint32_t kXessQualityUltraQuality = 104;
constexpr uint32_t kXessQualityUltraQualityPlus = 105;
constexpr uint32_t kXessQualityNative = 106;

constexpr uint32_t kXessInitHighResMv = 1u << 0u;
constexpr uint32_t kXessInitInvertedDepth = 1u << 1u;
constexpr uint32_t kXessInitExposureScaleTexture = 1u << 2u;
constexpr uint32_t kXessInitResponsivePixelMask = 1u << 3u;
constexpr uint32_t kXessInitUseNdcVelocity = 1u << 4u;
constexpr uint32_t kXessInitExternalDescriptorHeap = 1u << 5u;
constexpr uint32_t kXessInitLdrInputColor = 1u << 6u;
constexpr uint32_t kXessInitJitteredMv = 1u << 7u;
constexpr uint32_t kXessInitEnableAutoExposure = 1u << 8u;

struct Xess2D {
    uint32_t x = 0;
    uint32_t y = 0;
};

using XessCoord = Xess2D;

struct XessVkImageSubresourceRange {
    uint32_t aspect_mask = 0;
    uint32_t base_mip_level = 0;
    uint32_t level_count = 0;
    uint32_t base_array_layer = 0;
    uint32_t layer_count = 0;
};

struct XessVkImageViewInfo {
    uint64_t image_view = 0;
    uint64_t image = 0;
    XessVkImageSubresourceRange subresource_range = {};
    uint32_t format = 0;
    uint32_t width = 0;
    uint32_t height = 0;
};

struct XessVkExecuteParams {
    XessVkImageViewInfo color_texture = {};
    XessVkImageViewInfo velocity_texture = {};
    XessVkImageViewInfo depth_texture = {};
    XessVkImageViewInfo exposure_scale_texture = {};
    XessVkImageViewInfo responsive_pixel_mask_texture = {};
    XessVkImageViewInfo output_texture = {};
    float jitter_offset_x = 0.0f;
    float jitter_offset_y = 0.0f;
    float exposure_scale = 1.0f;
    uint32_t reset_history = 0;
    uint32_t input_width = 0;
    uint32_t input_height = 0;
    XessCoord input_color_base = {};
    XessCoord input_motion_vector_base = {};
    XessCoord input_depth_base = {};
    XessCoord input_responsive_mask_base = {};
    XessCoord reserved0 = {};
    XessCoord output_color_base = {};
};

struct XessVkInitParams {
    Xess2D output_resolution = {};
    uint32_t quality_setting = kXessQualityQuality;
    uint32_t init_flags = 0;
    uint32_t creation_node_mask = 0;
    uint32_t visible_node_mask = 0;
    uint64_t temp_buffer_heap = 0;
    uint64_t buffer_heap_offset = 0;
    uint64_t temp_texture_heap = 0;
    uint64_t texture_heap_offset = 0;
    uint64_t pipeline_cache = 0;
};

struct XessVkRuntimeState {
    bool has_init = false;
    XessVkInitParams init = {};
    core::Float2 velocity_scale = {};
    core::Float2 jitter_scale = {};
    float exposure_multiplier = 1.0f;
};

[[nodiscard]] core::FrameContext NormalizeVkFrameContext(uint64_t frame_id,
                                                         const XessVkRuntimeState& runtime,
                                                         const XessVkExecuteParams& exec);

[[nodiscard]] std::string DescribeVkFrameContext(const core::FrameContext& frame);
[[nodiscard]] const char* XessQualityName(uint32_t quality) noexcept;
[[nodiscard]] float XessQualityScale(uint32_t quality) noexcept;
[[nodiscard]] std::string DescribeXessInitFlags(uint32_t flags);

static_assert(sizeof(Xess2D) == 8);
static_assert(sizeof(XessVkImageSubresourceRange) == 20);
static_assert(sizeof(XessVkImageViewInfo) == 48);
static_assert(sizeof(XessVkExecuteParams) == 360);
static_assert(sizeof(XessVkInitParams) == 64);

} // namespace osr::interop::xess_bridge
