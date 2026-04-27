#pragma once

#include "core/frame_context.h"

#include <cstdint>

namespace osr::interop::fsr2_bridge {

enum FfxBridgeCreateFlags : uint32_t {
    FfxBridgeCreateFlagHighDynamicRange = 1u << 0,
    FfxBridgeCreateFlagDepthInverted = 1u << 1,
    FfxBridgeCreateFlagDepthInfinite = 1u << 2,
    FfxBridgeCreateFlagAutoExposure = 1u << 3,
    FfxBridgeCreateFlagMotionVectorsJittered = 1u << 4,
    FfxBridgeCreateFlagDisplayResolutionMotionVectors = 1u << 5
};

struct FfxBridgeResource {
    void* native_resource = nullptr;
    uint64_t debug_id = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t format = 0;
};

struct FfxBridgeCreateDesc {
    void* native_device = nullptr;
    uint32_t max_render_width = 0;
    uint32_t max_render_height = 0;
    uint32_t display_width = 0;
    uint32_t display_height = 0;
    uint32_t flags = 0;
};

struct FfxBridgeDispatchDesc {
    void* native_command_list = nullptr;
    FfxBridgeResource color;
    FfxBridgeResource depth;
    FfxBridgeResource motion_vectors;
    FfxBridgeResource exposure;
    FfxBridgeResource reactive;
    FfxBridgeResource transparency_and_composition;
    FfxBridgeResource output;
    float jitter_x = 0.0f;
    float jitter_y = 0.0f;
    float motion_vector_scale_x = 0.0f;
    float motion_vector_scale_y = 0.0f;
    uint32_t render_width = 0;
    uint32_t render_height = 0;
    uint32_t output_width = 0;
    uint32_t output_height = 0;
    float exposure_scale = 1.0f;
    float pre_exposure = 1.0f;
    bool reset = false;
};

[[nodiscard]] core::ResourceDesc ToResourceDesc(const FfxBridgeResource& resource, core::ResourceKind kind);

} // namespace osr::interop::fsr2_bridge

