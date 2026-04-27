#pragma once

namespace osr::interop::dlss_bridge {

class DlssBridge {
public:
    [[nodiscard]] constexpr bool EnabledInV0() const noexcept {
        return false;
    }
};

} // namespace osr::interop::dlss_bridge
