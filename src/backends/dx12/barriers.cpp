#include "backends/dx12/barriers.h"

namespace osr::backends::dx12 {

const char* ToString(ResourceAccess access) noexcept {
    switch (access) {
    case ResourceAccess::ShaderRead: return "ShaderRead";
    case ResourceAccess::UnorderedAccess: return "UnorderedAccess";
    case ResourceAccess::CopySource: return "CopySource";
    case ResourceAccess::CopyDest: return "CopyDest";
    case ResourceAccess::Unknown:
    default: return "Unknown";
    }
}

std::string DescribeTransition(const core::ResourceDesc& resource, ResourceAccess before, ResourceAccess after) {
    return std::string("resource=") + core::ToString(resource.kind) +
           " before=" + ToString(before) +
           " after=" + ToString(after);
}

} // namespace osr::backends::dx12

