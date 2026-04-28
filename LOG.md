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

### DX12 Wind Tunnel Proof Of Life

- Added reusable `src/demo/wind_tunnel/synthetic_frame.*` to emit deterministic SR inputs: color, depth, current-to-previous pixel motion vectors, reactive mask, jitter, reset flag, dimensions, exposure defaults, and a normalized `FrameContext`.
- Added `src/tests/wind_tunnel_synthetic_frame_tests.cpp` covering render size, buffer sizes, validation, reset propagation, finite MV/depth values, reactive coverage, foreground depth, and jitter bounds.
- Added `src/demo/dx12_wind_tunnel/main.cpp` and `tools/run_dx12_wind_tunnel.bat`.
- The DX12 proof creates a D3D12 device, command queue, command allocator/list, and real `ID3D12Resource` textures for color input, color output, depth, motion vectors, and reactive mask.
- The proof replaces synthetic CPU buffer pointers with D3D12 resource pointers in `FrameContext`, validates the result, calls `Dx12Backend::DispatchDebugUpscale`, and writes readable metadata to `build/manual/osr_dx12_wind_tunnel_metadata.txt`.
- Verification: direct MinGW compile passed for `osr_wind_tunnel_synthetic_frame_tests.exe`.
- Verification: `osr_wind_tunnel_synthetic_frame_tests.exe` passed.
- Verification: direct MinGW compile passed for `osr_dx12_wind_tunnel.exe`.
- Verification: `osr_dx12_wind_tunnel.exe` ran successfully; output reported render `853x533`, display `1280x800`, validation `infos=0 warnings=0 errors=0`.
- Verification: DX12 log recorded `Dispatch debug upscale shader=spatial_debug_upscale.hlsl view=Final groups=(160,100) srv=4 uav=1 cbv=1 internal_uav=1 mode=dx12_command_recording_pending`.
- Current limitation: D3D12 textures are allocated and exported, but CPU upload, swapchain presentation, and actual command-list reconstruction passes are next.

### Research Update: Trust-Guided Sparse Temporal Attention

- Updated `RESEARCH.md` with the current breakthrough thesis: oSR should not be treated as a sharper scaler, but as a low-cost temporal evidence system.
- Source-backed conclusion: modern SR quality is dominated by context/history selection. DLSS transformers buy this with broad learned attention and high compute; oSR should approximate the useful part with deterministic trust fields, tile risk routing, bounded residual search, reactive/disocclusion handling, and confidence-gated sharpening.
- Added concrete experiments for the DX12 wind tunnel: MV truth table, foreground-depth MV dilation, reactive-mask synthesis, trust heatmaps/metrics, risk-tile residual search, GPU confidence-gated sharpening, and optional tiny residual neural refinement only after deterministic trust works.
- Folded in research-agent findings: baseline validators must include YCoCg/neighborhood and luma-variance clamps; SVGF motivates variance-guided trust; ReSTIR/Area ReSTIR motivate selective candidate reuse; Unreal TSR motivates history resurrection as a later experiment.

### Harness Design

- Added `HARNESS.md`, defining the dual-mode harness direction: one DX12 wind-tunnel executable for human eye testing and deterministic headless capture/metrics runs.
- Design source notes: XeSS Inspector motivates overrides, overlays, histograms, and frame dumps; Unreal TSR motivates internal debug views; FSR2 debug checker motivates textual input validation; PIX timing/GPU capture docs motivate GPU markers, timestamps, and debug-layer-clean command recording.
- The harness contract requires capture packs with `session.json`, `frames.csv`, `metrics.csv`, warnings/bookmarks, selected texture dumps, and replayable run configuration.
- Folded in agent research naming the two modes as `Pilot Mode` and `Lab Mode`, with readable scene content, artifact tags, and metric-to-cause rules so eye-test notes become actionable tuning hypotheses.
- Folded in external tooling research: PIX/RGP markers, optional RenderDoc trigger workflow, dump-around-frame captures, and secondary perceptual metrics such as FLIP/VMAF.
- The immediate implementation order is H0/H1: shared run/capture schema, then DX12 buffer upload/readback/hash logging.

### Harness H0/H1 Capture Pack And Buffer Truth

