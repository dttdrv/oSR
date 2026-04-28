# oSR Harness Design

The harness has two equal users:

- A human tester who needs a readable, responsive `.exe` for eye testing.
- The engineering loop, which needs deterministic captures, logs, metrics, and replayable failures.

The goal is one harness with two modes, not two divergent tools.

```text
Pilot Mode -> same frame graph -> capture/log/metrics mode
Lab Mode   -> same frame graph -> capture/log/metrics mode
```

## Design Principle

Every visible artifact should become data.

If a tester sees shimmer, ghosting, blur, wrong motion, reactive-mask trails, or unstable text, the harness should be able to bookmark that moment and write enough state for later diagnosis:

- scenario id
- frame index
- camera path state
- quality mode/render scale
- jitter index/sequence
- motion-vector convention
- depth convention
- reactive-mask mode
- reset/history state
- reconstruction mode
- debug view
- GPU timings
- metric values
- resource hashes
- optional texture dumps
- tester note

## Source-Informed Features

- Intel XeSS Inspector exposes SR state overrides, HUD overlays, histograms, velocity/depth visualizations, and frame dumps. oSR should copy the diagnostic shape, not Intel's implementation: <https://www.intel.com/content/www/us/en/developer/articles/technical/intel-xess-inspector.html>
- Intel XeSS-SR debugging guidance emphasizes jitter scale/sign, velocity scale/sign, static-scene testing, depth, responsive masks, and history reset: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- Unreal TSR exposes visualization modes for accumulated samples, parallax disocclusion, history rejection, history clamp, resurrection, spatial AA, and flickering analysis. oSR needs similar internal visualizations: <https://dev.epicgames.com/documentation/unreal-engine/temporal-super-resolution-in-unreal-engine>
- AMD FSR2's debug checker validates app-supplied inputs and emits warnings for suspicious configuration. oSR should have a debug checker for its own `FrameContext`: <https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-temporal/>
- PIX timing captures show why the harness should emit CPU/GPU markers and timestamps, and PIX GPU captures reinforce the need for D3D12 debug-layer-clean command recording: <https://learn.microsoft.com/en-us/windows/win32/direct3dtools/pix/articles/timing-captures/pix-timing-captures>
- Radeon GPU Profiler is important for the target Radeon 760M path because it can expose event timing, occupancy, barriers, wavefront behavior, and user debug markers on AMD hardware: <https://gpuopen.com/manuals/rgp_manual/rgp_manual-index/>
- RenderDoc's capture model motivates optional "capture frame N" hooks and a queued capture workflow when RenderDoc is present: <https://renderdoc.org/docs/window/capture_attach.html>

## Pilot Mode: Interactive Eye Testing

Primary executable:

```text
tools\run_dx12_wind_tunnel.bat
build\manual\osr_dx12_wind_tunnel.exe
```

Required window layout:

```text
+--------------------------------------------------------+------------------+
|                                                        | Scenario         |
|                                                        | Quality          |
|                Render/output viewport                 | Reconstruction   |
|                                                        | Debug view       |
|                                                        | Input overrides  |
|                                                        | Metrics          |
+--------------------------------------------------------+------------------+
| timeline / bookmarks / A-B compare / current warning line                 |
+-------------------------------------------------------------------------+
```

The UI should be quiet and technical:

- no landing screen
- no decorative panels
- readable monospace metrics
- one right-side settings panel
- bottom timeline/bookmark strip
- hotkeys visible but not dominant

Core controls:

- Scenario: static rails, slow pan, foreground occluder, particles, readable text/sign, camera cut, exposure ramp.
- Quality: Native, Ultra Quality, Quality, Balanced, Performance, Ultra Performance, Custom slider.
- Reconstruction: nearest, bilinear, spatial, temporal fixed-alpha, temporal baseline clamp, oSR trust, oSR trust + residual search.
- Debug view: final, input color, depth, MV arrows, MV magnitude, reactive mask, disocclusion, history trust, accumulation weight, residual-search offset, rejected history, edge energy, difference.
- Override panel: MV scale, X/Y sign flips, pixel/NDC mode, jitter on/off, jitter scale, reversed depth, reset history, reactive mask clamp.
- Playback: pause, step frame, scrub, reset, deterministic camera path, free camera.
- Compare: A/B split, blink compare, zoom loupe, freeze current debug view.
- Capture: dump current frame, dump N frames around current frame, or start/stop a short capture.
- Bookmark: save artifact note with current frame state.

Readable scene content is mandatory:

- high-contrast signs: `STATIC TEXT`, `MOVING TEXT`, `THIN RAILS`, `ALPHA PARTICLES`, `OCCLUSION TEST`
- checker floor
- slanted edge walls
- rotating cube silhouettes
- foreground poles crossing background text
- particles with missing or weak motion vectors
- thin subpixel rails at shallow angles
- exposure gradient wall for HDR/color-space checks

