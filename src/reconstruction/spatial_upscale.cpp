#include "reconstruction/spatial_upscale.h"

namespace osr::reconstruction {

bool ValidateSpatialUpscaleInputs(const core::FrameContext& frame) {
    return frame.color_input.IsPresent() &&
           frame.color_output.IsPresent() &&
           frame.render_size.IsValid() &&
           frame.display_size.IsValid();
}

} // namespace osr::reconstruction

