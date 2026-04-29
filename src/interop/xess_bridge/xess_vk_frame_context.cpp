#include "interop/xess_bridge/xess_vk_frame_context.h"

#include <sstream>

namespace osr::interop::xess_bridge {

namespace {

bool HasFlag(uint32_t flags, uint32_t flag) noexcept {
    return (flags & flag) != 0;
}

bool IsPresent(const XessVkImageViewInfo& image) noexcept {
    return image.image != 0 || image.image_view != 0;
}

void* NativeHandle(uint64_t handle) noexcept {
    return reinterpret_cast<void*>(static_cast<uintptr_t>(handle));
}

core::Dimensions Extent(const XessVkImageViewInfo& image) noexcept {
    return {image.width, image.height};
}

core::ResourceDesc Resource(const XessVkImageViewInfo& image,
                            core::ResourceKind kind,
                            const char* name) {
    core::ResourceDesc desc;
    desc.kind = kind;
    desc.native_resource = NativeHandle(image.image);
    desc.debug_id = image.image;
    desc.extent = Extent(image);
    desc.api_format = image.format;
    desc.debug_name = name;
    desc.provenance = "xess_vk_execute";
    return desc;
}

void AppendFlag(std::ostringstream& out, bool& first, uint32_t flags, uint32_t flag, const char* name) {
    if (!HasFlag(flags, flag)) {
        return;
    }
    if (!first) {
        out << "|";
    }
    first = false;
    out << name;
}

void AddNote(core::FrameContext& frame, const std::string& note) {
    frame.notes.push_back(note);
}

} // namespace

const char* XessQualityName(uint32_t quality) noexcept {
    switch (quality) {
    case kXessQualityUltraPerformance: return "UltraPerformance";
    case kXessQualityPerformance: return "Performance";
    case kXessQualityBalanced: return "Balanced";
    case kXessQualityQuality: return "Quality";
    case kXessQualityUltraQuality: return "UltraQuality";
    case kXessQualityUltraQualityPlus: return "UltraQualityPlus";
    case kXessQualityNative: return "Native";
    default: return "Unknown";
    }
}

float XessQualityScale(uint32_t quality) noexcept {
    switch (quality) {
    case kXessQualityUltraPerformance: return 1.0f / 3.0f;
    case kXessQualityPerformance: return 1.0f / 2.3f;
    case kXessQualityBalanced: return 1.0f / 2.0f;
    case kXessQualityQuality: return 1.0f / 1.7f;
    case kXessQualityUltraQuality: return 1.0f / 1.5f;
    case kXessQualityUltraQualityPlus: return 1.0f / 1.3f;
    case kXessQualityNative: return 1.0f;
    default: return 0.0f;
    }
}

std::string DescribeXessInitFlags(uint32_t flags) {
    if (flags == 0) {
        return "NONE";
    }

    std::ostringstream out;
    bool first = true;
    AppendFlag(out, first, flags, kXessInitHighResMv, "HIGH_RES_MV");
    AppendFlag(out, first, flags, kXessInitInvertedDepth, "INVERTED_DEPTH");
    AppendFlag(out, first, flags, kXessInitExposureScaleTexture, "EXPOSURE_SCALE_TEXTURE");
    AppendFlag(out, first, flags, kXessInitResponsivePixelMask, "RESPONSIVE_PIXEL_MASK");
    AppendFlag(out, first, flags, kXessInitUseNdcVelocity, "USE_NDC_VELOCITY");
    AppendFlag(out, first, flags, kXessInitExternalDescriptorHeap, "EXTERNAL_DESCRIPTOR_HEAP");
    AppendFlag(out, first, flags, kXessInitLdrInputColor, "LDR_INPUT_COLOR");
    AppendFlag(out, first, flags, kXessInitJitteredMv, "JITTERED_MV");
    AppendFlag(out, first, flags, kXessInitEnableAutoExposure, "ENABLE_AUTOEXPOSURE");
    const uint32_t known = kXessInitHighResMv |
                           kXessInitInvertedDepth |
                           kXessInitExposureScaleTexture |
                           kXessInitResponsivePixelMask |
                           kXessInitUseNdcVelocity |
                           kXessInitExternalDescriptorHeap |
                           kXessInitLdrInputColor |
                           kXessInitJitteredMv |
                           kXessInitEnableAutoExposure;
    const uint32_t unknown = flags & ~known;
    if (unknown != 0) {
        if (!first) {
            out << "|";
        }
        out << "UNKNOWN(0x" << std::hex << unknown << ")";
    }
    return out.str();
}

core::FrameContext NormalizeVkFrameContext(uint64_t frame_id,
                                           const XessVkRuntimeState& runtime,
                                           const XessVkExecuteParams& exec) {
    const uint32_t flags = runtime.has_init ? runtime.init.init_flags : 0;

    core::FrameContext frame;
    frame.frame_id = frame_id;
    frame.source_api = "xess_vk_proxy";
    frame.color_input = Resource(exec.color_texture, core::ResourceKind::ColorInput, "xess_vk_color");
    frame.color_output = Resource(exec.output_texture, core::ResourceKind::ColorOutput, "xess_vk_output");
    frame.depth = Resource(exec.depth_texture, core::ResourceKind::Depth, "xess_vk_depth");
    frame.motion_vectors = Resource(exec.velocity_texture, core::ResourceKind::MotionVectors, "xess_vk_velocity");

    if (HasFlag(flags, kXessInitResponsivePixelMask) || IsPresent(exec.responsive_pixel_mask_texture)) {
        frame.reactive_mask =
            Resource(exec.responsive_pixel_mask_texture, core::ResourceKind::ReactiveMask, "xess_vk_responsive_mask");
    }
    if (HasFlag(flags, kXessInitExposureScaleTexture) || IsPresent(exec.exposure_scale_texture)) {
        frame.exposure.exposure_texture =
            Resource(exec.exposure_scale_texture, core::ResourceKind::Exposure, "xess_vk_exposure_scale");
    }

    frame.render_size = {exec.input_width, exec.input_height};
    if (!frame.render_size.IsValid()) {
        frame.render_size = Extent(exec.color_texture);
    }

    if (runtime.has_init && runtime.init.output_resolution.x != 0 && runtime.init.output_resolution.y != 0) {
        frame.display_size = {runtime.init.output_resolution.x, runtime.init.output_resolution.y};
    } else {
        frame.display_size = Extent(exec.output_texture);
    }
    frame.upscale_size = frame.display_size;

    frame.jitter_offset = {exec.jitter_offset_x, exec.jitter_offset_y};
    frame.motion_vector_scale = runtime.velocity_scale;
    frame.motion_vector_space = HasFlag(flags, kXessInitUseNdcVelocity)
        ? core::MotionVectorSpace::NormalizedDeviceCoordinates
        : core::MotionVectorSpace::Pixel;

    frame.color_space = HasFlag(flags, kXessInitLdrInputColor)
        ? core::ColorSpace::LinearSdr
        : core::ColorSpace::LinearHdr;
    frame.exposure.auto_exposure = HasFlag(flags, kXessInitEnableAutoExposure);
    frame.exposure.exposure_scale = exec.exposure_scale * runtime.exposure_multiplier;
    frame.exposure.pre_exposure = 1.0f;

    frame.flags.reset_history = exec.reset_history != 0;
    frame.flags.high_dynamic_range = !HasFlag(flags, kXessInitLdrInputColor);
    frame.flags.depth_inverted = HasFlag(flags, kXessInitInvertedDepth);
    frame.flags.motion_vectors_jittered = HasFlag(flags, kXessInitJitteredMv);
    frame.flags.display_resolution_motion_vectors = HasFlag(flags, kXessInitHighResMv);

    if (runtime.has_init) {
        std::ostringstream note;
        note << "quality=" << runtime.init.quality_setting << "("
             << XessQualityName(runtime.init.quality_setting) << ")"
             << " scale=" << XessQualityScale(runtime.init.quality_setting)
             << " init_flags=" << DescribeXessInitFlags(flags);
        AddNote(frame, note.str());
    } else {
        AddNote(frame, "xessVKExecute observed before xessVKInit; output size inferred from execute texture.");
    }

    if (runtime.velocity_scale.x == 0.0f && runtime.velocity_scale.y == 0.0f) {
        AddNote(frame, "velocity scale was not observed through xessSetVelocityScale; temporal path must remain conservative.");
    }

    return frame;
}

std::string DescribeVkFrameContext(const core::FrameContext& frame) {
    std::ostringstream out;
    out << "FrameContext source=" << frame.source_api
        << " render=" << frame.render_size.width << "x" << frame.render_size.height
        << " display=" << frame.display_size.width << "x" << frame.display_size.height
        << " output=" << frame.upscale_size.width << "x" << frame.upscale_size.height
        << " jitter=(" << frame.jitter_offset.x << "," << frame.jitter_offset.y << ")"
        << " mv_scale=(" << frame.motion_vector_scale.x << "," << frame.motion_vector_scale.y << ")"
        << " reset=" << (frame.flags.reset_history ? "true" : "false")
        << " depth_inverted=" << (frame.flags.depth_inverted ? "true" : "false")
        << " high_res_mv=" << (frame.flags.display_resolution_motion_vectors ? "true" : "false")
        << " exposure_texture=" << (frame.exposure.exposure_texture.has_value() ? "true" : "false")
        << " responsive_mask=" << (frame.reactive_mask.has_value() ? "true" : "false");

    if (!frame.notes.empty()) {
        out << " note=\"" << frame.notes.front() << "\"";
    }
    return out.str();
}

} // namespace osr::interop::xess_bridge
