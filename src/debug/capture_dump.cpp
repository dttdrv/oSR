#include "debug/capture_dump.h"

#include <fstream>

namespace osr::debug {

bool WriteFrameMetadata(const std::filesystem::path& output_path, const core::FrameContext& frame) {
    std::ofstream file(output_path, std::ios::app);
    if (!file.is_open()) {
        return false;
    }

    file << "frame_id: " << frame.frame_id << "\n";
    file << "source_api: " << frame.source_api << "\n";
    file << "render_size: [" << frame.render_size.width << ", " << frame.render_size.height << "]\n";
    file << "upscale_size: [" << frame.upscale_size.width << ", " << frame.upscale_size.height << "]\n";
    file << "display_size: [" << frame.display_size.width << ", " << frame.display_size.height << "]\n";
    file << "frame_time_delta_ms: " << frame.frame_time_delta_ms << "\n";
    file << "camera: [near=" << frame.camera.near_plane
         << ", far=" << frame.camera.far_plane
         << ", fov_y_rad=" << frame.camera.vertical_fov_radians
         << ", view_space_to_meters=" << frame.camera.view_space_to_meters << "]\n";
    file << "sharpness: " << frame.reconstruction.sharpness << "\n";
    file << "jitter: [" << frame.jitter_offset.x << ", " << frame.jitter_offset.y << "]\n";
    file << "motion_vector_scale: [" << frame.motion_vector_scale.x << ", " << frame.motion_vector_scale.y << "]\n";
    file << "reset_history: " << (frame.flags.reset_history ? "true" : "false") << "\n";
    file << "---\n";
    return true;
}

} // namespace osr::debug
