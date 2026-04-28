# oSR Engineering Log

Append-only engineering changelog. New entries go at the top of the dated section or under a new date.

## 2026-04-27

### Phase 0 Bootstrap

- Initialized the repository for the first oSR prototype scaffold.
- Decision: v0 targets FSR3.1 / FidelityFX API style DX12 input first.
- Decision: OptiScaler and similar GPL projects are research/reference only; no GPL implementation code is copied into oSR.
- Decision: SDK/sample apps come before real games.
- Decision: v0 is super resolution only. Frame generation, anti-cheat games, DLSS/NVNGX spoofing, XeSS runtime replacement, Vulkan, DX11, and older static FSR2 pattern scanning are out of scope.
- Created modular CMake targets: `osr_core`, `osr_dx12`, `osr_fsr_bridge`, `osr_reconstruction`, `osr_vendor_stubs`, `osr_profiles`, and `osr_debug`.
- Implemented the initial API-neutral `FrameContext`, validation report, structured logger, config loader, resource registry, FFX-style bridge skeleton, DX12 debug upscale metadata path, reconstruction placeholders, profile loader, debug metadata writer, and a frame-context validation test.
- Verification: `cmake --preset ninja-debug` completed successfully with GNU 15.2.0 on Windows.
- Verification: `cmake --build --preset ninja-debug` completed successfully.
- Verification: `ctest --preset ninja-debug` passed `1/1` tests.
- Not run: AMD FidelityFX SDK / FSR3 sample proof-of-life, because the SDK sample is not present in this repository yet.

### SDK Install And Breakthrough Track

- Installed AMD FidelityFX SDK locally at `external/FidelityFX-SDK` for sample-app validation. The checkout is not vendored into oSR history and is ignored by `.gitignore`.
- FidelityFX SDK observed commit: `e236f23`.
- FidelityFX SDK readme identifies the package as AMD FSR SDK 2.2.0 "Redstone", including Super Resolution Temporal 2.3.4, Super Resolution Upscaler 3.1.5, and related FSR frame-generation components.
- Found the DX12 FSR sample solution at `external/FidelityFX-SDK/Samples/Upscalers/FidelityFX_FSR/dx12/FidelityFX_FSR_2022.sln`.
- Not run: FidelityFX sample build, because `msbuild`/`devenv` were not available in the sandbox PATH or standard Visual Studio install locations.
- Added `RESEARCH.md` to define the breakthrough track without overclaiming: oSR targets quality-per-millisecond and motion clarity on Radeon 760M-class hardware through explainable trust-field reconstruction.
- Added first trust-field implementation pieces: `trust_field`, `reactive_mask_synthesis`, trust-field HLSL shader stubs, synthesized reactive-mask shader stub, and trust debug visualization shader stub.
- Added debug/resource plumbing for `TrustField`, `SynthesizedReactiveMask`, `HistoryColor`, debug view modes, and internal descriptor accounting.
- Verification: direct MinGW compile of `src/tests/trust_field_tests.cpp` plus trust/reactive sources succeeded.
- Verification: direct MinGW run of `build/manual/osr_trust_field_tests.exe` passed.
- Verification: direct MinGW compile/run of `build/manual/osr_frame_context_tests.exe` passed.
- Build caveat: after an earlier timed-out `cmake --build --preset ninja-debug`, Ninja stalls before executing real build steps even though `ninja -n` and direct compiler invocations work. Treat this as a generated build-directory/tool lock issue to repair next.

### Current Landscape Research

- NVIDIA publicly states DLSS 4 moved Super Resolution/Ray Reconstruction/DLAA from CNNs to transformer models to improve temporal stability, reduce ghosting, improve detail in motion, and smooth edges. DLSS 4.5 adds a second-generation transformer model and NVIDIA says it uses substantially more compute: <https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/>
- AMD FSR "Redstone" is ML-powered on RDNA 4-class hardware; broad fallback support remains FSR 3.1-class upscaling: <https://www.amd.com/en/products/graphics/technologies/fidelityfx/super-resolution.html>
- TAA/TAAU research frames temporal upscaling as sample accumulation plus history validation. That makes history trust the central quality bottleneck: <https://research.nvidia.com/labs/rtr/publication/yang2020survey/>
- Practical oSR inference: do not run a heavy transformer on Radeon 760M. Borrow the attention idea as deterministic sample selection: confidence maps, bounded patch checks, reactive masks, disocclusion detection, and confidence-gated sharpening.

### Robust Oracle Harness

