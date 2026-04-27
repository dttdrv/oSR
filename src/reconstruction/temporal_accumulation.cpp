#include "reconstruction/temporal_accumulation.h"

namespace osr::reconstruction {

bool ShouldResetHistory(const core::FrameContext& previous, const core::FrameContext& current) {
    if (current.flags.reset_history) {
        return true;
    }
    if (!previous.render_size.IsValid() || !previous.display_size.IsValid()) {
        return true;
    }
    return previous.render_size.width != current.render_size.width ||
           previous.render_size.height != current.render_size.height ||
           previous.display_size.width != current.display_size.width ||
           previous.display_size.height != current.display_size.height;
}

} // namespace osr::reconstruction

