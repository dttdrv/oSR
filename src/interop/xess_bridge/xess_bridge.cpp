#include "interop/xess_bridge/xess_bridge.h"

namespace osr::interop::xess_bridge {

static_assert(!XessBridge{}.EnabledInV0(), "Full XeSS runtime replacement remains out of v0; the proxy DLL is an opt-in diagnostic bridge.");

} // namespace osr::interop::xess_bridge
