#include "interop/xess_bridge/xess_bridge.h"

namespace osr::interop::xess_bridge {

static_assert(!XessBridge{}.EnabledInV0(), "XeSS bridge is intentionally out of scope for v0.");

} // namespace osr::interop::xess_bridge

