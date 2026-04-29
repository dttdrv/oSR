#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
    for (int i = 0; i < 18; ++i) {
        (void)vk_execute(nullptr, nullptr, nullptr);
    }
    FreeLibrary(module);
    if (result == 0) {
        return Fail("xessGetVersion should fail without libxess_real.dll in smoke test");
    }
    if (!Contains(log_path, "oSR XeSS proxy loaded") ||
        !Contains(log_path, "xessGetVersion") ||
        !Contains(log_path, "quality=105(UltraQualityPlus)") ||
        !Contains(log_path, "output=1920x1200") ||
        !Contains(log_path, "xessVKExecute") ||
        !Contains(log_path, "throttling=enabled") ||
        !Contains(log_path, "libxess_real.dll")) {
        return Fail("proxy smoke log missing expected entries");
    }
    return 0;
}
