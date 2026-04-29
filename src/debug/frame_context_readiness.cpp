#include "debug/frame_context_readiness.h"

#include <cmath>
#include <sstream>
#include <utility>

namespace osr::debug {

namespace {

void Add(ReadinessReport& report,
         ReadinessSeverity severity,
         std::string code,
         std::string message) {
    report.items.push_back({severity, std::move(code), std::move(message)});
}

bool SameExtent(core::Dimensions lhs, core::Dimensions rhs) noexcept {
    return lhs.width == rhs.width && lhs.height == rhs.height;
}

bool HasNonZeroExtent(const core::ResourceDesc& resource) noexcept {
    return resource.extent.IsValid();
}

void RequireResource(ReadinessReport& report,
                     const core::ResourceDesc& resource,
                     core::ResourceKind kind,
                     const char* code) {
    if (!resource.IsPresent()) {
        Add(report, ReadinessSeverity::Error, code,
            std::string("Required SR resource missing: ") + core::ToString(kind));
        return;
    }
    if (!HasNonZeroExtent(resource)) {
        Add(report, ReadinessSeverity::Error, std::string(code) + "_extent",
            std::string("Required SR resource has no extent: ") + core::ToString(kind));
    }
}

void RequireExtent(ReadinessReport& report,
                   const core::ResourceDesc& resource,
                   core::Dimensions expected,
                   const char* code,
                   const char* label) {
    if (resource.IsPresent() && expected.IsValid() && !SameExtent(resource.extent, expected)) {
        std::ostringstream out;
        out << label << " extent " << resource.extent.width << "x" << resource.extent.height
            << " does not match expected " << expected.width << "x" << expected.height;
        Add(report, ReadinessSeverity::Error, code, out.str());
    }
}

} // namespace

ReadinessReport EvaluateSrHarnessReadiness(const core::FrameContext& frame) {
    ReadinessReport report;

    RequireResource(report, frame.color_input, core::ResourceKind::ColorInput, "missing_color_input");
    RequireResource(report, frame.color_output, core::ResourceKind::ColorOutput, "missing_color_output");
    RequireResource(report, frame.depth, core::ResourceKind::Depth, "missing_depth");
    RequireResource(report, frame.motion_vectors, core::ResourceKind::MotionVectors, "missing_motion_vectors");

    if (!frame.render_size.IsValid()) {
        Add(report, ReadinessSeverity::Error, "invalid_render_size", "Render size is required for SR harness runs.");
    }
    if (!frame.upscale_size.IsValid()) {
        Add(report, ReadinessSeverity::Error, "invalid_upscale_size", "Upscale/output size is required for SR harness runs.");
    }
    if (!frame.display_size.IsValid()) {
        Add(report, ReadinessSeverity::Error, "invalid_display_size", "Display size is required for SR harness runs.");
    }

    RequireExtent(report, frame.color_input, frame.render_size, "color_extent_mismatch", "Color input");
    RequireExtent(report, frame.depth, frame.render_size, "depth_extent_mismatch", "Depth");
    RequireExtent(report, frame.color_output, frame.upscale_size, "output_extent_mismatch", "Output color");

    const auto expected_mv_extent = frame.flags.display_resolution_motion_vectors
        ? frame.display_size
        : frame.render_size;
    RequireExtent(report, frame.motion_vectors, expected_mv_extent, "motion_vector_extent_mismatch", "Motion vectors");

    if (!frame.reactive_mask.has_value() || !frame.reactive_mask->IsPresent()) {
        Add(report, ReadinessSeverity::Error, "missing_reactive_mask",
            "Harness frames must include a reactive/responsive mask so alpha history can be tested.");
    } else {
        RequireExtent(report, *frame.reactive_mask, frame.render_size, "reactive_mask_extent_mismatch", "Reactive mask");
    }

    if (!frame.exposure.exposure_texture.has_value() || !frame.exposure.exposure_texture->IsPresent()) {
        Add(report, ReadinessSeverity::Error, "missing_exposure_texture",
            "Harness frames must include exposure metadata/texture coverage for HDR and auto-exposure stress tests.");
    }

    if (frame.motion_vector_space == core::MotionVectorSpace::Unknown) {
        Add(report, ReadinessSeverity::Error, "unknown_motion_vector_space",
            "Motion-vector space must be explicit; the harness must not guess pixel vs NDC velocity.");
    }
    const bool zero_mv_scale = std::fabs(frame.motion_vector_scale.x) < 0.00001f &&
                               std::fabs(frame.motion_vector_scale.y) < 0.00001f;
    if (zero_mv_scale) {
        Add(report, ReadinessSeverity::Error, "zero_motion_vector_scale",
            "Motion-vector scale must be explicit and non-zero.");
    }

    if (frame.frame_time_delta_ms <= 0.0f) {
        Add(report, ReadinessSeverity::Error, "missing_frame_time_delta",
            "Frame time delta is required for motion clarity and temporal response metrics.");
    }
    if (frame.exposure.exposure_scale <= 0.0f || frame.exposure.pre_exposure <= 0.0f) {
        Add(report, ReadinessSeverity::Error, "invalid_exposure",
            "Exposure scale and pre-exposure must be positive.");
    }
    if (frame.camera.near_plane <= 0.0f || frame.camera.far_plane <= frame.camera.near_plane) {
        Add(report, ReadinessSeverity::Error, "invalid_camera_range",
            "Camera near/far planes must be present for depth and disocclusion testing.");
    }

    bool has_error = false;
    for (const auto& item : report.items) {
        if (item.severity == ReadinessSeverity::Error) {
            has_error = true;
            break;
        }
    }
    report.ready = !has_error;
    if (report.ready) {
        Add(report, ReadinessSeverity::Info, "ready",
            "FrameContext contains the mandatory SR harness inputs and metadata.");
    }
    return report;
}

std::string SummarizeReadiness(const ReadinessReport& report) {
    std::ostringstream out;
    out << (report.ready ? "ready" : "not_ready");
    for (const auto& item : report.items) {
        out << " [" << ToString(item.severity) << ":" << item.code << "] " << item.message;
    }
    return out.str();
}

const char* ToString(ReadinessSeverity severity) noexcept {
    switch (severity) {
    case ReadinessSeverity::Info: return "info";
    case ReadinessSeverity::Warning: return "warning";
    case ReadinessSeverity::Error: return "error";
    default: return "unknown";
    }
}

} // namespace osr::debug
