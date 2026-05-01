# Capture Packs

Capture packs are the evidence format for oSR. A claim about quality should
usually point to a capture pack, a metric gate, and the exact command that
generated it.

## Layout

Typical path:

```text
build/manual/captures/<run_name>/
  session.json
  frames.csv
  metrics.csv
  warnings.jsonl
  bookmarks.jsonl
  sequence_gate_metrics.csv
  portable_run_metrics.csv
  capture_analysis.json
  capture_gate_thresholds.cfg
  frame_000012/
    frame_context.json
    artifacts.json
    color_input.ppm
    color_input.rgba8.raw
    color_output.ppm
    color_output.rgba8.raw
    spatial_baseline.ppm
    spatial_baseline.rgba8.raw
    native_reference.ppm
    native_reference.rgba8.raw
    depth.pgm
    depth.r32f.raw
    motion_vectors_magnitude.pgm
    motion_vectors_x.pgm
    motion_vectors_y.pgm
    motion_vectors.rg32f.raw
    reactive_mask.pgm
    reactive_mask.r32f.raw
    history_weight.pgm
    history_weight.r32f.raw
    color_residual.pgm
    color_residual.r32f.raw
    depth_residual.pgm
    depth_residual.r32f.raw
    feature_lock_strength.pgm
    feature_lock_strength.r32f.raw
```

Not every capture has every file. Spatial-only captures may not include temporal
debug maps. Older captures may not include `native_reference` or feature-lock
maps.

## Files

`session.json`

- run identity
- scenario and algorithm names
- command line
- first-frame dimensions and color space
- threshold provenance
- first-frame SR readiness verdict

`frame_context.json`

- normalized SR input contract for one frame
- resources and extents
- jitter
- motion-vector scale and space
- reset/history flags
- exposure information
- camera parameters
- per-frame SR readiness verdict

`frames.csv`

- frame index
- dimensions
- jitter
- motion-vector scale
- reset state
- timing slots
- validation warning/error counts

`metrics.csv`

- ghost, shimmer, edge/text, material leak, residual, trust, and temporal
  diagnostic columns

`warnings.jsonl`

- validation messages
- temporal diagnostic findings
- likely causes and suggested actions

`artifacts.json`

- manifest for per-frame image/raw artifacts
- exact names, formats, widths, and heights

`capture_analysis.json`

- machine-readable result from offline capture analysis
- includes gate result and thresholds used

## Important Metrics

`sr_ready`

- `1` means the controlled harness frame contains all mandatory SR evidence.
- `0` means the frame may still be useful, but should not be treated as a full
  controlled SR sample.

`history_weight_mean`

- average temporal history contribution.
- High values are expected in stable regions.
- High values in motion/reactive regions are suspicious.

`motion_history_trusted_pct`

- percent of moving samples whose history is trusted.
- Should be near zero for conservative v0 behavior.

`static_history_trusted_pct`

- percent of static samples whose history is trusted.
- Should be high in stable scenes.

`text_native_contrast_ratio`

- output text contrast divided by native-reference text contrast.
- Values below the threshold mean text/detail is too soft.
- Very high values can indicate ringing, not true quality.

`bad_lock_signal`

- feature-lock evidence in regions where locks should not appear.
- A high value means feature locks may be leaking into transparent, specular, or
  reactive content.

`specular_history_leak` and `transparent_history_leak`

- history trust inside material stress regions.
- Lower is safer because these regions often break temporal assumptions.

`temporal_delta_ratio`

- temporal frame delta divided by spatial frame delta.
- Lower generally means better stability.
- The canonical sequence gate currently expects `<= 0.80`.

## Thresholds

Default thresholds live in:

```text
profiles/capture_gate.cfg
```

Use them through:

```powershell
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012 --gate --thresholds profiles\capture_gate.cfg
```

The portable and DX12 harnesses can snapshot the threshold file into the capture
pack so the result remains auditable later.

## Interpreting Failures

Common gate failures:

- `text ROI trusted history below threshold`: stable text is not accumulating
  enough history. Check color/depth residual thresholds and feature-lock logic.
- `text/native contrast below threshold`: output is too soft versus native.
  Check sharpening and feature-lock detail recovery.
- `reactive history trusted above threshold`: particles/transparency are keeping
  too much history. Check reactive masks and reactive penalty.
- `specular history trusted above threshold`: glints are accumulating history.
  Check material-risk classification or reactive synthesis.
- `motion history trusted above threshold`: moving regions trust stale history.
  Check motion-vector scale/sign and rejection thresholds.

## Minimal Evidence For A Quality Claim

For a claim like "this change improved text stability":

1. Save before/after capture packs.
2. Run `tools/run_capture_compare.bat`.
3. Include the command lines.
4. Include `capture_analysis.json` gate status.
5. Check at least one visual PPM output.
6. Record the result in `LOG.md`.

Do not promote changes only because one metric improves. The usual failure mode
is improving text/detail while making reactive trails, transparent panes, or
moving edges worse.
