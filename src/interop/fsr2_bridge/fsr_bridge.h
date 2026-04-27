#pragma once

#include "backends/dx12/dx12_backend.h"
#include "core/frame_context.h"
#include "interop/fsr2_bridge/ffx_types.h"

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace osr::interop::fsr2_bridge {

#if defined(_WIN32)
#define OSR_FFX_BRIDGE_EXPORT __declspec(dllexport)
#else
#define OSR_FFX_BRIDGE_EXPORT
#endif

using BridgeContextHandle = uint64_t;

class FsrBridge {
public:
    BridgeContextHandle CreateContext(const FfxBridgeCreateDesc& desc);
    bool DestroyContext(BridgeContextHandle handle);
    bool Dispatch(BridgeContextHandle handle, const FfxBridgeDispatchDesc& desc);

    [[nodiscard]] std::optional<core::FrameContext> LastFrame() const;

private:
    struct ContextState {
        FfxBridgeCreateDesc create_desc = {};
        backends::dx12::Dx12Backend backend;
        uint64_t frame_counter = 0;
    };

    core::FrameContext NormalizeFrame(ContextState& context, const FfxBridgeDispatchDesc& desc) const;

    BridgeContextHandle next_handle_ = 1;
    std::unordered_map<BridgeContextHandle, ContextState> contexts_;
    std::optional<core::FrameContext> last_frame_;
};

extern "C" {
OSR_FFX_BRIDGE_EXPORT BridgeContextHandle osrFfxCreateContext(const FfxBridgeCreateDesc* desc);
OSR_FFX_BRIDGE_EXPORT bool osrFfxDispatch(BridgeContextHandle handle, const FfxBridgeDispatchDesc* desc);
OSR_FFX_BRIDGE_EXPORT bool osrFfxDestroyContext(BridgeContextHandle handle);
}

} // namespace osr::interop::fsr2_bridge