Text and signs matter because humans notice temporal softness, ghosting, and unstable detail faster on readable content than on abstract geometry.

## Lab Mode: Headless / Automated Runs

The same executable should support CLI runs:

```text
osr_dx12_wind_tunnel.exe --scenario rails_pan --frames 240 --mode osr_trust --quality quality --capture metrics
osr_dx12_wind_tunnel.exe --scenario particles --frames 180 --mode compare_all --dump-frame 96
osr_dx12_wind_tunnel.exe --scenario rails_pan --dump-around 96 --dump-radius 15
osr_dx12_wind_tunnel.exe --replay captures/2026-04-28_rails_pan/session.json
```

Required properties:

- deterministic random seed
- deterministic jitter sequence
- deterministic camera/object paths
- fixed frame count
- fixed output directory
- noninteractive exit code

Exit codes:

- `0`: completed, no validation errors, metrics inside configured thresholds
- `1`: harness/runtime failure
- `2`: validation errors
- `3`: metric threshold failure

## Capture Pack

Each run writes a capture pack:

```text
captures/
  2026-04-28T103200Z_rails_pan_quality_osrtrust/
    session.json
    frames.csv
    metrics.csv
    warnings.jsonl
    bookmarks.jsonl
    frame_0096/
      frame_context.json
      color_input.dds
      depth.r32.bin
      motion_vectors.rg32f.bin
      reactive.r8.bin
      output.dds
      debug_history_trust.dds
      debug_disocclusion.dds
      debug_mv.png
```

`session.json` should contain:

- git commit
- executable version
- GPU adapter name/vendor/device id
- driver version if available
- OS/build
- command line
- scenario config
- quality config
- reconstruction config
- thresholds
- source links or algorithm version ids where useful

`frames.csv` should contain one row per frame:

```text
frame_id, scenario_time, render_w, render_h, display_w, display_h,
jitter_x, jitter_y, mv_scale_x, mv_scale_y, reset_history,
gpu_upload_ms, gpu_reconstruct_ms, gpu_present_ms, cpu_frame_ms,
validation_errors, validation_warnings
```

`metrics.csv` should contain the generic quality probes plus temporal trust diagnostics:

```text
frame_id, ghost_score, shimmer_score, disocclusion_leak,
reactive_trail_score, edge_preservation, text_contrast,
history_reject_pct, residual_search_pct, stable_tile_pct,
motion_risk_tile_pct, reactive_tile_pct, disocclusion_tile_pct,
mv_luma_residual_mean, mv_luma_residual_p95,
mv_depth_residual_mean, mv_depth_residual_p95,
bad_history_trusted_pct, good_history_rejected_pct,
reactive_history_trusted_pct, disocclusion_history_trusted_pct,
trust_evidence_agreement_pct, history_trust_mean,
accumulation_weight_mean
```

`bookmarks.jsonl` should contain human notes:

```json
{"frame":96,"view":"final","tag":"Shimmer","note":"thin rail shimmer on left edge","severity":3,"rect":[120,240,180,360],"camera":"deterministic","mode":"osr_trust"}
```

Supported bookmark tags:

- Ghosting
- Shimmer
- Blur
- EdgeCrawl
- ParticleTrail
- TextSmear
- DisocclusionLeak
- MVWrong
- HDRWeird

The note system is the bridge between subjective testing and reproducible engineering work. A tester should be able to press one key, mark the artifact, optionally drag a rectangle, and continue moving.

## Metrics

The first metrics should be deliberately simple and hard to game.

Use generic perceptual metrics as secondary evidence, not the primary pass/fail signal:

- PSNR/SSIM/MS-SSIM for sanity checks.
- FLIP for perceptual image-difference comparisons against native/reference frames: <https://research.nvidia.com/publication/flip>
- VMAF/libvmaf for longer video-style full-reference runs where useful: <https://github.com/Netflix/vmaf>
- LPIPS only for offline experiments, not realtime gating.

The custom oSR metrics below are more important because they point to fixable SR causes.

### Ghost Score

Measures stale history behind moving/reactive/disoccluded objects.

Inputs:

- current color
- previous output/history
- reactive mask
- disocclusion mask
- motion vectors

High ghost score means history is contributing where it should not.

### Shimmer Score

Temporal variance in static high-frequency areas after compensating for expected jitter.

Useful scenes:

- rails
- checker floor
- readable sign
- subpixel fence

### Disocclusion Leak

History weight in pixels marked as newly revealed by depth/MV tests.

Expected behavior:

- near zero on confident disocclusion
- conservative fallback to current frame

### Reactive Trail Score

History contribution in particle/transparency regions after reactive mask is applied.

Useful sweeps:

- reactive max `0.0, 0.25, 0.5, 0.8, 0.9, 1.0`
- supplied mask vs synthesized mask vs absent mask

### Edge / Text Score