- Added `src/reconstruction/temporal_oracle.*` as a CPU reference for temporal trust/accumulation decisions.
- Added `src/demo/trust_field_demo.cpp`, producing deterministic CSV-like scenario rows for stable, reactive, disoccluded, and reset cases.
- Added `src/tests/temporal_oracle_tests.cpp` with canonical numeric expectations, reset dominance, disocclusion rejection, reactive rejection, monotonic motion degradation, shimmer variance reduction, and 2000-case deterministic fuzz invariants.
- Extended trust computation with luma/color consistency and explicit `evidence_trust`.
- Important test result: the shimmer variance test initially failed, revealing that pure trust decay prevented stable noisy content from retaining enough history. The policy was corrected by separating current evidence trust from trust memory and allowing strong evidence to rebuild trust.
- Aligned `trust_field_update.hlsl` semantics closer to the CPU oracle by adding color delta, disocclusion, reset, min history weight, trust recovery floor, and full rejection handling.
- Verification: direct MinGW compile/run passed for `osr_trust_field_tests.exe`.
- Verification: direct MinGW compile/run passed for `osr_temporal_oracle_tests.exe`.
- Verification: direct MinGW compile/run passed for `osr_trust_field_demo.exe`; observed output includes stable trust `0.881667`, stable history weight `0.758233`, and full current-frame fallback for reactive/disoccluded/reset scenarios.
- Build caveat remains: `cmake --preset ninja-debug` now stalls during compiler ABI detection in this sandbox even after replacing the generated build directory. Manual compiler verification is currently the reliable local path.

### Tile-Gated Trust Classification

- Added `src/reconstruction/tile_classifier.*` to classify 8x8-style sample tiles into `Stable`, `ShimmerRisk`, `MotionRisk`, `ReactiveRisk`, `DisocclusionRisk`, or `Reset`.
- Added `src/tests/tile_classifier_tests.cpp` covering stable tiles, large motion, reactive pixels, disocclusion ratio, reset dominance, and high luma variance.
- Strengthened tile classifier tests with empty-tile handling, exact threshold boundaries, priority order, partial tiles, low-average-trust shimmer classification, and `ToString` coverage.
- Extended the trust-field demo to print tile-level classifications and stats.
- Verification: direct MinGW compile/run passed for `osr_tile_classifier_tests.exe`.
- Verification: direct MinGW compile/run passed again for `osr_temporal_oracle_tests.exe`.
- Verification: direct MinGW compile/run passed for `osr_trust_field_demo.exe`; demo now prints stable, motion-risk, and reactive-risk tile rows.

### Bounded Residual Search Prototype

- Added `src/reconstruction/residual_search.*`, a CPU prototype for tiny local candidate search around the motion-vector prediction.
- The search scores candidates by luma delta, depth delta, and motion-prior distance. This is the first deterministic, shader-friendly approximation of sparse temporal attention.
- Added `src/tests/residual_search_tests.cpp` covering invalid grids, offset recovery, depth-vs-luma scoring, out-of-bounds rejection, and tie-breaking toward smaller offsets.
- Verification: direct MinGW compile/run passed for `osr_residual_search_tests.exe`.
- Verification: direct MinGW run passed for `osr_tile_classifier_tests.exe`, `osr_temporal_oracle_tests.exe`, `osr_trust_field_tests.exe`, and `osr_trust_field_demo.exe`.

### Confidence-Gated Sharpening

- Extended `src/reconstruction/sharpening.*` with `ConfidenceGatedSharpness`.
- Policy: disabled sharpening returns zero; disocclusions suppress sharpening; reactive pixels reduce sharpening; low-trust pixels strongly reduce sharpening; high-trust opaque pixels preserve base sharpness.
- Added `src/tests/sharpening_tests.cpp` covering clamp behavior, disabled behavior, trust scaling, reactive damping, disocclusion suppression, and monotonic trust response.
- Verification: direct MinGW compile/run passed for `osr_sharpening_tests.exe`.
- Verification: direct MinGW run passed for `osr_residual_search_tests.exe`, `osr_tile_classifier_tests.exe`, `osr_temporal_oracle_tests.exe`, `osr_trust_field_tests.exe`, and `osr_trust_field_demo.exe`.

### Manual Quality Mode Control

