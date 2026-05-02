# oSR Linux Handoff

Last updated: 2026-05-02
Repository: `https://github.com/dttdrv/oSR`
Current branch: `master`
Current known head before this handoff: `c968e39 Expand project documentation`

This is the detailed handoff for continuing oSR on Linux, specifically
elementaryOS. It is written for the next development session and for future
agents who need the full context without rereading the whole chat.

## One-Screen Summary

oSR is now a harness-first temporal super-resolution prototype. The center of
the project is not game injection right now. The center is a controlled harness
that can generate SR inputs, run reconstruction, emit capture packs, fail on bad
inputs, and produce enough data to improve the upscaler without guessing.

The Linux move is already started:

- `linux-core` CMake preset exists.
- Windows-only DX12/Win32/XeSS proxy targets are gated off on Linux.
- `tools/run_linux_core_tests.sh` runs the portable test suite.
- `tools/run_portable_wind_tunnel.sh` builds and runs a Linux-safe CPU harness.
- `docs/` now contains user, development, capture, Linux, and status docs.

Current truth:

- This is not `v1.0`.
- It is not XeSS/DLSS/FSR quality in real games yet.
- No Man's Sky does not currently use oSR reconstruction.
- The diagnostic XeSS proxy can observe No Man's Sky metadata and forward to the
  real XeSS runtime.
- The portable and DX12 harness paths are where quality work should continue.

## User Preferences And Operating Style

The user wants:

- Autonomous work.
- A lead-developer posture from the assistant.
- Decisions made without repeatedly asking for permission.
- Subagents when useful, though previous attempts hit agent thread limits.
- Serious graphics-engineering work, not marketing.
- No overclaiming.
- Linux and Windows development in parallel until the move to elementaryOS is
  complete.
- Current focus entirely on the harness instead of game injection.
- A real `.exe`/application-style manual test environment for eye testing.
- Readable in-scene content and readable diagnostics.
- Resolution/quality controls like DLSS/XeSS quality modes.
- Logs and captures detailed enough that the assistant can improve the upscaler
  from the data.
- A breakthrough path that can plausibly improve quality-per-millisecond on
  Radeon 760M-class hardware without heavy neural inference.

Important style constraints from project instructions:

- For UI-related work, use `taste-skill`.
- For generated images, read the Image-Gen skill first.
- Maintain `LOG.md` and `STATE.yaml`.
- Use subagents when possible, but do not block if the agent limit is reached.
- Do not create god files.
- Keep code modular.
- Use C++ for runtime/wrapper components.
- Use HLSL/compute shaders for reconstruction passes.
- Keep shader code isolated.
- Keep vendor-specific bridges isolated.
- Add structured logging and config from day one.
- Never silently guess motion-vector scale.
- Prefer correctness and observability over visual quality in v0.

## Current Repository State

Recent commits:

```text
c968e39 Expand project documentation
e6ff6f1 Add portable wind tunnel harness
1d6e8b8 Add portable Linux core harness path
da4ae02 Gate XeSS replacement mode
63efbca Serialize SR readiness in capture packs
1c4438c Add SR harness readiness contract
e2d635a Decode XeSS Vulkan frame context
6b0647a Align quality modes with XeSS
```

Current version and phase:

- `prototype_version: v0.2.0`
- `current_phase: phase_4`
- Phase 4 means temporal accumulation exists in the harness.
- Early Phase 5 history rejection/disocclusion diagnostics are already present.

Current documentation map:

- `README.md`: short project entry.
- `ARCHITECTURE.md`: system architecture and constraints.
- `ROADMAP.md`: phase plan.
- `HARNESS.md`: detailed harness design.
- `STATE.yaml`: machine-readable current state.
- `LOG.md`: append-only engineering log.
- `docs/README.md`: docs index.
- `docs/PROJECT_STATUS.md`: plain status.
- `docs/HARNESS_USER_GUIDE.md`: how to run harnesses.
- `docs/CAPTURE_PACKS.md`: capture-pack layout and metrics.
- `docs/LINUX.md`: elementaryOS/Linux path.
- `docs/DEVELOPMENT.md`: engineering workflow.
- `docs/wiki/`: source-backed SR research wiki.

