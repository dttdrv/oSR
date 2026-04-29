#include "interop/fsr2_bridge/fsr_bridge.h"

#include "core/logging.h"

#include <sstream>

namespace osr::interop::fsr2_bridge {

namespace {

FsrBridge& BridgeInstance() {
    static FsrBridge bridge;
    return bridge;
}

bool HasFlag(uint32_t flags, FfxBridgeCreateFlags flag) {
    return (flags & static_cast<uint32_t>(flag)) != 0;
}

void LogValidation(uint64_t frame_id, const core::ValidationReport& report) {
    for (const auto& message : report.messages) {
        core::LogLevel level = core::LogLevel::Info;
        if (message.severity == core::ValidationSeverity::Warning) {
            level = core::LogLevel::Warning;
        } else if (message.severity == core::ValidationSeverity::Error) {
            level = core::LogLevel::Error;
        }

        core::Logger::Instance().Log(level, frame_id, "fsr_bridge.validation",
            message.code + ": " + message.message);
    }
}

} // namespace

core::ResourceDesc ToResourceDesc(const FfxBridgeResource& resource, core::ResourceKind kind) {
    return {
        kind,
        resource.native_resource,
        resource.debug_id,
        {resource.width, resource.height},
        resource.format,
        core::ToString(kind),
        resource.state,
        resource.provenance != nullptr ? resource.provenance : ""
    };
}

BridgeContextHandle FsrBridge::CreateContext(const FfxBridgeCreateDesc& desc) {
    const BridgeContextHandle handle = next_handle_++;
    ContextState state;
    state.create_desc = desc;
    state.backend.Initialize(desc.native_device);
    contexts_[handle] = std::move(state);

    std::ostringstream message;
    message << "Created FFX-style context handle=" << handle
            << " max_render=" << desc.max_render_width << "x" << desc.max_render_height
            << " display=" << desc.display_width << "x" << desc.display_height;
    core::Logger::Instance().Log(core::LogLevel::Info, 0, "fsr_bridge", message.str());

    return handle;
}

bool FsrBridge::DestroyContext(BridgeContextHandle handle) {
    const auto erased = contexts_.erase(handle);
    core::Logger::Instance().Log(core::LogLevel::Info, 0, "fsr_bridge",
        erased > 0 ? "Destroyed FFX-style context." : "Destroy requested for unknown FFX-style context.");
    return erased > 0;
}

bool FsrBridge::Dispatch(BridgeContextHandle handle, const FfxBridgeDispatchDesc& desc) {
    auto it = contexts_.find(handle);
    if (it == contexts_.end()) {
        core::Logger::Instance().Log(core::LogLevel::Error, 0, "fsr_bridge", "Dispatch requested for unknown context.");
        return false;
    }

    auto& context = it->second;
    auto frame = NormalizeFrame(context, desc);
    const auto report = core::ValidateFrameContext(frame);
    LogValidation(frame.frame_id, report);

    std::ostringstream message;
    message << "FrameContext source=" << frame.source_api
            << " render=" << frame.render_size.width << "x" << frame.render_size.height
            << " display=" << frame.display_size.width << "x" << frame.display_size.height
            << " jitter=(" << frame.jitter_offset.x << "," << frame.jitter_offset.y << ")"
            << " mv_scale=(" << frame.motion_vector_scale.x << "," << frame.motion_vector_scale.y << ")"
            << " reset=" << (frame.flags.reset_history ? "true" : "false")
            << " exposure_texture=" << (frame.exposure.exposure_texture.has_value() ? "true" : "false");
    core::Logger::Instance().Log(core::LogLevel::Info, frame.frame_id, "fsr_bridge", message.str());

    last_frame_ = frame;
    if (report.HasErrors()) {
        return false;
    }
    return context.backend.DispatchDebugUpscale(desc.native_command_list, frame);
}

std::optional<core::FrameContext> FsrBridge::LastFrame() const {
    return last_frame_;
}

core::FrameContext FsrBridge::NormalizeFrame(ContextState& context, const FfxBridgeDispatchDesc& desc) const {
    const auto& create = context.create_desc;
    core::FrameContext frame;
    frame.frame_id = ++context.frame_counter;
    frame.source_api = "ffx_dx12_bridge";
    frame.color_input = ToResourceDesc(desc.color, core::ResourceKind::ColorInput);
    frame.color_output = ToResourceDesc(desc.output, core::ResourceKind::ColorOutput);
    frame.depth = ToResourceDesc(desc.depth, core::ResourceKind::Depth);
    frame.motion_vectors = ToResourceDesc(desc.motion_vectors, core::ResourceKind::MotionVectors);

    if (desc.reactive.native_resource != nullptr || desc.reactive.debug_id != 0) {
        frame.reactive_mask = ToResourceDesc(desc.reactive, core::ResourceKind::ReactiveMask);
    }
    if (desc.transparency_and_composition.native_resource != nullptr || desc.transparency_and_composition.debug_id != 0) {
        frame.transparency_and_composition_mask =
            ToResourceDesc(desc.transparency_and_composition, core::ResourceKind::TransparencyAndCompositionMask);
    }
    if (desc.exposure.native_resource != nullptr || desc.exposure.debug_id != 0) {
        frame.exposure.exposure_texture = ToResourceDesc(desc.exposure, core::ResourceKind::Exposure);
    }

    frame.render_size = {desc.render_width, desc.render_height};
    frame.upscale_size = {desc.output_width, desc.output_height};
    frame.display_size = {desc.output_width, desc.output_height};
    if (!frame.display_size.IsValid()) {
        frame.display_size = {create.display_width, create.display_height};
    }
    if (!frame.upscale_size.IsValid()) {
        frame.upscale_size = frame.display_size;
    }
    frame.frame_time_delta_ms = desc.frame_time_delta_ms;
    frame.camera.near_plane = desc.camera_near;
    frame.camera.far_plane = desc.camera_far;
    frame.camera.vertical_fov_radians = desc.camera_fov_y_radians;
    frame.camera.view_space_to_meters = desc.view_space_to_meters;
    frame.reconstruction.sharpness = desc.sharpness;
    frame.reconstruction.sharpening_enabled = desc.sharpening_enabled;
    frame.reconstruction.debug_view_enabled = desc.debug_view_enabled;

    frame.jitter_offset = {desc.jitter_x, desc.jitter_y};
    frame.motion_vector_scale = {desc.motion_vector_scale_x, desc.motion_vector_scale_y};
    frame.motion_vector_space = core::MotionVectorSpace::Pixel;
    frame.color_space = HasFlag(create.flags, FfxBridgeCreateFlagHighDynamicRange)
        ? core::ColorSpace::LinearHdr
        : core::ColorSpace::LinearSdr;
    frame.exposure.auto_exposure = HasFlag(create.flags, FfxBridgeCreateFlagAutoExposure);
    frame.exposure.exposure_scale = desc.exposure_scale;
    frame.exposure.pre_exposure = desc.pre_exposure;
    frame.flags.reset_history = desc.reset;
    frame.flags.high_dynamic_range = HasFlag(create.flags, FfxBridgeCreateFlagHighDynamicRange);
    frame.flags.input_color_nonlinear = frame.color_space == core::ColorSpace::NonLinearSrgb;
    frame.flags.output_color_nonlinear = frame.color_space == core::ColorSpace::NonLinearSrgb;
    frame.flags.depth_inverted = HasFlag(create.flags, FfxBridgeCreateFlagDepthInverted);
    frame.flags.depth_infinite = HasFlag(create.flags, FfxBridgeCreateFlagDepthInfinite);
    frame.flags.motion_vectors_jittered = HasFlag(create.flags, FfxBridgeCreateFlagMotionVectorsJittered);
    frame.flags.display_resolution_motion_vectors =
        HasFlag(create.flags, FfxBridgeCreateFlagDisplayResolutionMotionVectors);
    return frame;
}

extern "C" OSR_FFX_BRIDGE_EXPORT BridgeContextHandle osrFfxCreateContext(const FfxBridgeCreateDesc* desc) {
    if (desc == nullptr) {
        return 0;
    }
    return BridgeInstance().CreateContext(*desc);
}

extern "C" OSR_FFX_BRIDGE_EXPORT bool osrFfxDispatch(BridgeContextHandle handle, const FfxBridgeDispatchDesc* desc) {
    if (desc == nullptr) {
        return false;
    }
    return BridgeInstance().Dispatch(handle, *desc);
}

extern "C" OSR_FFX_BRIDGE_EXPORT bool osrFfxDestroyContext(BridgeContextHandle handle) {
    return BridgeInstance().DestroyContext(handle);
}

} // namespace osr::interop::fsr2_bridge