- Added `src/core/quality_mode.*` for DLSS-style resolution presets and custom render-scale slider values.
- Presets for 1920x1200 output currently resolve to Native 1920x1200, UltraQuality 1478x924, Quality 1280x800, Balanced 1114x696, Performance 960x600, and UltraPerformance 640x400.
- Added `src/tests/quality_mode_tests.cpp` and `src/demo/quality_mode_demo.cpp`.
- `RuntimeConfig` now carries `quality_mode` and optional `custom_render_scale`.
- Verification: direct MinGW compile/run passed for `osr_quality_mode_tests.exe`.
- Verification: direct MinGW compile/run passed for `osr_quality_mode_demo.exe`; demo printed preset and slider render sizes.

### Persistent Manual Launchers

- Added `src/demo/manual_console.cpp`, an interactive console menu that stays open and lets a tester inspect quality modes, enter custom render-scale values, change display resolution, and run trust/tile scenarios.
- Added double-clickable launchers:
  - `tools/run_manual_console.bat`
  - `tools/run_quality_mode_demo.bat`
  - `tools/run_manual_tests.bat`
- Updated `README.md` with Explorer-friendly manual testing instructions.
- Verification: direct MinGW compile passed for `osr_manual_console.exe`.
- Verification: scripted console run exercised custom scale, trust oracle, and tile risk scenarios. PowerShell object piping can feed leading blank/unknown options, but the interactive menu remains persistent for normal keyboard use.

### Manual 3D Wind Tunnel

- Added `tools/manual_3d_scene.html`, a standalone WebGL 3D diagnostic scene for manual inspection of edges, thin rails, particles, internal render scale, subpixel jitter, freeze-frame behavior, and simple output debug views.
- Added `tools/run_3d_scene.bat` for double-click launch in Explorer.
- Design reason: Intel's XeSS-SR guidance treats temporal SR validation as an input-debugging problem: expose render/input resolution, output resolution, jitter, motion vectors, depth, responsive masks, and history reset behavior before judging reconstruction quality.
- XeSS-SR source notes captured for harness direction:
  - XeSS-SR accepts low-resolution jittered input color plus motion vectors and depth, then produces target-resolution output: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
  - Intel documents quality preset scaling from Native AA through Ultra Performance and recommends runtime resolution queries instead of hardcoded preset dimensions: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
  - Intel's sample apps expose input color, input velocity, output, and pause controls, which validates the harness direction of visible input/debug modes: <https://github.com/intel/xess/tree/main/samples>
  - XeSS Inspector exposes overrides for exposure, jitter scale, velocity scale, quality level, init flags, HUD buffer overlays, and frame dumps, so oSR should grow equivalent lightweight diagnostics: <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-xess-inspector.html>
- Verification: Node parsed the embedded `manual_3d_scene.html` script successfully with `new Function(...)`.
- Verification: Playwright loaded `http://127.0.0.1:8765/manual_3d_scene.html` from a temporary local `python -m http.server`; page title was `oSR Manual 3D Scene`.
- Current limitation: this WebGL scene does not yet output real depth, motion-vector, or responsive-mask textures. Those belong in the next DX12 harness step.

### Native Manual 3D Wind Tunnel

- Added `src/demo/manual_3d_wind_tunnel.cpp`, a native Win32 `.exe` manual harness with a simple 3D scene, free camera movement, a right-side settings panel, quality presets, render-scale slider, jitter controls, freeze/reset behavior, particles/thin-rail toggles, edge/luma debug views, and readable overlay diagnostics.
- Added `tools/run_3d_wind_tunnel.bat` for double-click build/run using the known working MinGW compiler.
- Added CMake target `osr_manual_3d_wind_tunnel`.
- Design decision: the native harness is now the primary manual visual test. The HTML/WebGL harness remains as a fast browser fallback, but the executable is closer to the future DX12 sample harness and avoids the "opens and closes immediately" console-app problem.
- Verification: direct MinGW compile succeeded for `build/manual/osr_3d_wind_tunnel.exe`, including `gdi32`, `user32`, and `comctl32`.
- Verification: short GUI smoke tests launched `osr_3d_wind_tunnel.exe`, confirmed the process was still running after three seconds, then stopped it.
- Current limitation: the executable uses a deterministic software rasterizer into a Win32 DIB, not DX12. It is intentionally a control/readability/manual-testing harness before the DX12 buffer-producing harness.

### Research Links

- OptiScaler architecture and compatibility model: <https://github.com/optiscaler/OptiScaler>
- AMD FSR Super Resolution upscaler integration: <https://gpuopen.com/manuals/fsr_sdk/techniques/super-resolution-upscaler/>
- AMD FSR3 dispatch description fields: <https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/>
- Intel XeSS-SR integration guide: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- NVIDIA Streamline programming model: <https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md>
- Microsoft DirectSR input model: <https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html>