## Linux Setup

On elementaryOS:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git
```

Clone:

```bash
git clone https://github.com/dttdrv/oSR.git
cd oSR
```

Run portable tests:

```bash
bash tools/run_linux_core_tests.sh
```

Manual equivalent:

```bash
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core
```

Run portable harness:

```bash
bash tools/run_portable_wind_tunnel.sh
```

Fast smoke:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 8 --display-size 320x200 --quality quality --capture-run-name portable_smoke --capture-frame 8 --overwrite
```

Quality-gated Linux-safe capture:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 16 --display-size 1280x800 --quality quality --capture-run-name portable_quality_gate --capture-frame 12 --overwrite --metric-gate
```

Expected corrupted-MV failure:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 16 --display-size 320x200 --quality quality --mv-mode flip-x --metric-gate
```

Expected result: exit code `3`, with `Metric gate failed`.

## CMake Split

Important options:

```text
OSR_BUILD_TESTS
OSR_BUILD_DX12
OSR_BUILD_WIN32_HARNESS
OSR_BUILD_XESS_PROXY
```

Defaults:

- Windows:
  - `OSR_BUILD_DX12=ON`
  - `OSR_BUILD_WIN32_HARNESS=ON`
  - `OSR_BUILD_XESS_PROXY=OFF`
- Linux:
  - `OSR_BUILD_DX12=OFF`
  - `OSR_BUILD_WIN32_HARNESS=OFF`
  - `OSR_BUILD_XESS_PROXY=OFF`

Do not try to build `osr_dx12` on Linux. It intentionally requires Windows
D3D12 headers/libraries.

Portable targets that should build on Linux:

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
- portable tests

Windows-only targets:

- `osr_dx12`
- `osr_fsr_bridge`
- `osr_dx12_wind_tunnel`
- `osr_manual_3d_wind_tunnel`
- `osr_xess_proxy`

## Harnesses

### Portable Wind Tunnel

File:

```text
src/demo/portable_wind_tunnel.cpp
```

Launchers:

```text
tools/run_portable_wind_tunnel.sh
tools/run_portable_wind_tunnel.bat
```

Purpose:

- Linux-safe.
- No D3D12.
- No Win32.
- Deterministic synthetic frames.
- CPU temporal resolve.
- Sequence metrics.
- Temporal diagnostics.
- Capture pack output.
- Capture-analysis gate.
- Negative corrupted-MV gate.

Important options:

```text
--frames N
--start-frame N
--display-size WIDTHxHEIGHT
--quality native|ultra-quality-plus|ultra-quality|quality|balanced|performance|ultra-performance
--render-scale FLOAT
--capture-run-name NAME
--capture-frame N
--capture-root PATH
--thresholds PATH
--metric-gate
--overwrite
--no-jitter
--mv-mode correct|zero|flip-x|flip-y|half-scale|double-scale|jitter-contaminated
--history-weight FLOAT
--motion-rejection FLOAT
--color-rejection FLOAT
--depth-rejection FLOAT
--history-clip-margin FLOAT
--sharpening FLOAT
```

Recent good portable result:

```text
Command:
build/linux-core/osr_portable_wind_tunnel.exe
  --frames 16
  --display-size 1280x800
  --quality quality
  --capture-run-name portable_quality_gate_v3
  --capture-frame 12
  --capture-root build/manual/captures
  --overwrite
  --metric-gate

Result:
exit 0
sr_ready=1
capture-analysis gate=ok
text/native contrast ratio=0.93776
motion history trusted=0
static history trusted=98.8316%
```

Important caveat:

- `320x200` runs are good smoke tests.
- Do not treat `320x200` capture-analysis failures as final quality evidence.
- At tiny sizes, text-history trust can under-report because the ROI is too
  coarse.
