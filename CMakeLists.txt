cmake_minimum_required(VERSION 3.22)

project(oSR VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(OSR_BUILD_TESTS "Build oSR tests" ON)

add_library(osr_core STATIC
    src/core/config.cpp
    src/core/frame_context.cpp
    src/core/logging.cpp
    src/core/resource_registry.cpp
)
target_include_directories(osr_core PUBLIC src)

add_library(osr_reconstruction STATIC
    src/reconstruction/disocclusion.cpp
    src/reconstruction/history_rejection.cpp
    src/reconstruction/sharpening.cpp
    src/reconstruction/spatial_upscale.cpp
    src/reconstruction/temporal_accumulation.cpp
)
target_include_directories(osr_reconstruction PUBLIC src)
target_link_libraries(osr_reconstruction PUBLIC osr_core)

add_library(osr_dx12 STATIC
    src/backends/dx12/barriers.cpp
    src/backends/dx12/debug_upscale_pass.cpp
    src/backends/dx12/descriptor_helpers.cpp
    src/backends/dx12/dx12_backend.cpp
)
target_include_directories(osr_dx12 PUBLIC src)
target_link_libraries(osr_dx12 PUBLIC osr_core osr_reconstruction)
if(WIN32)
    target_link_libraries(osr_dx12 PUBLIC d3d12 dxgi dxguid)
endif()

add_library(osr_fsr_bridge STATIC
    src/interop/fsr2_bridge/fsr_bridge.cpp
)
target_include_directories(osr_fsr_bridge PUBLIC src)
target_link_libraries(osr_fsr_bridge PUBLIC osr_core osr_dx12)

add_library(osr_vendor_stubs STATIC
    src/interop/dlss_bridge/dlss_bridge.cpp
    src/interop/xess_bridge/xess_bridge.cpp
)
target_include_directories(osr_vendor_stubs PUBLIC src)
target_link_libraries(osr_vendor_stubs PUBLIC osr_core)

add_library(osr_profiles STATIC
    src/profiles/game_profile.cpp
    src/profiles/profile_loader.cpp
)
target_include_directories(osr_profiles PUBLIC src)
target_link_libraries(osr_profiles PUBLIC osr_core)

add_library(osr_debug STATIC
    src/debug/capture_dump.cpp
    src/debug/overlay.cpp
    src/debug/validation.cpp
)
target_include_directories(osr_debug PUBLIC src)
target_link_libraries(osr_debug PUBLIC osr_core)

if(OSR_BUILD_TESTS)
    enable_testing()
    add_executable(osr_frame_context_tests
        src/tests/frame_context_validation_tests.cpp
    )
    target_link_libraries(osr_frame_context_tests PRIVATE osr_core osr_debug)
    add_test(NAME frame_context_validation COMMAND osr_frame_context_tests)
endif()

