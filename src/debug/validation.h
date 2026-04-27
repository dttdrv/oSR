#pragma once

#include "core/frame_context.h"

#include <string>

namespace osr::debug {

[[nodiscard]] std::string SummarizeValidation(const core::ValidationReport& report);

} // namespace osr::debug