- Added `src/debug/capture_pack.*` with a no-dependency capture-pack writer for `session.json`, `frames.csv`, `metrics.csv`, `warnings.jsonl`, `bookmarks.jsonl`, and per-frame `frame_context.json`.
- Added `src/tests/capture_pack_tests.cpp` covering JSON escaping, CSV escaping, deterministic hashing, file creation, exact headers, warnings JSONL, escaped notes, and duplicate-session behavior.
- Added `src/demo/dx12_wind_tunnel/dx12_texture_io.*` for D3D12 texture upload/readback using `GetCopyableFootprints`, padded row pitches, command-list copy barriers, a fence wait, and canonical unpadded-row hashing.
- Updated the DX12 wind tunnel to upload/readback/hash synthetic color, depth, motion-vector, and reactive-mask textures. Metadata now records format, extent, row size, row pitch, total bytes, CPU hash, GPU hash, and match status.
- Updated `tools/run_dx12_wind_tunnel.bat` and `tools/run_manual_tests.bat` for the new capture and texture I/O modules.
- Verification: direct MinGW compile passed for `osr_capture_pack_tests.exe`.
- Verification: `osr_capture_pack_tests.exe` passed.
- Verification: direct MinGW compile passed for `osr_dx12_wind_tunnel.exe`.
- Verification: `osr_dx12_wind_tunnel.exe` ran successfully and reported `Transfer hashes: matched`.
- Verification details: `color_input`, `depth`, `motion_vectors`, and `reactive_mask` all had matching CPU/GPU hashes. Example run wrote capture pack `build/manual/captures/2026-04-28T11-47-27Z_dx12_wind_tunnel_h1_buffer_truth`.

### DX12 Wind Tunnel H2 Presentation

- Added `src/demo/dx12_wind_tunnel/display_upscale.*` for display-sized nearest upscaling of the synthetic color buffer.
- Added `src/demo/dx12_wind_tunnel/presenter.*` for Win32 window creation, DXGI swapchain creation, backbuffer RTV setup, message pumping, and copying `color_output` to the swapchain backbuffer.
- Updated `src/demo/dx12_wind_tunnel/main.cpp` with `--headless` and `--present-frames N` modes.
- Updated `tools/run_dx12_wind_tunnel.bat` to forward command-line arguments.
- Verification: direct MinGW compile passed for the refactored DX12 wind tunnel with presentation modules.
- Verification: `osr_dx12_wind_tunnel.exe --headless` passed with validation `infos=0 warnings=0 errors=0` and matched transfer hashes.
- Verification: `osr_dx12_wind_tunnel.exe --present-frames 3` opened the DX12 window, presented the display-sized color output, and exited cleanly.
- Current limitation: H2 still uses CPU nearest upscale into `color_output`. The next step is a command-list-recorded GPU debug upscale/copy shader.

### Capture Pack Debug Artifacts

- Added `src/demo/wind_tunnel/debug_dumps.*` to write no-dependency debug artifacts into capture-pack frame folders.
- Frame artifacts now include `color_input.ppm`, `color_output.ppm`, `depth.pgm`, `motion_vectors_magnitude.pgm`, `reactive_mask.pgm`, exact `.raw` buffers, and `artifacts.json`.
- Added `src/tests/wind_tunnel_debug_dumps_tests.cpp` covering PPM/PGM headers, raw motion-vector buffer size, and artifact manifest creation.
- Updated the DX12 wind tunnel to write debug artifacts after H1 upload/readback/hash validation.
- Verification: direct MinGW compile/run passed for `osr_wind_tunnel_debug_dumps_tests.exe`.
- Verification: `osr_dx12_wind_tunnel.exe --headless` wrote frame artifacts; latest checked capture included all expected PPM/PGM/raw files and an `artifacts.json` manifest with hashes.
- Verification: `tools/run_manual_tests.bat` passed after adding the debug dump test.

### Motion-Vector Truth Modes And Temporal Diagnostics

