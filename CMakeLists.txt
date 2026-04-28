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
    src/core/quality_mode.cpp
    src/core/resource_registry.cpp
)
target_include_directories(osr_core PUBLIC src)

add_library(osr_reconstruction STATIC
    src/reconstruction/disocclusion.cpp
    src/reconstruction/history_rejection.cpp
    src/reconstruction/reactive_mask_synthesis.cpp
    src/reconstruction/residual_search.cpp
    src/reconstruction/sharpening.cpp
    src/reconstruction/spatial_upscale.cpp
    src/reconstruction/temporal_accumulation.cpp
    src/reconstruction/temporal_oracle.cpp
    src/reconstruction/tile_classifier.cpp
    src/reconstruction/trust_field.cpp
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
    src/debug/capture_pack.cpp
    src/debug/overlay.cpp
    src/debug/validation.cpp
)
target_include_directories(osr_debug PUBLIC src)
target_link_libraries(osr_debug PUBLIC osr_core)

add_library(osr_wind_tunnel STATIC
    src/demo/wind_tunnel/synthetic_frame.cpp
)
target_include_directories(osr_wind_tunnel PUBLIC src)
target_link_libraries(osr_wind_tunnel PUBLIC osr_core)

add_executable(osr_trust_field_demo
    src/demo/trust_field_demo.cpp
)
target_link_libraries(osr_trust_field_demo PRIVATE osr_reconstruction)

add_executable(osr_quality_mode_demo
    src/demo/quality_mode_demo.cpp
)
target_link_libraries(osr_quality_mode_demo PRIVATE osr_core)

add_executable(osr_manual_3d_wind_tunnel
    src/demo/manual_3d_wind_tunnel.cpp
)
target_link_libraries(osr_manual_3d_wind_tunnel PRIVATE osr_core osr_wind_tunnel)
if(WIN32)
    target_link_libraries(osr_manual_3d_wind_tunnel PRIVATE user32 gdi32 comctl32)
endif()

add_executable(osr_dx12_wind_tunnel
    src/demo/dx12_wind_tunnel/main.cpp
    src/demo/dx12_wind_tunnel/display_upscale.cpp
    src/demo/dx12_wind_tunnel/dx12_texture_io.cpp
    src/demo/dx12_wind_tunnel/presenter.cpp
)
target_link_libraries(osr_dx12_wind_tunnel PRIVATE osr_core osr_debug osr_wind_tunnel osr_dx12)
if(WIN32)
    target_link_libraries(osr_dx12_wind_tunnel PRIVATE d3d12 dxgi dxguid user32)
endif()

if(OSR_BUILD_TESTS)
    enable_testing()
    add_executable(osr_frame_context_tests
        src/tests/frame_context_validation_tests.cpp
    )
    target_link_libraries(osr_frame_context_tests PRIVATE osr_core osr_debug)
    add_test(NAME frame_context_validation COMMAND osr_frame_context_tests)

    add_executable(osr_trust_field_tests
        src/tests/trust_field_tests.cpp
    )
    target_link_libraries(osr_trust_field_tests PRIVATE osr_core osr_reconstruction)
    add_test(NAME trust_field COMMAND osr_trust_field_tests)

    add_executable(osr_temporal_oracle_tests
        src/tests/temporal_oracle_tests.cpp
    )
    target_link_libraries(osr_temporal_oracle_tests PRIVATE osr_reconstruction)
    add_test(NAME temporal_oracle COMMAND osr_temporal_oracle_tests)

    add_executable(osr_tile_classifier_tests
        src/tests/tile_classifier_tests.cpp
    )
    target_link_libraries(osr_tile_classifier_tests PRIVATE osr_reconstruction)
    add_test(NAME tile_classifier COMMAND osr_tile_classifier_tests)

    add_executable(osr_residual_search_tests
        src/tests/residual_search_tests.cpp
    )
    target_link_libraries(osr_residual_search_tests PRIVATE osr_reconstruction)
    add_test(NAME residual_search COMMAND osr_residual_search_tests)

    add_executable(osr_sharpening_tests
        src/tests/sharpening_tests.cpp
    )
    target_link_libraries(osr_sharpening_tests PRIVATE osr_reconstruction)
    add_test(NAME sharpening COMMAND osr_sharpening_tests)

    add_executable(osr_quality_mode_tests
        src/tests/quality_mode_tests.cpp
    )
    target_link_libraries(osr_quality_mode_tests PRIVATE osr_core)
    add_test(NAME quality_mode COMMAND osr_quality_mode_tests)

    add_executable(osr_wind_tunnel_synthetic_frame_tests
        src/tests/wind_tunnel_synthetic_frame_tests.cpp
    )
    target_link_libraries(osr_wind_tunnel_synthetic_frame_tests PRIVATE osr_core osr_wind_tunnel)
    add_test(NAME wind_tunnel_synthetic_frame COMMAND osr_wind_tunnel_synthetic_frame_tests)

    add_executable(osr_capture_pack_tests
        src/tests/capture_pack_tests.cpp
    )
    target_link_libraries(osr_capture_pack_tests PRIVATE osr_core osr_debug)
    add_test(NAME capture_pack COMMAND osr_capture_pack_tests)
endif()