- Normal lab-size capture gates should use something like `1280x800`.

### DX12 Wind Tunnel

File:

```text
src/demo/dx12_wind_tunnel/main.cpp
```

Launcher:

```text
tools/run_dx12_wind_tunnel.bat
```

Purpose:

- Windows GPU reference.
- D3D12 resources.
- Compute spatial path.
- Temporal GPU path.
- CPU/GPU debug-map parity.
- Capture packs with real resource readbacks.
- MV sweeps.

Useful command:

```powershell
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name dx12_temporal_manual --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

MV sweep:

```powershell
tools\run_dx12_mv_sweep.bat
```

### Native Win32 Manual 3D Wind Tunnel

File:

```text
src/demo/manual_3d_wind_tunnel.cpp
```

Launcher:

```text
tools/run_3d_wind_tunnel.bat
```

Purpose:

- Human eye testing.
- Simple 3D software-rasterized scene.
- Free camera.
- Settings panel.
- Quality presets.
- Jitter and freeze controls.
- Debug views.

Known limitation:

- This is not the full temporal GPU path.
- It is for visual perception leads, not final proof.

### WebGL Fallback

Files:

```text
tools/manual_3d_scene.html
tools/run_3d_scene.bat
```

Purpose:

- Quick browser fallback.
- Useful when native Windows app path is inconvenient.
- Not the main harness anymore.

## Quality Modes And Calculations

Current core quality ladder mirrors XeSS-style presets:

```text
Native             = 1.0
UltraQualityPlus   = 1 / 1.3  = 0.769230769
UltraQuality       = 1 / 1.5  = 0.666666667
Quality            = 1 / 1.7  = 0.588235294
Balanced           = 1 / 2.0  = 0.5
Performance        = 1 / 2.3  = 0.434782609
UltraPerformance   = 1 / 3.0  = 0.333333333
```

Important history:

- The user previously asked for DLSS-ish values:
  - Quality `66%`
  - Balanced `58%`
  - Performance `50%`
- Later, the user asked to match XeSS values:
  - Native
  - Ultra Quality Plus
  - Ultra Quality
  - Quality
  - Balanced
  - Performance
  - Ultra Performance
- Final implemented decision: match the XeSS-style ladder.
- Do not silently change this again. If changing quality presets, document it in
  `LOG.md`, `STATE.yaml`, tests, and all harness UI/docs.

For `1920x1200`, current rounded render sizes are:

```text
Native:             1920x1200
UltraQualityPlus:   1477x923
UltraQuality:       1280x800
Quality:            1129x706
Balanced:           960x600
Performance:        835x522
UltraPerformance:   640x400
```

For `1280x800`, current rounded render sizes include:

```text
Quality: 753x471
```

For `320x200`, current rounded render sizes include:

```text
Quality: 188x118
```

Observed No Man's Sky XeSS Balanced:

```text
Output: 1920x1080
Input/render: 960x544
Quality enum: 102 = Balanced
Scale: 0.5
Note: height is 544 instead of 540, likely alignment/internal rounding.
```

## Reconstruction State

Current CPU temporal path:

- Jitter-aware bilinear spatial base.
- Motion-compensated display-space history lookup.
- Current-to-previous pixel motion vectors.
- Luma/color residual rejection.
- Depth residual rejection.
- Reactive mask suppression.
- YCoCg history clipping.
- Confidence-gated sharpening.
- Feature-lock detail recovery on stable high-trust edges.
- Debug maps:
  - `history_weight`
  - `color_residual`
  - `depth_residual`
  - `feature_lock_strength`

Current default temporal settings:

```text
max_history_weight              = 0.92
reactive_penalty                = 0.95
motion_rejection_pixels         = 1.25
color_rejection_threshold       = 0.12
depth_rejection_threshold       = 0.025
sharpening_amount               = 0.50
sharpening_low_trust_scale      = 0.20
sharpening_reactive_scale       = 0.25
history_clip_margin             = 0.015
feature_lock_sharpening_boost   = 0.35
feature_lock_min_edge_strength  = 0.18
feature_lock_min_history_trust  = 0.70
feature_lock_max_luma_delta     = 0.045
feature_lock_max_luma_variance  = 0.0008
feature_lock_max_motion_pixels  = 1.5
feature_lock_reactive_unlock    = 0.20
feature_lock_acquire_rate       = 0.22
```

Why these defaults:

- Conservative history is preferred over trails.
- Sharpening `0.50` improved native-reference text contrast without failing
  stability gates.
- Sharpening `0.55` and `0.60` looked promising in selected-frame captures but
  failed broader sequence stability when promoted.
- History clip margin `0.015` beat `0.02` in guarded captures without reducing
  text trusted history.
- Feature-lock threshold sweeps did not beat defaults, so defaults were kept.

## Capture Gates

Default threshold file:

```text
profiles/capture_gate.cfg
```

Current thresholds:

```text
max_motion_history_trusted_pct = 1
min_static_history_trusted_pct = 90
min_text_history_trusted_pct = 35
max_specular_history_trusted_pct = 2
max_transparent_history_trusted_pct = 8
max_reactive_history_trusted_pct = 1
max_color_reject_candidate_pct = 3
min_locked_detail_score = 50
max_bad_lock_signal = 0.13
min_text_native_contrast_ratio = 0.90
```

Interpretation:

- Motion history should be almost zero.
- Static history should be high.
- Text should retain enough trusted history and contrast.
- Specular/transparent/reactive regions should not accumulate stale history.
- Native text contrast ratio below `0.90` means too soft.
- Bad lock signal above `0.13` means feature locks are leaking into unsafe
  regions.

## Capture Pack Format

Main docs:

```text
docs/CAPTURE_PACKS.md
```

Typical files:

```text
session.json
frames.csv
metrics.csv
warnings.jsonl
bookmarks.jsonl
sequence_gate_metrics.csv
portable_run_metrics.csv
capture_analysis.json
capture_gate_thresholds.cfg
frame_000012/frame_context.json
frame_000012/artifacts.json
frame_000012/*.ppm
frame_000012/*.pgm
frame_000012/*.raw
```

Important debug artifacts:

- `color_input.ppm`
- `color_output.ppm`
- `spatial_baseline.ppm`
- `native_reference.ppm`
- `depth.pgm`
- `motion_vectors_magnitude.pgm`
- `motion_vectors_x.pgm`
- `motion_vectors_y.pgm`
- `reactive_mask.pgm`
- `history_weight.pgm`
- `color_residual.pgm`
- `depth_residual.pgm`
- `feature_lock_strength.pgm`

SR readiness:

- A full controlled harness frame should report `sr_ready=1`.
- Game bridge captures may be bridge-valid but not harness-ready.
- This distinction is intentional.

## Game Injection State

This is not the focus right now, but here is the exact state.

No Man's Sky:

- Local Steam install was used.
- oSR diagnostic `libxess.dll` proxy was installed at one point.
- Original Intel runtime was preserved as `libxess_real.dll`.
- Proxy can load and forward.
- Proxy logs to game `Binaries/osr_logs/osr_xess_proxy.log`.
- Proxy observed real No Man's Sky XeSS Vulkan dispatches.

Observed NMS facts:

```text
Output/display: 1920x1080
Input/render: 960x544
XeSS quality enum: 102 = Balanced
Init flags: INVERTED_DEPTH | ENABLE_AUTOEXPOSURE
Depth inverted: true
Auto exposure: true
Jitter: live and in expected subpixel range
HIGH_RES_MV: not set
Responsive mask: not observed
Exposure texture: not observed
Velocity scale: not set by game during captured session
Normalized MV scale: (0, 0)
```

Critical decision:

- Because NMS did not call `xessSetVelocityScale`, oSR must not guess motion
  vector scale.
- Replacement policy refuses takeover when MV scale is unknown/zero.
- Replacement policy also refuses takeover while the Vulkan writer backend is
  unavailable.

Current proxy modes:

```text
OSR_XESS_MODE=passthrough
OSR_XESS_MODE=observe
OSR_XESS_MODE=osr
```

`osr` mode is a policy check, not finished replacement. It logs why replacement
is refused, then forwards unless a safe backend exists.

Do not spend the next Linux phase trying to force NMS replacement. Build the
Linux harness and Vulkan backend first.

## Important Mistakes And Lessons

### Mistake: treating game injection as the main path too early

What happened:

- A lot of effort went into No Man's Sky/XeSS proxy diagnostics.
- It produced valuable input-contract evidence.
- It did not make the game use oSR reconstruction.

Lesson:

- Injection is downstream.
- Harness quality and observability are upstream.

### Mistake: console apps opening and closing immediately

What happened:

- Early tools were console executables that closed too fast for manual testing.

Lesson:

- Manual tools need persistent UI or pause behavior.
- The native 3D wind tunnel exists partly because of this.
- Scripts use `OSR_NO_PAUSE=1` for automation but can pause for double-click use.

### Mistake: thinking a selected-frame sharper result was enough

What happened:

- Sharpening `0.55` and `0.60` passed selected-frame native-reference gates.
- Promoting `0.55` failed broader multi-frame sequence stability.

Lesson:

- Do not tune against one frame.
- Selected-frame capture must be paired with sequence metrics.

### Mistake: tiny captures as quality evidence

What happened:

- `320x200` portable captures are fast.
- Capture-analysis text-history gate can fail at tiny sizes because text ROI is
  too coarse.

Lesson:

- Use `320x200` as smoke.
- Use `1280x800` or higher for quality gates.

### Mistake: motion vectors can look okay if only final output is checked

What happened:

- Some corrupted MV modes can still produce stable-looking aggregate sequence
  metrics in synthetic scenes.

Lesson:

- Metric gates must include temporal diagnostics and explicit corrupted-MV
  failure paths.
- The portable harness now rejects `flip-x` under `--metric-gate`.
- The DX12 MV sweep remains important.

### Mistake: naming portable code under `dx12_wind_tunnel`

What happened:

- `display_upscale.*` is portable but lives under `src/demo/dx12_wind_tunnel`.

Lesson:

- This is technical debt.
- Move it later to a neutral path like `src/demo/wind_tunnel/display_upscale.*`
  when touching it for related work.

## Architecture Decisions

### Harness first

Decision:

- Focus entirely on the harness until reconstruction improves measurably.

Reason:

- Real games expose messy inputs.
- Without controlled captures, quality claims are subjective.
- The harness makes errors visible and reproducible.

### Clean-room references

Decision:

- OptiScaler and GPL projects are architecture references only.
- Do not copy GPL implementation code.

Reason:

- The project should remain clean-room unless licensing is deliberately changed.

### Heavy neural inference is not core

Decision:

- Do not design around heavy neural inference as the main path.

Reason:

- Primary target is AMD Radeon 760M iGPU.
- The useful breakthrough target is quality-per-millisecond through deterministic
  temporal evidence, not brute-force transformer inference.

### Trust-field thesis

Decision:

```text
validate renderer inputs
-> score temporal history
-> spend work only on risky tiles
-> expose every decision in captures
```

Reason:

- Modern ML SR appears to win by selecting better temporal/spatial context.
- oSR borrows the principle of context selection, not the expensive model.

### Conservative v0

Decision:

- Prefer lower accumulation over persistent ghost trails.

Reason:

- Users notice trails and wrong history immediately.
- Stable detail recovery can be improved later with confidence maps and feature
  locks.

### Vendor bridges isolated

Decision:

- FSR, XeSS, DLSS-style paths stay in separate `src/interop` modules.

Reason:

- Avoid contaminating the core reconstruction with vendor-specific assumptions.

### Platform backends isolated

Decision:

- DX12 stays under `src/backends/dx12`.
- Future Vulkan should go under `src/backends/vulkan`.

Reason:

- Do not hide API-specific synchronization/resource-layout details behind a
  premature abstraction.

## Code Map

Core:

```text
src/core/frame_context.*
src/core/quality_mode.*
src/core/config.*
src/core/logging.*
src/core/resource_registry.*
```

Portable wind tunnel:

```text
src/demo/portable_wind_tunnel.cpp
src/demo/wind_tunnel/synthetic_frame.*
src/demo/wind_tunnel/temporal_resolve.*
src/demo/wind_tunnel/temporal_diagnostics.*
src/demo/wind_tunnel/sequence_metrics.*
src/demo/wind_tunnel/debug_dumps.*
src/demo/wind_tunnel/synthetic_roi.*
```

DX12:

```text
src/backends/dx12/*
src/demo/dx12_wind_tunnel/*
```

Interop:

```text
src/interop/fsr2_bridge/*
src/interop/xess_bridge/*
src/interop/dlss_bridge/*
```

Reconstruction:

```text
src/reconstruction/temporal_accumulation.*
src/reconstruction/history_rejection.*
src/reconstruction/disocclusion.*
src/reconstruction/residual_search.*
src/reconstruction/tile_classifier.*
src/reconstruction/feature_locks.*
src/reconstruction/trust_field.*
src/reconstruction/sharpening.*
src/reconstruction/shaders/*
```

Debug/capture:

```text
src/debug/capture_pack.*
src/debug/capture_analysis.*
src/debug/capture_compare.*
src/debug/frame_context_readiness.*
src/debug/validation.*
```

Tests:

```text
src/tests/*
```

## Essential Verification Commands

Linux portable:

```bash
bash tools/run_linux_core_tests.sh
```

Portable harness quality gate:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 16 --display-size 1280x800 --quality quality --capture-run-name linux_handoff_check --capture-frame 12 --overwrite --metric-gate
```

Windows manual suite:

```powershell
$env:OSR_NO_PAUSE='1'
tools\run_manual_tests.bat
```

Windows DX12 temporal:

```powershell
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name dx12_handoff_check --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

Motion-vector corruption:

```powershell
tools\run_dx12_mv_sweep.bat
```

Capture analyzer:

```powershell
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012 --gate --thresholds profiles\capture_gate.cfg
```

Capture compare:

```powershell
tools\run_capture_compare.bat build\manual\captures\run_a build\manual\captures\run_b
```

## What To Do First On Linux

1. Clone repo.
2. Run:

```bash
bash tools/run_linux_core_tests.sh
```

3. Run:

```bash
bash tools/run_portable_wind_tunnel.sh
```

4. Confirm capture exists:

```bash
ls build/manual/captures/portable_manual
```

5. Inspect:

```bash
cat build/manual/captures/portable_manual/capture_analysis.json
```

6. If this works, the Linux portable path is alive.

## Next Linux Milestone

Build a Linux-native interactive harness.

Preferred direction:

```text
SDL2 or GLFW window
Vulkan renderer
same synthetic scene concepts
same FrameContext
same capture-pack writer
same debug views
same quality controls
same metric gates
```

Recommended module shape:

```text
src/backends/vulkan/
  vulkan_backend.*
  vulkan_resources.*
  vulkan_barriers.*
  vulkan_shader_dispatch.*

src/demo/vulkan_wind_tunnel/
  main.cpp
  presenter.*
  ui.*
  texture_io.*
```

Do not make Vulkan a side effect of DX12 code. Keep it separate until the shared
abstractions become obvious.

## Vulkan Harness First Targets

Target 1: window opens and stays open

- Linux executable starts.
- Shows synthetic color.
- Has resolution/quality controls.
- Logs startup.

Target 2: portable scene parity

- Uses same synthetic frame generator or equivalent data.
- Can run Quality/Balanced/Performance.
- Can freeze frame.
- Can toggle jitter.

Target 3: debug views

- color
- depth
- MV magnitude
- reactive mask
- history weight
- color residual
- depth residual
- feature lock strength

Target 4: capture pack

- writes `session.json`
- writes `frame_context.json`
- writes artifacts
- analyzer can gate it

Target 5: GPU temporal path

- Vulkan compute shader matches CPU oracle on selected frames.
- Debug maps compare against CPU maps.
- Bad MV modes fail gates.

## What Not To Do Next

Do not:

- chase No Man's Sky replacement before Vulkan writer exists
- guess motion-vector scale
- make a generic injector installer yet
- add neural inference as core path
- tune only one frame
- rely only on screenshots
- break `LOG.md` append-only behavior
- skip `STATE.yaml` updates
- merge platform-specific code into portable core
- create a giant renderer/harness/reconstruction file

## Research Context

Research docs:

```text
docs/wiki/source-index.md
docs/wiki/sr-input-contract.md
docs/wiki/fsr.md
docs/wiki/xess.md
docs/wiki/dlss-streamline.md
docs/wiki/temporal-reconstruction.md
docs/wiki/borrowable-ideas.md
docs/wiki/research-backlog.md
```

Key source-backed lessons already captured:

- FSR, XeSS, DLSS/Streamline, and DirectSR validate the same general temporal SR
  input contract.
- The central problem is history validation.
- DLSS 4/4.5 public material supports the idea that better context selection
  improves temporal stability, ghosting, and detail in motion.
- oSR cannot afford a heavy transformer path on Radeon 760M.
- oSR should approximate sparse temporal attention deterministically:
  - trust fields
  - tile risk classification
  - bounded residual search
  - reactive/disocclusion suppression
  - static-detail confidence
  - feature locks
  - confidence-gated sharpening

## Known Open Problems

Quality:

- Not yet better than XeSS/FSR/DLSS in real games.
- Synthetic scenes are necessary but not sufficient.
- Need more scenarios:
  - camera pans
  - occluders
  - foliage-like alpha
  - specular water/glints
  - UI/HUD contamination
  - exposure changes
  - HDR/scRGB
  - thin diagonal geometry

Engineering:

- Linux interactive backend missing.
- Vulkan writer missing.
- Runtime HLSL compilation should be replaced with embedded bytecode on Windows.
- `display_upscale.*` should move out of `dx12_wind_tunnel`.
- Capture thresholds need calibration across more scenes.
- Need location-aware worst-pixel reporting for temporal GPU sequence parity.
- Need CPU-vs-HLSL numeric parity tests once shader compilation path is cleaner.
- Need GPU timing queries for DX12 and eventually Vulkan.

Game bridge:

- FSR bridge is still skeletal.
- XeSS proxy is diagnostic only.
- DLSS/NVNGX spoofing is out of scope for now.
- Anti-cheat games are out of scope.

## Glossary

`FrameContext`

- Normalized temporal SR contract for one frame.

`SR readiness`

- Strict check that a harness frame has all mandatory controlled SR inputs.

`Capture pack`

- Directory containing JSON/CSV/raw/debug images for a reproducible frame/run.

`History weight`

- How much previous output contributes to current output.

`Reactive mask`

- Mask for particles/transparency/rapid shading changes where history should be
  suppressed.

`Feature lock`

- Conservative detail-confidence signal for stable high-trust edges/text.

`Bad lock signal`

- Feature-lock signal in unsafe regions.

`Temporal delta ratio`

- Temporal frame delta divided by spatial frame delta. Lower usually means more
  stable output.

`Motion-vector truth-table mode`

- Synthetic corruption mode like `flip-x`, `zero`, or `double-scale` used to
  verify the harness fails when inputs are wrong.

## Final Handoff Instruction

On Linux, keep the project honest:

1. Make the harness better.
2. Make the data richer.
3. Make bad inputs fail loudly.
4. Compare every quality change against capture packs.
5. Only then return to game injection.

The next developer should be able to run `bash tools/run_portable_wind_tunnel.sh`
and immediately have evidence to inspect. That is the new baseline.
