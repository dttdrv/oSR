# Linux Development

oSR is now harness-first and dual-direction: the portable core, CPU reconstruction
lab, capture analysis, quality modes, XeSS metadata normalization, and non-DX12
tests are expected to build on Linux. Windows-only work remains available behind
explicit CMake options.

## elementaryOS Setup

Install a normal C++ toolchain:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git
```

Run the portable test path:

```bash
bash tools/run_linux_core_tests.sh
```

Equivalent manual commands:

```bash
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core
```

## What Builds On Linux Now

- `osr_core`
- `osr_reconstruction`
- `osr_vendor_stubs`
- `osr_profiles`
- `osr_debug`
- `osr_wind_tunnel`
- `osr_sequence_lab`
- `osr_quality_mode_demo`
- `osr_trust_field_demo`
- capture analyzer/compare tools
- portable unit tests for frame contracts, readiness, XeSS metadata, quality
  modes, CPU temporal logic, ROI analysis, and capture packs

## Windows-Only Targets

These remain intentionally gated off on Linux:

- `osr_dx12`
- `osr_fsr_bridge`, because the current FSR bridge is DX12-shaped
- `osr_dx12_wind_tunnel`
- `osr_manual_3d_wind_tunnel`
- `osr_xess_proxy`

Linux graphics backends should be added as separate modules instead of bending
the DX12 backend into a platform abstraction. The likely next backend is a
Vulkan harness/writer path, because it maps to native Linux and Proton testing
more naturally than D3D12.

## Near-Term Linux Harness Direction

1. Keep the CPU sequence lab and capture analyzer identical across Windows and
   Linux.
2. Add a portable interactive harness backend after the current DX12 lab gates
   stay green. SDL2 plus Vulkan is the preferred shape, but it should be kept
   behind `src/backends/vulkan` and `src/demo/vulkan_wind_tunnel`.
3. Treat Proton/game injection as downstream. The current mission is still a
   measurable, inspectable harness that improves reconstruction quality without
   hiding broken inputs.
