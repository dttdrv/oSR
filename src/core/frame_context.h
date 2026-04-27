#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace osr::core {

struct Dimensions {
    uint32_t width = 0;
    uint32_t height = 0;

    [[nodiscard]] bool IsValid() const noexcept;
};

struct Float2 {
    float x = 0.0f;
    float y = 0.0f;
};

enum class ResourceKind {
    Unknown,
    ColorInput,
    ColorOutput,
    Depth,
    MotionVectors,
    Exposure,
    ReactiveMask,
    TransparencyAndCompositionMask,
    HistoryColor,
    TrustField,
    SynthesizedReactiveMask,
    DebugVisualization
};

struct ResourceDesc {
    ResourceKind kind = ResourceKind::Unknown;
    void* native_resource = nullptr;
    uint64_t debug_id = 0;
    Dimensions extent = {};
    uint32_t api_format = 0;
    std::string debug_name;

    [[nodiscard]] bool IsPresent() const noexcept;
};

enum class MotionVectorSpace {
    Unknown,
    Pixel,
    NormalizedDeviceCoordinates
};

enum class ColorSpace {
    Unknown,
    LinearSdr,
    LinearHdr,
    ScRgb,
    NonLinearSrgb
};

struct ExposureInfo {
    bool auto_exposure = false;
    float exposure_scale = 1.0f;
    float pre_exposure = 1.0f;
    std::optional<ResourceDesc> exposure_texture;
};

struct FrameFlags {
    bool reset_history = false;
    bool high_dynamic_range = false;
    bool depth_inverted = false;
    bool depth_infinite = false;
    bool motion_vectors_jittered = false;
    bool display_resolution_motion_vectors = false;
};

struct FrameContext {
    uint64_t frame_id = 0;
    std::string source_api;

    ResourceDesc color_input;
    ResourceDesc color_output;
    ResourceDesc depth;
    ResourceDesc motion_vectors;
    std::optional<ResourceDesc> reactive_mask;
    std::optional<ResourceDesc> transparency_and_composition_mask;

    Dimensions render_size = {};
    Dimensions display_size = {};
    Float2 jitter_offset = {};
    Float2 motion_vector_scale = {};
    MotionVectorSpace motion_vector_space = MotionVectorSpace::Unknown;
    ColorSpace color_space = ColorSpace::Unknown;
    ExposureInfo exposure = {};
    FrameFlags flags = {};

    std::vector<std::string> notes;
};

enum class ValidationSeverity {
    Info,
    Warning,
    Error
};

struct ValidationMessage {
    ValidationSeverity severity = ValidationSeverity::Info;
    std::string code;
    std::string message;
};

struct ValidationReport {
    std::vector<ValidationMessage> messages;

    [[nodiscard]] bool HasErrors() const noexcept;
    [[nodiscard]] bool HasWarnings() const noexcept;
};

[[nodiscard]] ValidationReport ValidateFrameContext(const FrameContext& frame);
[[nodiscard]] const char* ToString(ResourceKind kind) noexcept;
[[nodiscard]] const char* ToString(MotionVectorSpace space) noexcept;
[[nodiscard]] const char* ToString(ColorSpace color_space) noexcept;

} // namespace osr::core