- Added synthetic motion-vector truth-table modes: `correct`, `zero`, `flip-x`, `flip-y`, `half-scale`, `double-scale`, and `jitter-contaminated`.
- Added `--mv-mode` to `osr_dx12_wind_tunnel.exe` so Lab Mode can intentionally corrupt motion vectors while preserving valid color/depth/reactive buffers.
- Added CPU temporal diagnostics that build deterministic previous/current frames, reproject previous color/depth through supplied current-to-previous motion vectors, run the trust-field policy, and write MV residual and history-trust confusion metrics into `metrics.csv` and the readable metadata file.
- Extended capture metrics with `mv_luma_residual_mean`, `mv_luma_residual_p95`, `mv_depth_residual_mean`, `mv_depth_residual_p95`, `bad_history_trusted_pct`, `good_history_rejected_pct`, `reactive_history_trusted_pct`, `disocclusion_history_trusted_pct`, `trust_evidence_agreement_pct`, `history_trust_mean`, and `accumulation_weight_mean`.
- Added tests for synthetic MV mode transforms and temporal diagnostics.
- Verification: `tools/run_manual_tests.bat` passed.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --mv-mode flip-x` passed with validation `infos=0 warnings=0 errors=0`, matched transfer hashes, and wrote capture pack `build/manual/captures/2026-04-28T12-13-27Z_dx12_wind_tunnel_h1_buffer_truth_flip-x`.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --mv-mode correct` passed with validation `infos=0 warnings=0 errors=0`, matched transfer hashes, and wrote capture pack `build/manual/captures/2026-04-28T12-14-02Z_dx12_wind_tunnel_h1_buffer_truth_correct`.

### Temporal Diagnostic Verdicts And MV Sweep

- Added `AnalyzeTemporalDiagnostics`, producing compact findings with tag, likely cause, suggested action, severity, and evidence value.
- Added capture-pack diagnostic warning serialization to `warnings.jsonl`.
- Added `--metric-gate` / `--fail-on-diagnostics` to the DX12 wind tunnel. Severe diagnostic findings now return exit code `3` after logs and capture packs are written.
- Added `tools/run_dx12_mv_sweep.bat`, a deterministic headless sweep over all current MV truth-table modes.
- Updated batch launchers so automation can set `OSR_NO_PAUSE=1` and receive the child executable exit code.
- Verification: `tools/run_manual_tests.bat` passed with `OSR_NO_PAUSE=1`.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --mv-mode correct --metric-gate` exited `0`.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --mv-mode flip-x --metric-gate` exited `3` and wrote `MVTruthModeActive` / `MVResidualHigh` findings to `warnings.jsonl`.

### DX12 Debug Upscale Command Recording

- Replaced the backend's metadata-only pending path with actual command-list recording.
- Added a real GPU copy path for native-size debug dispatches where input and output dimensions/formats match.
- Kept a temporary heartbeat UAV clear for scaled output until embedded compute shader bytecode is added for the real spatial upscale path.
- Added DX12 wind-tunnel CLI controls `--display-size WIDTHxHEIGHT` and `--render-scale VALUE`, allowing native-size copy tests and scaled-output stress tests without code changes.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --render-scale 1.0 --display-size 640x400 --metric-gate` exited `0`, wrote `debug_dispatch_result: recorded_and_executed`, and logged `mode=dx12_copy_recorded`.
- Verification: `tools/run_manual_tests.bat` passed with `OSR_NO_PAUSE=1`.

### DX12 Compute Spatial Debug Upscale

- Added a runtime-compiled DX12 compute path for `spatial_debug_upscale.hlsl`, with a root signature, compute PSO, SRV/UAV descriptor heap, root constants, output UAV transition, dispatch, UAV barrier, and transition back to shader-read for presentation/readback.
- The compute shader is embedded in the backend for the prototype and kept in sync with `src/reconstruction/shaders/spatial_debug_upscale.hlsl`. Later packaging should replace runtime compilation with embedded bytecode.
- Added readback-after-dispatch validation for `color_output_after_dispatch`, comparing the GPU-produced display output against the CPU nearest reference.
- Fixed the launcher and error path so output hash mismatches fail the DX12 wind tunnel.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --mv-mode correct --metric-gate` exited `0`, logged `mode=dx12_compute_upscale_recorded`, and reported `Reconstruct output hash: matched`.
- Verification: metadata recorded `color_output_after_dispatch ... matched=true`.
- Verification: `tools/run_dx12_wind_tunnel.bat --present-frames 3 --mv-mode correct` exited `0` after opening the DX12 presentation window and presenting three frames.
- Verification: `tools/run_manual_tests.bat` passed with `OSR_NO_PAUSE=1`.

