# oSR Roadmap

## Phase 0: Research And Repo Setup

Goal: establish source-backed design, repository structure, logging, and state tracking.

Pass gate:
- `ARCHITECTURE.md`, `ROADMAP.md`, `STATE.yaml`, and `LOG.md` exist.
- CMake configures the modular targets.
- `LOG.md` records research links and initial decisions.

## Phase 1: Wrapper/Shim Proof Of Life

Goal: load the wrapper in a controlled DX12 sample process and emit structured logs.

Pass gate:
- Wrapper load is visible in `osr_runtime.log`.
- Process, module path, bridge mode, and config are logged.
- No reconstruction work is required yet.

## Phase 2: Pass-Through And Buffer Capture

Goal: normalize FSR-style dispatch input into `FrameContext`, validate metadata, and optionally dump one-frame metadata/captures.

Pass gate:
- Color, depth, motion vectors, output, sizes, jitter, exposure, masks, and reset state are logged.
- Missing or suspicious motion-vector scale is reported.
- Capture is disabled by default.

## Phase 3: Deterministic Spatial Upscaler

Goal: implement a small HLSL compute upscaler path with predictable output.

Pass gate:
- Debug output is produced through a compute pass.
- GPU cost target at 1920x1200 is under 0.6 ms on Radeon 760M-class hardware.
- Spatial baseline includes timing for the trust/debug resources disabled.

## Phase 4: Temporal Accumulation

Goal: add history storage and reprojection using motion vectors, depth, and jitter.

Pass gate:
- History is reset on explicit reset, first frame, and resolution changes.
- Static scenes converge without uncontrolled ghost trails.
- First temporal path target is under 1.5 ms GPU, with warning over 2.5 ms.
- A trust field is generated or cleared each frame and exposed to debug views.
- CPU oracle and HLSL trust update agree on canonical scenario outputs before GPU output is trusted.

## Phase 5: History Rejection And Disocclusion

Goal: reject invalid history using depth consistency, disocclusion, reactive masks, and conservative accumulation weights.

Pass gate:
- Debug views expose motion vectors, depth, disocclusion, accumulation weight, and history confidence.
- Fast foreground motion prefers current-frame clarity over persistent trails.
- Reactive-mask synthesis is available when the source API provides no reactive/responsive mask.
- Sharpening is reduced in low-trust/reactive regions.

## Phase 6: Game Profiles And Benchmarking

Goal: add per-game config overrides and repeatable benchmark/capture workflows.

Pass gate:
- Profiles can override bridge mode, depth convention, motion-vector hints, logging, and capture toggles.
- Profiles can override quality mode or custom render scale.
- Benchmark output includes CPU logging cost and GPU pass timings.

## Phase 7: Optional Tiny Neural Refinement Pass

Goal: evaluate a small refinement pass only after the deterministic path is correct.

Pass gate:
- The pass is optional, isolated, and disabled by default.
- It does not become the core path.
- It fits the same logging, validation, and profiling discipline.
