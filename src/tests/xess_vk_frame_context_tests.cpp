#include "interop/xess_bridge/xess_vk_frame_context.h"

#include <iostream>
#include <string_view>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

bool Require(bool condition, const char* message) {
    if (!condition) {
        std::cerr << message << "\n";
        return false;
    }
    return true;
}

osr::interop::xess_bridge::XessVkImageViewInfo Image(uint64_t image,
                                                      uint32_t width,
                                                      uint32_t height,
                                                      uint32_t format) {
    osr::interop::xess_bridge::XessVkImageViewInfo info {};
    info.image_view = image + 1000;
    info.image = image;
    info.format = format;
    info.width = width;
    info.height = height;
    return info;
}

} // namespace

int main() {
    using namespace osr::interop::xess_bridge;

    XessVkInitParams init {};
    init.output_resolution = {1920, 1080};
    init.quality_setting = kXessQualityBalanced;
    init.init_flags = kXessInitHighResMv |
                      kXessInitInvertedDepth |
                      kXessInitExposureScaleTexture |
                      kXessInitResponsivePixelMask |
                      kXessInitJitteredMv;

    XessVkRuntimeState runtime {};
    runtime.init = init;
    runtime.has_init = true;
    runtime.velocity_scale = {1920.0f, -1080.0f};

    XessVkExecuteParams exec {};
    exec.color_texture = Image(0x100, 1280, 720, 44);
    exec.velocity_texture = Image(0x200, 1920, 1080, 103);
    exec.depth_texture = Image(0x300, 1280, 720, 126);
    exec.exposure_scale_texture = Image(0x400, 1, 1, 97);
    exec.responsive_pixel_mask_texture = Image(0x500, 1280, 720, 9);
    exec.output_texture = Image(0x600, 1920, 1080, 44);
    exec.jitter_offset_x = 0.25f;
    exec.jitter_offset_y = -0.25f;
    exec.exposure_scale = 1.5f;
    exec.reset_history = 1;
    exec.input_width = 1280;
    exec.input_height = 720;

    const auto frame = NormalizeVkFrameContext(7, runtime, exec);

    if (!Require(frame.frame_id == 7, "frame id should propagate")) return 1;
    if (!Require(frame.source_api == "xess_vk_proxy", "source api should identify XeSS Vulkan proxy")) return 1;
    if (!Require(frame.render_size.width == 1280 && frame.render_size.height == 720, "render size should come from execute input dimensions")) return 1;
    if (!Require(frame.display_size.width == 1920 && frame.display_size.height == 1080, "display size should come from init output resolution")) return 1;
    if (!Require(frame.upscale_size.width == 1920 && frame.upscale_size.height == 1080, "upscale size should match output")) return 1;
    if (!Require(frame.color_input.debug_id == 0x100 && frame.color_output.debug_id == 0x600, "color resources should map from image handles")) return 1;
    if (!Require(frame.depth.debug_id == 0x300 && frame.motion_vectors.debug_id == 0x200, "depth and velocity resources should map from image handles")) return 1;
    if (!Require(frame.reactive_mask.has_value() && frame.reactive_mask->debug_id == 0x500, "responsive mask should become reactive mask")) return 1;
    if (!Require(frame.exposure.exposure_texture.has_value() && frame.exposure.exposure_texture->debug_id == 0x400, "exposure texture should be captured")) return 1;
    if (!Require(frame.exposure.exposure_scale == 1.5f, "execute exposure scale should propagate")) return 1;
    if (!Require(frame.jitter_offset.x == 0.25f && frame.jitter_offset.y == -0.25f, "jitter should propagate")) return 1;
    if (!Require(frame.motion_vector_scale.x == 1920.0f && frame.motion_vector_scale.y == -1080.0f, "stored velocity scale should propagate")) return 1;
    if (!Require(frame.motion_vector_space == osr::core::MotionVectorSpace::Pixel, "pixel velocity convention should be selected without NDC flag")) return 1;
    if (!Require(frame.flags.display_resolution_motion_vectors, "HIGH_RES_MV should set display-resolution MV flag")) return 1;
    if (!Require(frame.flags.depth_inverted, "inverted depth flag should propagate")) return 1;
    if (!Require(frame.flags.motion_vectors_jittered, "jittered MV flag should propagate")) return 1;
    if (!Require(frame.flags.reset_history, "reset history should propagate")) return 1;

    const auto summary = DescribeVkFrameContext(frame);
    if (!Require(summary.find("FrameContext source=xess_vk_proxy") != std::string_view::npos,
                 "summary should identify source")) return 1;
    if (!Require(summary.find("render=1280x720") != std::string_view::npos,
                 "summary should include render size")) return 1;
    if (!Require(summary.find("display=1920x1080") != std::string_view::npos,
                 "summary should include display size")) return 1;
    if (!Require(summary.find("mv_scale=(1920,-1080)") != std::string_view::npos,
                 "summary should include velocity scale")) return 1;

    return 0;
}
