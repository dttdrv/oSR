# Development Guide

This guide is for working on oSR without losing the thread of what the prototype
is trying to prove.

## Build Targets

Core portable targets:

- `osr_core`
- `osr_reconstruction`
- `osr_vendor_stubs`
- `osr_profiles`
- `osr_debug`
- `osr_wind_tunnel`
- `osr_sequence_lab`
- `osr_portable_wind_tunnel`
- `osr_capture_analyzer`
- `osr_capture_compare`

Windows-only targets:

- `osr_dx12`
- `osr_fsr_bridge`
- `osr_dx12_wind_tunnel`
- `osr_manual_3d_wind_tunnel`
- `osr_xess_proxy`

## CMake Options

```text
OSR_BUILD_TESTS=ON|OFF
OSR_BUILD_DX12=ON|OFF
OSR_BUILD_WIN32_HARNESS=ON|OFF
OSR_BUILD_XESS_PROXY=ON|OFF
```

Defaults:

- Windows: DX12 and Win32 harness targets default on.
- Linux: DX12 and Win32 harness targets default off.
- XeSS proxy always defaults off because it is an opt-in diagnostic DLL.

## Presets

Portable Linux/core:

```bash
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core
```

Windows Ninja:

```powershell
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug
```

Visual Studio:

```powershell
cmake --preset vs2022-debug
cmake --build --preset vs2022-debug
```

## Required Verification Before Claiming Success

For portable/core changes:

```bash
bash tools/run_linux_core_tests.sh
```

For harness/reconstruction changes:

```powershell
$env:OSR_NO_PAUSE='1'
tools\run_manual_tests.bat
```

For a quality-affecting change:

```powershell
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name <name> --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

For Linux-safe capture evidence:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 16 --display-size 1280x800 --quality quality --capture-run-name <name> --capture-frame 12 --overwrite --metric-gate
```

For motion-vector changes:

```powershell
tools\run_dx12_mv_sweep.bat
```

## Logging And State Discipline

`LOG.md`

- append-only
- record decisions, failures, test results, and source links
- newest entries go at the top of the current dated section or under a new date

`STATE.yaml`

- update when phase, build status, assumptions, unknowns, targets, or next
  actions change
- keep the latest verified command results visible

`ROADMAP.md`

- update pass/fail gates when the definition of done changes

`docs/wiki/source-index.md`

- add source links before making new research-backed claims

## Code Boundaries

Keep platform-specific work isolated:

- DX12 code stays under `src/backends/dx12` or `src/demo/dx12_wind_tunnel`.
- Future Vulkan code should stay under `src/backends/vulkan` or
  `src/demo/vulkan_wind_tunnel`.
- Vendor bridge logic stays under `src/interop/<vendor>_bridge`.
- Shader code stays isolated under `src/reconstruction/shaders` or an explicit
  shader directory.
- Portable harness logic belongs under `src/demo/wind_tunnel` or
  `src/demo/portable_wind_tunnel.cpp`.

Do not create a monolithic harness file that owns rendering, metrics, capture
serialization, reconstruction, and platform APIs at once.

## Quality Rules

- Never silently guess motion-vector scale.
- Treat jitter convention, depth convention, color space, exposure, and
  responsive/reactive masks as first-class inputs.
- Prefer conservative history rejection over visible trails in v0.
- Measure text, thin geometry, transparent regions, specular regions, reactive
  particles, moving regions, and static regions separately.
- A better screenshot is not enough. Use capture packs and metric gates.

## Adding A Reconstruction Change

1. Add or update a focused CPU test where possible.
2. Update the CPU oracle or portable harness first.
3. Add HLSL/GPU parity only after the CPU behavior is understood.
4. Run portable tests.
5. Run a capture gate.
6. Compare before/after captures.
7. Record the result in `LOG.md` and `STATE.yaml`.

## Adding A New Harness Feature

1. Decide if it is portable, DX12-only, or future Vulkan-only.
2. Add CLI controls before UI controls.
3. Serialize the state into capture packs.
4. Add one smoke test and one failure-mode test when practical.
5. Document how a user should run it.

## Current Known Technical Debt

- `display_upscale.*` is portable but still lives under `dx12_wind_tunnel`.
- Runtime HLSL compilation is still used for prototype speed.
- The Linux path lacks an interactive graphics backend.
- The Vulkan writer required for real XeSS takeover does not exist yet.
- Capture gate thresholds are calibrated on synthetic scenes and need more
  scenarios before being treated as universal.
