#pragma once

#include "core/frame_context.h"

#include <filesystem>

namespace osr::debug {

bool WriteFrameMetadata(const std::filesystem::path& output_path, const core::FrameContext& frame);

} // namespace osr::debug

