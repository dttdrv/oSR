#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "interop/xess_bridge/xess_vk_frame_context.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

namespace {

int Fail(const char* message) {
    std::cerr << message << "\n";
    return 1;
}

bool Contains(const std::filesystem::path& path, const std::string& needle) {
    std::ifstream in(path, std::ios::binary);
    const std::string text((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    return text.find(needle) != std::string::npos;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        return Fail("usage: osr_xess_proxy_smoke <proxy-dll>");
    }
    const std::filesystem::path dll_path = argv[1];
    const auto log_path = dll_path.parent_path() / "osr_logs" / "osr_xess_proxy.log";
    std::filesystem::remove(log_path);

    HMODULE module = LoadLibraryW(dll_path.wstring().c_str());
    if (!module) {
        return Fail("LoadLibrary failed for proxy DLL");
    }
    using GetVersionFn = int (*)(void*);
    auto* get_version = reinterpret_cast<GetVersionFn>(GetProcAddress(module, "xessGetVersion"));
    if (!get_version) {
        FreeLibrary(module);
        return Fail("xessGetVersion export missing");
    }
    using ExecuteFn = int (*)(void*, void*, const void*);
    auto* vk_execute = reinterpret_cast<ExecuteFn>(GetProcAddress(module, "xessVKExecute"));
    if (!vk_execute) {
        FreeLibrary(module);
        return Fail("xessVKExecute export missing");
    }
    using InitFn = int (*)(void*, const void*);
    auto* vk_init = reinterpret_cast<InitFn>(GetProcAddress(module, "xessVKInit"));
    if (!vk_init) {
        FreeLibrary(module);
        return Fail("xessVKInit export missing");
    }
    using SetScaleFn = int (*)(void*, float, float);
    auto* set_velocity_scale = reinterpret_cast<SetScaleFn>(GetProcAddress(module, "xessSetVelocityScale"));
    if (!set_velocity_scale) {
        FreeLibrary(module);
        return Fail("xessSetVelocityScale export missing");
    }
    using GetInputResolutionFn = int (*)(void*, const void*, unsigned int, void*);
    auto* get_input_resolution = reinterpret_cast<GetInputResolutionFn>(GetProcAddress(module, "xessGetInputResolution"));
    if (!get_input_resolution) {
        FreeLibrary(module);
        return Fail("xessGetInputResolution export missing");
    }
    const int result = get_version(nullptr);
    struct Xess2D {
        unsigned int x = 0;
        unsigned int y = 0;
    };
    const Xess2D output {1920, 1200};
    Xess2D input {};
    (void)get_input_resolution(nullptr, &output, 105, &input);
    void* fake_context = reinterpret_cast<void*>(0x1234);
    osr::interop::xess_bridge::XessVkInitParams init {};
    init.output_resolution = {1920, 1080};
    init.quality_setting = osr::interop::xess_bridge::kXessQualityBalanced;
    init.init_flags = osr::interop::xess_bridge::kXessInitHighResMv |
                      osr::interop::xess_bridge::kXessInitInvertedDepth |
                      osr::interop::xess_bridge::kXessInitResponsivePixelMask;
    (void)vk_init(fake_context, &init);
    (void)set_velocity_scale(fake_context, 1920.0f, -1080.0f);
    osr::interop::xess_bridge::XessVkExecuteParams exec {};
    exec.color_texture.image = 0x100;
    exec.color_texture.width = 1280;
    exec.color_texture.height = 720;
    exec.velocity_texture.image = 0x200;
    exec.velocity_texture.width = 1920;
    exec.velocity_texture.height = 1080;
    exec.depth_texture.image = 0x300;
    exec.depth_texture.width = 1280;
    exec.depth_texture.height = 720;
    exec.responsive_pixel_mask_texture.image = 0x400;
    exec.responsive_pixel_mask_texture.width = 1280;
    exec.responsive_pixel_mask_texture.height = 720;
    exec.output_texture.image = 0x500;
    exec.output_texture.width = 1920;
    exec.output_texture.height = 1080;
    exec.jitter_offset_x = 0.25f;
    exec.jitter_offset_y = -0.25f;
    exec.exposure_scale = 1.0f;
    exec.reset_history = 1;
    exec.input_width = 1280;
    exec.input_height = 720;
    for (int i = 0; i < 18; ++i) {
        (void)vk_execute(fake_context, nullptr, &exec);
    }
    FreeLibrary(module);
    if (result == 0) {
        return Fail("xessGetVersion should fail without libxess_real.dll in smoke test");
    }
    if (!Contains(log_path, "oSR XeSS proxy loaded") ||
        !Contains(log_path, "xessGetVersion") ||
        !Contains(log_path, "quality=105(UltraQualityPlus)") ||
        !Contains(log_path, "output=1920x1200") ||
        !Contains(log_path, "xessVKInit context=") ||
        !Contains(log_path, "quality=102(Balanced)") ||
        !Contains(log_path, "init_flags=0xb(HIGH_RES_MV|INVERTED_DEPTH|RESPONSIVE_PIXEL_MASK)") ||
        !Contains(log_path, "FrameContext source=xess_vk_proxy render=1280x720 display=1920x1080") ||
        !Contains(log_path, "mv_scale=(1920,-1080)") ||
        !Contains(log_path, "xessVKExecute") ||
        !Contains(log_path, "throttling=enabled") ||
        !Contains(log_path, "libxess_real.dll")) {
        return Fail("proxy smoke log missing expected entries");
    }
    return 0;
}
