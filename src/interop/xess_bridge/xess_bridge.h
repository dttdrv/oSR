#pragma once

namespace osr::interop::xess_bridge {

class XessBridge {
public:
    [[nodiscard]] constexpr bool EnabledInV0() const noexcept {
        return false;
    }
};

} // namespace osr::interop::xess_bridge
