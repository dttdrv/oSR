#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/logging.h"

#include <filesystem>
#include <mutex>
#include <sstream>
#include <string>

namespace {

using XessResult = int;

constexpr XessResult kXessProxyError = -1;

struct ProxyState {
    std::once_flag init_once;
    HMODULE real_module = nullptr;
    std::filesystem::path module_dir;
    bool attempted_load = false;
};

ProxyState& State() {
    static ProxyState state;
    return state;
}

std::string Ptr(const void* value) {
    std::ostringstream out;
    out << value;
    return out.str();
}

void Log(osr::core::LogLevel level, const std::string& message) {
    osr::core::Logger::Instance().Log(level, 0, "xess_proxy", message);
}

std::filesystem::path ThisModulePath() {
    HMODULE module = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS |
                                GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                            reinterpret_cast<LPCWSTR>(&ThisModulePath),
                            &module)) {
        return {};
    }
    wchar_t path[MAX_PATH] {};
    const DWORD count = GetModuleFileNameW(module, path, MAX_PATH);
    if (count == 0 || count >= MAX_PATH) {
        return {};
    }
    return std::filesystem::path(path);
}

void Initialize() {
    auto& state = State();
    const auto module_path = ThisModulePath();
    state.module_dir = module_path.empty() ? std::filesystem::current_path() : module_path.parent_path();
    wchar_t env_log[MAX_PATH] {};
    const DWORD env_count = GetEnvironmentVariableW(L"OSR_XESS_PROXY_LOG", env_log, MAX_PATH);
    const auto log_path = env_count > 0 && env_count < MAX_PATH
        ? std::filesystem::path(env_log)
        : state.module_dir / "osr_logs" / "osr_xess_proxy.log";
    std::filesystem::create_directories(log_path.parent_path());
    osr::core::Logger::Instance().Configure(log_path,
                                            osr::core::LogLevel::Debug);
    Log(osr::core::LogLevel::Info, "oSR XeSS proxy loaded from " + module_path.string());

    const auto real_path = state.module_dir / "libxess_real.dll";
    state.attempted_load = true;
    state.real_module = LoadLibraryExW(real_path.wstring().c_str(),
                                       nullptr,
                                       LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR |
                                           LOAD_LIBRARY_SEARCH_APPLICATION_DIR |
                                           LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (state.real_module) {
        Log(osr::core::LogLevel::Info, "loaded real XeSS runtime: " + real_path.string());
    } else {
        std::ostringstream out;
        out << "could not load " << real_path.string() << " GetLastError=" << GetLastError();
        Log(osr::core::LogLevel::Warning, out.str());
    }
}

void EnsureInitialized() {
    std::call_once(State().init_once, Initialize);
}

FARPROC Resolve(const char* name) {
    EnsureInitialized();
    auto& state = State();
    if (!state.real_module) {
        Log(osr::core::LogLevel::Warning, std::string(name) + " called without libxess_real.dll loaded");
        return nullptr;
    }
    FARPROC proc = GetProcAddress(state.real_module, name);
    if (!proc) {
        Log(osr::core::LogLevel::Warning, std::string(name) + " missing from libxess_real.dll");
    }
    return proc;
}

template <typename Fn, typename... Args>
XessResult ForwardResult(const char* name, Args... args) {
    auto* proc = reinterpret_cast<Fn>(Resolve(name));
    if (!proc) {
        return kXessProxyError;
    }
    return proc(args...);
}

} // namespace

