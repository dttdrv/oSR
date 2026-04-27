#pragma once

namespace osr::backends::dx12 {

class DeviceHooks {
public:
    bool Install(void* native_device) {
        native_device_ = native_device;
        return native_device_ != nullptr;
    }

    [[nodiscard]] bool Installed() const noexcept {
        return native_device_ != nullptr;
    }

private:
    void* native_device_ = nullptr;
};

} // namespace osr::backends::dx12

