#include "interop/dlss_bridge/dlss_bridge.h"

namespace osr::interop::dlss_bridge {

static_assert(!DlssBridge{}.EnabledInV0(), "DLSS/NVNGX bridge is intentionally out of scope for v0.");

} // namespace osr::interop::dlss_bridge