### CPU Temporal Resolve Mode

- Added `src/demo/wind_tunnel/temporal_resolve.*`, a conservative display-space temporal resolve used by the DX12 wind tunnel as the first temporal reconstruction mode.
- Added `--reconstruction spatial-gpu|temporal-cpu` and `--frame-id N` to the DX12 wind tunnel.
- `temporal-cpu` blends the current spatial output with a deterministic previous display history, suppressing history through reactive mask coverage and large motion vectors.
- Capture metrics now report temporal resolve history rejection through the existing `history_reject_pct` column, and metadata records history weight and suppression statistics.
- Added `src/tests/wind_tunnel_temporal_resolve_tests.cpp` covering expected 50/50 blending, reset behavior, and reactive history suppression.
- Verification: `tools/run_manual_tests.bat` passed with `OSR_NO_PAUSE=1`.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --reconstruction temporal-cpu --mv-mode correct --metric-gate` exited `0` with matched transfer/readback hashes.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --reconstruction spatial-gpu --mv-mode correct --metric-gate` exited `0` with matched transfer/readback hashes.

### Sequence Lab Metrics

- Added `src/demo/wind_tunnel/sequence_metrics.*` and `src/demo/sequence_lab.cpp`.
- Added `tools/run_sequence_lab.bat` to build/run `osr_sequence_lab.exe`.
- The sequence lab runs deterministic synthetic frames, compares spatial nearest output against the conservative temporal resolve, and writes `build/manual/osr_sequence_lab_metrics.csv`.
- Added `src/tests/wind_tunnel_sequence_metrics_tests.cpp` requiring temporal resolve to reduce mean frame-to-frame luma delta versus the spatial baseline on the default synthetic sequence.
- Verification: `tools/run_manual_tests.bat` passed with `OSR_NO_PAUSE=1`.
- Verification: `tools/run_sequence_lab.bat --frames 64 --metric-gate` exited `0`; latest run reported spatial delta `0.00436588`, temporal delta `0.00192951`, temporal/spatial ratio `0.441952`.

### DX12 Headless Sequence Mode

- Added `--frames N` to `osr_dx12_wind_tunnel.exe` for headless multi-frame metric runs.
- In `--headless --frames N` mode, the DX12 executable runs the deterministic sequence lab and writes `build/manual/osr_dx12_sequence_metrics.csv`.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --frames 64 --metric-gate` exited `0`; latest run reported spatial delta `0.0041617`, temporal delta `0.00178421`, temporal/spatial ratio `0.428722`.

### Sequence Ghost And Reactive Metrics

- Extended temporal resolve stats with reactive-history and motion-history weight means.
- Extended sequence metrics with `stability_improvement_pct`, `ghost_score`, and `reactive_trail_score`.
- Tightened default temporal motion rejection from `12 px` to `5 px` after the new ghost score flagged excessive moving-pixel history trust.
- Sequence metric gates now fail on temporal delta ratio, ghost score, or reactive trail score.
- Verification: `tools/run_sequence_lab.bat --frames 64 --metric-gate` exited `0`; latest run reported temporal/spatial ratio `0.445084`, stability improvement `55.4916%`, ghost score `0.362473`, reactive trail score `0.00192502`.
- Verification: `tools/run_dx12_wind_tunnel.bat --headless --frames 64 --metric-gate` exited `0`; latest run reported temporal/spatial ratio `0.432007`, stability improvement `56.7993%`, ghost score `0.407826`, reactive trail score `0.00368511`.
- Verification: `tools/run_manual_tests.bat` passed with `OSR_NO_PAUSE=1`.

### Research Links

- OptiScaler architecture and compatibility model: <https://github.com/optiscaler/OptiScaler>
- AMD FSR Super Resolution upscaler integration: <https://gpuopen.com/manuals/fsr_sdk/techniques/super-resolution-upscaler/>
- AMD FSR3 dispatch description fields: <https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/>
- Intel XeSS-SR integration guide: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- NVIDIA Streamline programming model: <https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md>
- Microsoft DirectSR input model: <https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html>