Measures contrast across known rails/text/sign edges.

This is not a full perceptual metric. It is a repeatable local probe for readable detail.

## Debug Views

Minimum visualizations:

- input color
- output color
- depth linearized
- MV magnitude
- MV arrows
- reactive mask
- disocclusion mask
- history trust
- evidence trust
- accumulation weight
- rejected history
- residual-search candidate offset
- tile classification
- edge energy
- difference from baseline/native where available

The debug views are as important as the final output. If a final frame looks wrong but the debug view does not explain why, the harness is incomplete.

## Improvement Loop

The harness should make this loop cheap:

1. Human sees artifact.
2. Press bookmark.
3. Harness writes frame state, note, debug view, and metrics.
4. Engineer replays the same frame range headlessly.
5. Algorithm change is tested against the same capture pack.
6. Metrics and bookmarked debug views show whether the change helped or just moved the artifact.

## Metric-To-Cause Rules

Logs should suggest likely causes. These are not automatic fixes; they are ranked hypotheses to investigate.

| Signal | Likely cause | Candidate action |
| --- | --- | --- |
| high ghost + high history weight + low disocclusion | disocclusion detector too weak | tighten depth test, add foreground MV dilation |
| particle trail + reactive present but low | reactive mask or clamp too weak | increase reactive synthesis/clamp for alpha region |
| shimmer high + low trust + residual disabled | unstable high-frequency tile needs selective search | enable residual search on `ShimmerRisk` |
| text blur + high trust + low sharpening | sharpening too conservative on stable detail | raise confidence-gated sharpening for stable opaque tiles |
| MV reprojection error high + sign-flip test improves | motion-vector convention mismatch | log profile override, never silently auto-fix |
| depth disagreement at silhouettes | wrong depth convention or missing dilation | check reversed depth flag, run 3x3 foreground dilation |
| reset artifact after camera cut | history was not invalidated | force reset on cut/FOV/resolution/path changes |
| HDR brightness trails | exposure/pre-exposure mismatch | log exposure source and compare input/output scale |

Every bookmark and metric spike should write a compact diagnosis block into `warnings.jsonl`:

```json
{"frame":96,"tag":"ParticleTrail","likely_cause":"reactive_mask_too_weak","evidence":{"reactive_mean":0.03,"history_weight_mean":0.61},"suggested_action":"raise synthesized reactive mask or clamp"}
```

The DX12 wind tunnel now supports `--metric-gate`. When enabled, severe temporal diagnostic findings return exit code `3` after the capture pack is written. This keeps exploratory corrupted-input sweeps usable while allowing regression jobs to fail on metric verdicts.

## Implementation Phases

### H0: Harness Contract

- Add shared `HarnessRunConfig`, `HarnessFrameMetrics`, `HarnessBookmark`, and `HarnessSessionManifest` types.
- Write JSON/CSV helpers.
- Store git/build/scenario/reconstruction metadata.

### H1: DX12 Buffer Truth

- Upload synthetic color/depth/MV/reactive data into D3D12 textures.
- Read back hashes.
- Log resource states and formats.
- Output `session.json`, `frames.csv`, and metadata.

### H2: Visual Debug Views

- Present output in a DX12 swapchain.
- Add debug view selector.
- Add depth/MV/reactive views before final reconstruction.

### H3: Interactive Eye Testing

- Add right-side settings panel and bottom timeline.
- Add pause/step/scrub.
- Add A/B split and blink compare.
- Add bookmark note capture.

### H4: Metrics

- Implement ghost, shimmer, disocclusion leak, reactive trail, edge/text scores.
- Implement CPU temporal diagnostics that reproject a deterministic previous frame through supplied motion vectors and score whether the trust policy accepts good history or rejects bad/reactive/disoccluded history.
- Emit compact diagnostic verdicts into `warnings.jsonl`, including likely cause, suggested action, severity, and evidence value.
- Add threshold-based exit codes for headless runs.

### H5: Replay

- Replay `session.json` deterministically.
- Load or regenerate synthetic scenes.
- Compare metrics between two runs.

### H6: PIX/External Tool Integration

- Add optional PIX markers around upload, reconstruction, debug view, present.
- Keep captures debug-layer clean.
- Document how to take PIX GPU/timing captures.
- Add optional Radeon GPU Profiler marker discipline for AMD performance investigations.
- Add optional RenderDoc capture hook if the API is detected at runtime.
- Add CLI flags for external-tool-friendly capture windows:

```text
--capture-frame 120
--dump-around 120 --dump-radius 15
--pix-markers on
--rgp-markers on
--renderdoc-trigger on
```

## Non-Goals

- Do not build a generic game benchmark suite yet.
- Do not depend on anti-cheat games.
- Do not optimize for screenshots only.
- Do not accept subjective claims without a capture pack.
- Do not make the logging so heavy that it changes frame timing in normal mode.
