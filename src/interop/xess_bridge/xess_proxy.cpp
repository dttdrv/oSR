#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "core/logging.h"
#include "interop/xess_bridge/xess_replacement_policy.h"
#include "interop/xess_bridge/xess_vk_frame_context.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <mutex>
#include <unordered_map>
#include <sstream>
#include <string>

namespace {

using XessResult = int;

constexpr XessResult kXessProxyError = -1;
constexpr uint64_t kUnthrottledExecuteLogs = 16;
constexpr uint64_t kExecuteLogInterval = 120;

struct Xess2D {
    uint32_t x = 0;
    uint32_t y = 0;
};

struct ProxyState {
    std::once_flag init_once;
    HMODULE real_module = nullptr;
    std::filesystem::path module_dir;
    bool attempted_load = false;
    std::atomic<uint64_t> d3d12_execute_count {0};
    std::atomic<uint64_t> vk_execute_count {0};
    osr::interop::xess_bridge::XessProxyMode proxy_mode =
        osr::interop::xess_bridge::XessProxyMode::Passthrough;
    std::mutex contexts_mutex;
    std::unordered_map<void*, osr::interop::xess_bridge::XessVkRuntimeState> vk_contexts;
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

const char* QualityName(unsigned int quality) noexcept {
    return osr::interop::xess_bridge::XessQualityName(quality);
}

float QualityScale(unsigned int quality) noexcept {
    return osr::interop::xess_bridge::XessQualityScale(quality);
}

std::string ResolutionString(const void* value) {
    if (!value) {
        return "<null>";
    }
    const auto* resolution = static_cast<const Xess2D*>(value);
    std::ostringstream out;
    out << resolution->x << "x" << resolution->y;
    return out.str();
}

void Log(osr::core::LogLevel level, const std::string& message) {
    osr::core::Logger::Instance().Log(level, 0, "xess_proxy", message);
}

void LogFrame(osr::core::LogLevel level, uint64_t frame_id, const std::string& message) {
    osr::core::Logger::Instance().Log(level, frame_id, "xess_proxy", message);
}

bool ShouldLogExecute(uint64_t count) noexcept {
    return count <= kUnthrottledExecuteLogs || (count % kExecuteLogInterval) == 0;
}

std::string ExecuteThrottleSuffix(uint64_t count) {
    std::ostringstream out;
    out << " execute_count=" << count;
    if (count == kUnthrottledExecuteLogs + 1) {
        out << " throttling=enabled interval=" << kExecuteLogInterval;
    }
    return out.str();
}

osr::interop::xess_bridge::XessVkRuntimeState VkRuntimeFor(void* context) {
    auto& state = State();
    std::lock_guard lock(state.contexts_mutex);
    const auto it = state.vk_contexts.find(context);
    if (it != state.vk_contexts.end()) {
        return it->second;
    }
    return {};
}

void UpdateVkRuntime(void* context, const osr::interop::xess_bridge::XessVkRuntimeState& runtime) {
    auto& state = State();
    std::lock_guard lock(state.contexts_mutex);
    state.vk_contexts[context] = runtime;
}

template <typename Mutator>
void MutateVkRuntime(void* context, Mutator mutator) {
    auto& state = State();
    std::lock_guard lock(state.contexts_mutex);
    mutator(state.vk_contexts[context]);
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
    char mode_env[64] {};
    const DWORD mode_count = GetEnvironmentVariableA("OSR_XESS_MODE", mode_env, static_cast<DWORD>(sizeof(mode_env)));
    state.proxy_mode = osr::interop::xess_bridge::ParseXessProxyMode(
        mode_count > 0 && mode_count < sizeof(mode_env) ? mode_env : nullptr);
    Log(osr::core::LogLevel::Info,
        std::string("xess proxy mode=") + osr::interop::xess_bridge::ToString(state.proxy_mode));

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

bool ReplacementBackendAvailable() noexcept {
    return false;
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
        << " output=" << ResolutionString(output_resolution)
        << " quality=" << quality << "(" << QualityName(quality) << ")"
        << " scale=" << QualityScale(quality)
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
        << " output=" << ResolutionString(output_resolution)
        << " quality=" << quality << "(" << QualityName(quality) << ")"
        << " scale=" << QualityScale(quality)
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
    MutateVkRuntime(context, [x, y](auto& runtime) {
        runtime.jitter_scale = {x, y};
    });
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
    MutateVkRuntime(context, [x, y](auto& runtime) {
        runtime.velocity_scale = {x, y};
    });
    using Fn = XessResult (*)(void*, float, float);
    return ForwardResult<Fn>("xessSetVelocityScale", context, x, y);
}

__declspec(dllexport) XessResult xessSetExposureMultiplier(void* context, float value) {
    std::ostringstream out;
    out << "xessSetExposureMultiplier context=" << context << " value=" << value;
    Log(osr::core::LogLevel::Info, out.str());
    MutateVkRuntime(context, [value](auto& runtime) {
        runtime.exposure_multiplier = value;
    });
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
    EnsureInitialized();
    const uint64_t execute_count = State().d3d12_execute_count.fetch_add(1) + 1;
    if (ShouldLogExecute(execute_count) || execute_count == kUnthrottledExecuteLogs + 1) {
        LogFrame(osr::core::LogLevel::Info,
                 execute_count,
                 "xessD3D12Execute context=" + Ptr(context) +
                     " command_list=" + Ptr(command_list) +
                     " execute_params=" + Ptr(execute_params) +
                     ExecuteThrottleSuffix(execute_count));
    }
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

__declspec(dllexport) XessResult xessVKGetRequiredInstanceExtensions(uint32_t* count,
                                                                      const char* const** extensions,
                                                                      uint32_t* min_vk_api_version) {
    Log(osr::core::LogLevel::Debug, "xessVKGetRequiredInstanceExtensions count=" + Ptr(count) +
                                    " extensions=" + Ptr(extensions) +
                                    " min_vk_api_version=" + Ptr(min_vk_api_version));
    using Fn = XessResult (*)(uint32_t*, const char* const**, uint32_t*);
    return ForwardResult<Fn>("xessVKGetRequiredInstanceExtensions", count, extensions, min_vk_api_version);
}

__declspec(dllexport) XessResult xessVKGetRequiredDeviceExtensions(void* instance,
                                                                    void* physical_device,
                                                                    uint32_t* count,
                                                                    const char* const** extensions) {
    Log(osr::core::LogLevel::Debug, "xessVKGetRequiredDeviceExtensions instance=" + Ptr(instance) +
                                    " physical_device=" + Ptr(physical_device) +
                                    " count=" + Ptr(count) +
                                    " extensions=" + Ptr(extensions));
    using Fn = XessResult (*)(void*, void*, uint32_t*, const char* const**);
    return ForwardResult<Fn>("xessVKGetRequiredDeviceExtensions", instance, physical_device, count, extensions);
}

__declspec(dllexport) XessResult xessVKGetRequiredDeviceFeatures(void* instance,
                                                                 void* physical_device,
                                                                 void** features) {
    Log(osr::core::LogLevel::Debug, "xessVKGetRequiredDeviceFeatures instance=" + Ptr(instance) +
                                    " physical_device=" + Ptr(physical_device) +
                                    " features=" + Ptr(features));
    using Fn = XessResult (*)(void*, void*, void**);
    return ForwardResult<Fn>("xessVKGetRequiredDeviceFeatures", instance, physical_device, features);
}

__declspec(dllexport) XessResult xessVKCreateContext(void* instance, void* physical_device, void* device, void* out_context) {
    Log(osr::core::LogLevel::Info, "xessVKCreateContext instance=" + Ptr(instance) + " physical_device=" + Ptr(physical_device) + " device=" + Ptr(device) + " out_context=" + Ptr(out_context));
    using Fn = XessResult (*)(void*, void*, void*, void*);
    const XessResult result = ForwardResult<Fn>("xessVKCreateContext", instance, physical_device, device, out_context);
    if (result == 0 && out_context != nullptr) {
        void* created_context = *static_cast<void**>(out_context);
        MutateVkRuntime(created_context, [](auto&) {});
        Log(osr::core::LogLevel::Info, "registered XeSS Vulkan context=" + Ptr(created_context));
    }
    return result;
}

__declspec(dllexport) XessResult xessVKInit(void* context, const void* init_params) {
    if (init_params != nullptr) {
        const auto* params = static_cast<const osr::interop::xess_bridge::XessVkInitParams*>(init_params);
        auto runtime = VkRuntimeFor(context);
        runtime.has_init = true;
        runtime.init = *params;
        UpdateVkRuntime(context, runtime);

        std::ostringstream out;
        out << "xessVKInit context=" << context
            << " output=" << params->output_resolution.x << "x" << params->output_resolution.y
            << " quality=" << params->quality_setting << "("
            << osr::interop::xess_bridge::XessQualityName(params->quality_setting) << ")"
            << " scale=" << osr::interop::xess_bridge::XessQualityScale(params->quality_setting)
            << " init_flags=0x" << std::hex << params->init_flags << std::dec
            << "(" << osr::interop::xess_bridge::DescribeXessInitFlags(params->init_flags) << ")"
            << " init_params=" << init_params;
        Log(osr::core::LogLevel::Info, out.str());
    } else {
        Log(osr::core::LogLevel::Info, "xessVKInit context=" + Ptr(context) + " init_params=<null>");
    }
    using Fn = XessResult (*)(void*, const void*);
    return ForwardResult<Fn>("xessVKInit", context, init_params);
}

__declspec(dllexport) XessResult xessVKGetInitParams(void* context, void* init_params) {
    Log(osr::core::LogLevel::Debug, "xessVKGetInitParams context=" + Ptr(context) + " init_params=" + Ptr(init_params));
    using Fn = XessResult (*)(void*, void*);
    return ForwardResult<Fn>("xessVKGetInitParams", context, init_params);
}

__declspec(dllexport) XessResult xessVKExecute(void* context, void* command_buffer, const void* execute_params) {
    EnsureInitialized();
    const uint64_t execute_count = State().vk_execute_count.fetch_add(1) + 1;
    osr::interop::xess_bridge::XessReplacementDecision replacement_decision;
    replacement_decision.reason = "not_evaluated";
    bool has_decoded_frame = false;
    osr::core::FrameContext decoded_frame;
    if (execute_params != nullptr) {
        const auto* params = static_cast<const osr::interop::xess_bridge::XessVkExecuteParams*>(execute_params);
        decoded_frame = osr::interop::xess_bridge::NormalizeVkFrameContext(execute_count,
                                                                           VkRuntimeFor(context),
                                                                           *params);
        has_decoded_frame = true;
        replacement_decision = osr::interop::xess_bridge::EvaluateXessReplacementDecision(State().proxy_mode,
                                                                                         decoded_frame,
                                                                                         true,
                                                                                         command_buffer != nullptr,
                                                                                         ReplacementBackendAvailable());
    } else {
        replacement_decision = osr::interop::xess_bridge::EvaluateXessReplacementDecision(State().proxy_mode,
                                                                                         {},
                                                                                         false,
                                                                                         command_buffer != nullptr,
                                                                                         ReplacementBackendAvailable());
    }
    if (ShouldLogExecute(execute_count) || execute_count == kUnthrottledExecuteLogs + 1) {
        std::string message = "xessVKExecute context=" + Ptr(context) +
                              " command_buffer=" + Ptr(command_buffer) +
                              " execute_params=" + Ptr(execute_params) +
                              ExecuteThrottleSuffix(execute_count);
        if (has_decoded_frame) {
            message += " ";
            message += osr::interop::xess_bridge::DescribeVkFrameContext(decoded_frame);
        }
        message += " replacement_decision={mode=";
        message += osr::interop::xess_bridge::ToString(State().proxy_mode);
        message += " run_osr=";
        message += replacement_decision.run_osr ? "true" : "false";
        message += " forward=";
        message += replacement_decision.forward_to_real_xess ? "true" : "false";
        message += " reason=";
        message += replacement_decision.reason;
        message += "}";
        LogFrame(osr::core::LogLevel::Info, execute_count, message);
    }
    if (replacement_decision.run_osr && !replacement_decision.forward_to_real_xess) {
        LogFrame(osr::core::LogLevel::Error,
                 execute_count,
                 "OSR replacement was selected but no Vulkan writer is linked; forwarding is disabled by policy error.");
        return kXessProxyError;
    }
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