extern "C" {

__declspec(dllexport) XessResult xessGetVersion(void* version) {
    Log(osr::core::LogLevel::Debug, "xessGetVersion version=" + Ptr(version));
    using Fn = XessResult (*)(void*);
    return ForwardResult<Fn>("xessGetVersion", version);
}

__declspec(dllexport) XessResult xessGetIntelXeFXVersion(void* version) {
    Log(osr::core::LogLevel::Debug, "xessGetIntelXeFXVersion version=" + Ptr(version));
    using Fn = XessResult (*)(void*);
    return ForwardResult<Fn>("xessGetIntelXeFXVersion", version);
}

__declspec(dllexport) XessResult xessIsOptimalDriver(void* context) {
    Log(osr::core::LogLevel::Debug, "xessIsOptimalDriver context=" + Ptr(context));
    using Fn = XessResult (*)(void*);
    return ForwardResult<Fn>("xessIsOptimalDriver", context);
}

__declspec(dllexport) XessResult xessGetProperties(void* context, const void* output_resolution, void* properties) {
    Log(osr::core::LogLevel::Debug, "xessGetProperties context=" + Ptr(context) + " output_resolution=" + Ptr(output_resolution) + " properties=" + Ptr(properties));
    using Fn = XessResult (*)(void*, const void*, void*);
    return ForwardResult<Fn>("xessGetProperties", context, output_resolution, properties);
}

__declspec(dllexport) XessResult xessGetInputResolution(void* context, const void* output_resolution, unsigned int quality, void* input_resolution) {
    std::ostringstream out;
    out << "xessGetInputResolution context=" << context
        << " output_resolution=" << output_resolution
        << " quality=" << quality
        << " input_resolution=" << input_resolution;
    Log(osr::core::LogLevel::Debug, out.str());
    using Fn = XessResult (*)(void*, const void*, unsigned int, void*);
    return ForwardResult<Fn>("xessGetInputResolution", context, output_resolution, quality, input_resolution);
}

__declspec(dllexport) XessResult xessGetOptimalInputResolution(void* context,
                                                               const void* output_resolution,
                                                               unsigned int quality,
                                                               void* input_resolution_optimal,
                                                               void* input_resolution_min,
                                                               void* input_resolution_max) {
    std::ostringstream out;
    out << "xessGetOptimalInputResolution context=" << context
        << " output_resolution=" << output_resolution
        << " quality=" << quality
        << " optimal=" << input_resolution_optimal
        << " min=" << input_resolution_min
        << " max=" << input_resolution_max;
    Log(osr::core::LogLevel::Debug, out.str());
    using Fn = XessResult (*)(void*, const void*, unsigned int, void*, void*, void*);
    return ForwardResult<Fn>("xessGetOptimalInputResolution",
                             context,
                             output_resolution,
                             quality,
                             input_resolution_optimal,
                             input_resolution_min,
                             input_resolution_max);
}

__declspec(dllexport) XessResult xessForceLegacyScaleFactors(void* context, int force) {
    std::ostringstream out;
    out << "xessForceLegacyScaleFactors context=" << context << " force=" << force;
    Log(osr::core::LogLevel::Info, out.str());
    using Fn = XessResult (*)(void*, int);
    return ForwardResult<Fn>("xessForceLegacyScaleFactors", context, force);
}

__declspec(dllexport) XessResult xessGetJitterScale(void* context, float* x, float* y) {
    Log(osr::core::LogLevel::Debug, "xessGetJitterScale context=" + Ptr(context));
    using Fn = XessResult (*)(void*, float*, float*);
    return ForwardResult<Fn>("xessGetJitterScale", context, x, y);
}

__declspec(dllexport) XessResult xessSetJitterScale(void* context, float x, float y) {
    std::ostringstream out;
    out << "xessSetJitterScale context=" << context << " x=" << x << " y=" << y;
    Log(osr::core::LogLevel::Info, out.str());
    using Fn = XessResult (*)(void*, float, float);
    return ForwardResult<Fn>("xessSetJitterScale", context, x, y);
}

__declspec(dllexport) XessResult xessGetVelocityScale(void* context, float* x, float* y) {
    Log(osr::core::LogLevel::Debug, "xessGetVelocityScale context=" + Ptr(context));
    using Fn = XessResult (*)(void*, float*, float*);
    return ForwardResult<Fn>("xessGetVelocityScale", context, x, y);
}

__declspec(dllexport) XessResult xessSetVelocityScale(void* context, float x, float y) {
    std::ostringstream out;
    out << "xessSetVelocityScale context=" << context << " x=" << x << " y=" << y;
    Log(osr::core::LogLevel::Info, out.str());
    using Fn = XessResult (*)(void*, float, float);
    return ForwardResult<Fn>("xessSetVelocityScale", context, x, y);
}

__declspec(dllexport) XessResult xessSetExposureMultiplier(void* context, float value) {
    std::ostringstream out;
    out << "xessSetExposureMultiplier context=" << context << " value=" << value;
    Log(osr::core::LogLevel::Info, out.str());
    using Fn = XessResult (*)(void*, float);
    return ForwardResult<Fn>("xessSetExposureMultiplier", context, value);
}

__declspec(dllexport) XessResult xessSetLoggingCallback(void* context, void* callback, void* user_data) {
    Log(osr::core::LogLevel::Info, "xessSetLoggingCallback context=" + Ptr(context) + " callback=" + Ptr(callback) + " user_data=" + Ptr(user_data));
    using Fn = XessResult (*)(void*, void*, void*);
    return ForwardResult<Fn>("xessSetLoggingCallback", context, callback, user_data);
}

__declspec(dllexport) XessResult xessStartDump(void* context, const void* dump_parameters) {
    Log(osr::core::LogLevel::Info, "xessStartDump context=" + Ptr(context) + " dump_parameters=" + Ptr(dump_parameters));
    using Fn = XessResult (*)(void*, const void*);
    return ForwardResult<Fn>("xessStartDump", context, dump_parameters);
}

__declspec(dllexport) XessResult xessDestroyContext(void* context) {
    Log(osr::core::LogLevel::Info, "xessDestroyContext context=" + Ptr(context));
    using Fn = XessResult (*)(void*);
    return ForwardResult<Fn>("xessDestroyContext", context);
}

__declspec(dllexport) const char* xessResultToString(XessResult result) {
    std::ostringstream out;
    out << "xessResultToString result=" << result;
    Log(osr::core::LogLevel::Debug, out.str());
    using Fn = const char* (*)(XessResult);
    auto* proc = reinterpret_cast<Fn>(Resolve("xessResultToString"));
    if (!proc) {
        return "oSR XeSS proxy error";
    }
    return proc(result);
}

__declspec(dllexport) XessResult xessD3D12CreateContext(void* device, void* out_context) {
    Log(osr::core::LogLevel::Info, "xessD3D12CreateContext device=" + Ptr(device) + " out_context=" + Ptr(out_context));
    using Fn = XessResult (*)(void*, void*);
    return ForwardResult<Fn>("xessD3D12CreateContext", device, out_context);
}

__declspec(dllexport) XessResult xessD3D12Init(void* context, const void* init_params) {
    Log(osr::core::LogLevel::Info, "xessD3D12Init context=" + Ptr(context) + " init_params=" + Ptr(init_params));
    using Fn = XessResult (*)(void*, const void*);
    return ForwardResult<Fn>("xessD3D12Init", context, init_params);
}

__declspec(dllexport) XessResult xessD3D12Execute(void* context, void* command_list, const void* execute_params) {
    Log(osr::core::LogLevel::Info, "xessD3D12Execute context=" + Ptr(context) + " command_list=" + Ptr(command_list) + " execute_params=" + Ptr(execute_params));
    using Fn = XessResult (*)(void*, void*, const void*);
    return ForwardResult<Fn>("xessD3D12Execute", context, command_list, execute_params);
}

__declspec(dllexport) XessResult xessD3D12BuildPipelines(void* context, void* pipeline_library, int blocking, unsigned int init_flags) {
    std::ostringstream out;
    out << "xessD3D12BuildPipelines context=" << context
        << " pipeline_library=" << pipeline_library
        << " blocking=" << blocking
        << " init_flags=" << init_flags;
    Log(osr::core::LogLevel::Info, out.str());
    using Fn = XessResult (*)(void*, void*, int, unsigned int);
    return ForwardResult<Fn>("xessD3D12BuildPipelines", context, pipeline_library, blocking, init_flags);
}

__declspec(dllexport) XessResult xessGetPipelineBuildStatus(void* context, void* status) {
    Log(osr::core::LogLevel::Debug, "xessGetPipelineBuildStatus context=" + Ptr(context) + " status=" + Ptr(status));
    using Fn = XessResult (*)(void*, void*);
    return ForwardResult<Fn>("xessGetPipelineBuildStatus", context, status);
}

__declspec(dllexport) XessResult xessVKCreateContext(void* instance, void* physical_device, void* device, void* out_context) {
    Log(osr::core::LogLevel::Info, "xessVKCreateContext instance=" + Ptr(instance) + " physical_device=" + Ptr(physical_device) + " device=" + Ptr(device) + " out_context=" + Ptr(out_context));
    using Fn = XessResult (*)(void*, void*, void*, void*);
    return ForwardResult<Fn>("xessVKCreateContext", instance, physical_device, device, out_context);
}

__declspec(dllexport) XessResult xessVKInit(void* context, const void* init_params) {
    Log(osr::core::LogLevel::Info, "xessVKInit context=" + Ptr(context) + " init_params=" + Ptr(init_params));
    using Fn = XessResult (*)(void*, const void*);
    return ForwardResult<Fn>("xessVKInit", context, init_params);
}

__declspec(dllexport) XessResult xessVKExecute(void* context, void* command_buffer, const void* execute_params) {
    Log(osr::core::LogLevel::Info, "xessVKExecute context=" + Ptr(context) + " command_buffer=" + Ptr(command_buffer) + " execute_params=" + Ptr(execute_params));
    using Fn = XessResult (*)(void*, void*, const void*);
    return ForwardResult<Fn>("xessVKExecute", context, command_buffer, execute_params);
}

__declspec(dllexport) XessResult xessVKBuildPipelines(void* context, void* pipeline_cache, int blocking, unsigned int init_flags) {
    std::ostringstream out;
    out << "xessVKBuildPipelines context=" << context
        << " pipeline_cache=" << pipeline_cache
        << " blocking=" << blocking
        << " init_flags=" << init_flags;
    Log(osr::core::LogLevel::Info, out.str());
    using Fn = XessResult (*)(void*, void*, int, unsigned int);
    return ForwardResult<Fn>("xessVKBuildPipelines", context, pipeline_cache, blocking, init_flags);
}

} // extern "C"

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(instance);
    }
    return TRUE;
}
