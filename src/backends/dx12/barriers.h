#pragma once

#include "core/frame_context.h"

#include <string>

namespace osr::backends::dx12 {

enum class ResourceAccess {
    Unknown,
    ShaderRead,
    UnorderedAccess,
    CopySource,
    CopyDest
};

[[nodiscard]] const char* ToString(ResourceAccess access) noexcept;
[[nodiscard]] std::string DescribeTransition(const core::ResourceDesc& resource, ResourceAccess before, ResourceAccess after);

} // namespace osr::backends::dx12

