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
- Manual harnesses expose the same diagnostic concepts before game interception is trusted: render scale, jitter, freeze/reset, edge views, and later depth/MV/mask views.
- The native manual 3D wind tunnel stays open as an `.exe`, supports camera movement/settings, and gives readable diagnostics for manual testing.
- The DX12 wind tunnel creates resource-backed synthetic color/output/depth/MV/reactive inputs and validates them through `FrameContext`.
- Capture packs and offline analysis report SR readiness, distinguishing complete harness frames from partial game bridge observations.
- Controlled DX12 temporal captures must report `sr_ready=1` before they are accepted as lab evidence.
- The portable core, CPU wind tunnel, capture analysis, and non-DX12 tests configure and run on Linux with Windows-only backends disabled.
- `docs/` contains practical user/developer guides for harness operation, capture-pack interpretation, Linux setup, and current project status.

## Phase 3: Deterministic Spatial Upscaler

Goal: implement a small HLSL compute upscaler path with predictable output.

Pass gate:
- Debug output is produced through a compute pass.
- GPU cost target at 1920x1200 is under 0.6 ms on Radeon 760M-class hardware.
- Spatial baseline includes timing for the trust/debug resources disabled.
- The 3D wind tunnel can reproduce visible differences between nearest, bilinear, and the first deterministic spatial pass on thin geometry and hard silhouettes.

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

## Future Packaging: Cross-Platform FSR Replacement Tool

Goal: after oSR reconstruction is visibly and measurably worthwhile, package it as a Windows/Linux tool that can automatically install or inject an oSR bridge for games with FSR-style temporal upscaler support.

This is explicitly not the current quality target. The current target remains reconstruction quality and observability in the harness and controlled game bridge.

Scope:
- Prefer FSR/FFX-style support first because it is widespread and vendor-neutral.
- Support Windows DLL proxy packaging and Linux shared-library/proton-oriented packaging.
- Detect game executable/API/runtime files, create backups, install the correct bridge, and restore cleanly.
- Keep anti-cheat games out of scope unless the game has an explicitly supported mod/plugin path.
- Never silently overwrite vendor DLLs or game files without backup and a restore path.

Pass gate:
- oSR already produces better controlled-scene results than the baseline being replaced.
- The bridge can capture and validate required temporal SR inputs.
- Installer has dry-run, install, verify, and restore modes.
- Logs clearly identify game, bridge type, original runtime version/hash, installed oSR version/hash, and restore location.
