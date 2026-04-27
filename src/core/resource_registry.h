#pragma once

#include "core/frame_context.h"

#include <cstdint>
#include <mutex>
#include <optional>
#include <unordered_map>

namespace osr::core {

class ResourceRegistry {
public:
    uint64_t Register(ResourceDesc resource);
    std::optional<ResourceDesc> Lookup(uint64_t id) const;
    void Clear();

private:
    mutable std::mutex mutex_;
    uint64_t next_id_ = 1;
    std::unordered_map<uint64_t, ResourceDesc> resources_;
};

} // namespace osr::core

