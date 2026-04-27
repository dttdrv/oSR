#pragma once

namespace osr::debug {

class Overlay {
public:
    void SetEnabled(bool enabled) noexcept {
        enabled_ = enabled;
    }

    [[nodiscard]] bool Enabled() const noexcept {
        return enabled_;
    }

private:
    bool enabled_ = false;
};

} // namespace osr::debug

