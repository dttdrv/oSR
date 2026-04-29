# Intel XeSS-SR Notes

XeSS is most useful to oSR for its strict integration/debugging guidance and its portable neural SR boundary. It validates the same temporal input contract while showing how much operational tooling a serious SR implementation needs.

## SDK Shape

Intel describes XeSS-SR as an AI temporal supersampling/AA stage that runs before post-processing and tonemapping. D3D12 and Vulkan execution records XeSS work into the application's command list/buffer, while resource lifetime and synchronization remain the application's responsibility.

Primary sources:

- XeSS-SR developer guide: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- XeSS SDK repository: <https://github.com/intel/xess>

Local oSR state:

- `src/interop/xess_bridge/xess_bridge.h` marks XeSS replacement out of v0.
- `src/interop/xess_bridge/xess_proxy.cpp` is a diagnostic proxy that forwards/logs entrypoints.
- No XeSS SDK headers are vendored into oSR.

## Inputs

XeSS accepts a low-resolution jittered color input, motion vectors, optional depth, optional exposure, optional responsive mask, and output. It supports two practical MV modes:

- dilated high-resolution motion vectors, or
- low-resolution undilated motion vectors plus depth so XeSS can upsample/dilate internally.

The public headers expose execute parameters for color, velocity, depth, exposure scale texture, responsive mask, output, jitter, exposure multiplier, reset, input dimensions, and coordinate offsets.

## Motion, Jitter, Responsive Mask

Key rules from Intel's guide:

- Motion vectors are current-to-previous.
- Pixel-space velocity is the normal path; NDC velocity needs explicit flags/scaling.
- MVs should not include jitter.
- Responsive masks are input-resolution, non-jittered, single-channel, and mark where the current frame should dominate.
- Exposure errors can create ghosting, blur, or brightness instability.

oSR implication: the harness must expose MV arrows, velocity scale/sign tests, responsive-mask views, exposure overrides, reset tests, and frame dumps before real-game quality tuning.

## XMX vs DP4a

XeSS has optimized Intel paths and a cross-vendor path that can run on Shader Model 6.4-class hardware with DP4a or equivalent support. This is important but also humbling for oSR:

- XeSS portable quality still uses a trained model and runtime.
- Radeon 760M does not have NVIDIA Tensor cores or Intel XMX.
- A heavy neural model is not a sensible default for our target.

oSR's portable alternative is to approximate the selection behavior with deterministic sparse candidate scoring, not to recreate XeSS inference.

## Diagnostics To Emulate

Intel XeSS Inspector is a strong model for oSR's tooling. Borrow conceptually:

- Runtime state display.
- Quality and input-resolution reporting.
- Jitter/velocity/exposure overrides.
- HUD/debug overlays.
- Velocity arrows and histograms.
- Multi-frame dumps around bad frames.

Local evidence that oSR is heading there:

- `src/debug/capture_pack.*`
- `src/debug/capture_analysis.*`
- `src/demo/dx12_wind_tunnel/main.cpp`
- `tools/run_dx12_mv_sweep.bat`

## What To Borrow

Safe to borrow:

- Public integration contract.
- Quality-mode mapping, including Ultra Quality Plus. oSR core modes now mirror the XeSS-style ladder: Native 1.0x, Ultra Quality Plus 1.3x, Ultra Quality 1.5x, Quality 1.7x, Balanced 2.0x, Performance 2.3x, Ultra Performance 3.0x.
- Input validation/debugging checklist.
- Inspector-style controls and capture ergonomics.
- Responsive-mask semantics.

Do not borrow:

- XeSS runtime binaries beyond normal redistribution/use.
- Proprietary model internals.
- Header layouts copied into oSR unless license-reviewed and intentionally vendored.
- Reverse-engineered `libxess.dll` behavior.
