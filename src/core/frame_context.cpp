#include "core/frame_context.h"

#include <cmath>
#include <utility>

namespace osr::core {

bool Dimensions::IsValid() const noexcept {
    return width > 0 && height > 0;
}

bool ResourceDesc::IsPresent() const noexcept {
    return native_resource != nullptr || debug_id != 0;
}

bool ValidationReport::HasErrors() const noexcept {
    for (const auto& message : messages) {
        if (message.severity == ValidationSeverity::Error) {
            return true;
        }
    }
    return false;
}

bool ValidationReport::HasWarnings() const noexcept {
    for (const auto& message : messages) {
        if (message.severity == ValidationSeverity::Warning) {
            return true;
        }
    }
    return false;
}

namespace {

void Add(ValidationReport& report, ValidationSeverity severity, std::string code, std::string message) {
    report.messages.push_back({severity, std::move(code), std::move(message)});
}

void RequireResource(ValidationReport& report, const ResourceDesc& resource, ResourceKind expected_kind) {
    if (!resource.IsPresent()) {
        Add(report, ValidationSeverity::Error, "missing_resource",
            std::string("Required resource is missing: ") + ToString(expected_kind));
    }
    if (resource.kind != expected_kind) {
        Add(report, ValidationSeverity::Warning, "resource_kind_mismatch",
            std::string("Resource kind mismatch for ") + ToString(expected_kind));
    }
}

} // namespace

ValidationReport ValidateFrameContext(const FrameContext& frame) {
    ValidationReport report;

    if (!frame.render_size.IsValid()) {
        Add(report, ValidationSeverity::Error, "invalid_render_size", "Render size must be non-zero.");
    }
    if (!frame.display_size.IsValid()) {
        Add(report, ValidationSeverity::Error, "invalid_display_size", "Display/output size must be non-zero.");
    }

    RequireResource(report, frame.color_input, ResourceKind::ColorInput);
    RequireResource(report, frame.color_output, ResourceKind::ColorOutput);
    RequireResource(report, frame.depth, ResourceKind::Depth);
    RequireResource(report, frame.motion_vectors, ResourceKind::MotionVectors);

    if (frame.motion_vector_space == MotionVectorSpace::Unknown) {
        Add(report, ValidationSeverity::Warning, "unknown_motion_vector_space",
            "Motion-vector space is unknown; oSR must not silently guess scale or sign.");
    }

    const bool zero_scale = std::fabs(frame.motion_vector_scale.x) < 0.00001f &&
                            std::fabs(frame.motion_vector_scale.y) < 0.00001f;
    if (zero_scale) {
        Add(report, ValidationSeverity::Warning, "zero_motion_vector_scale",
            "Motion-vector scale is zero or unset; temporal reconstruction must remain conservative.");
    }

    if (frame.exposure.pre_exposure <= 0.0f) {
        Add(report, ValidationSeverity::Error, "invalid_pre_exposure", "Pre-exposure must be greater than zero.");
    }

    if (std::fabs(frame.jitter_offset.x) > 0.5f || std::fabs(frame.jitter_offset.y) > 0.5f) {
        Add(report, ValidationSeverity::Warning, "jitter_out_of_expected_range",
            "Jitter is outside the expected [-0.5, 0.5] pixel-space range.");
    }

    if (!frame.reactive_mask.has_value()) {
        Add(report, ValidationSeverity::Info, "reactive_mask_absent",
            "Reactive/responsive mask is absent; transparent particles may need conservative history weights.");
    }

    return report;
}

const char* ToString(ResourceKind kind) noexcept {
    switch (kind) {
    case ResourceKind::ColorInput: return "ColorInput";
    case ResourceKind::ColorOutput: return "ColorOutput";
    case ResourceKind::Depth: return "Depth";
    case ResourceKind::MotionVectors: return "MotionVectors";
    case ResourceKind::Exposure: return "Exposure";
    case ResourceKind::ReactiveMask: return "ReactiveMask";
    case ResourceKind::TransparencyAndCompositionMask: return "TransparencyAndCompositionMask";
    case ResourceKind::HistoryColor: return "HistoryColor";
    case ResourceKind::TrustField: return "TrustField";
    case ResourceKind::SynthesizedReactiveMask: return "SynthesizedReactiveMask";
    case ResourceKind::DebugVisualization: return "DebugVisualization";
    case ResourceKind::Unknown:
    default: return "Unknown";
    }
}

const char* ToString(MotionVectorSpace space) noexcept {
    switch (space) {
    case MotionVectorSpace::Pixel: return "Pixel";
    case MotionVectorSpace::NormalizedDeviceCoordinates: return "NormalizedDeviceCoordinates";
    case MotionVectorSpace::Unknown:
    default: return "Unknown";
    }
}

const char* ToString(ColorSpace color_space) noexcept {
    switch (color_space) {
    case ColorSpace::LinearSdr: return "LinearSdr";
    case ColorSpace::LinearHdr: return "LinearHdr";
    case ColorSpace::ScRgb: return "ScRgb";
    case ColorSpace::NonLinearSrgb: return "NonLinearSrgb";
    case ColorSpace::Unknown:
    default: return "Unknown";
    }
}

} // namespace osr::core
