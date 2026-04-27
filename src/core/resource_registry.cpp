#include "core/resource_registry.h"

namespace osr::core {

uint64_t ResourceRegistry::Register(ResourceDesc resource) {
    std::lock_guard lock(mutex_);
    const uint64_t id = next_id_++;
    resource.debug_id = id;
    resources_[id] = std::move(resource);
    return id;
}

std::optional<ResourceDesc> ResourceRegistry::Lookup(uint64_t id) const {
    std::lock_guard lock(mutex_);
    const auto it = resources_.find(id);
    if (it == resources_.end()) {
        return std::nullopt;
    }
    return it->second;
}

void ResourceRegistry::Clear() {
    std::lock_guard lock(mutex_);
    resources_.clear();
}

} // namespace osr::core

