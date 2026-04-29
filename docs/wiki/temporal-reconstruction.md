# Temporal Reconstruction Theory

Temporal SR beats pure spatial scaling because it can use samples collected across frames. It fails when it trusts the wrong history.

## Why Temporal Beats Spatial

A spatial upscaler sees one frame. It can interpolate, sharpen, classify edges, and hallucinate plausible local detail, but it cannot recover subpixel scene samples that were only visible in adjacent frames.

Temporal SR uses:

- camera jitter to sample different subpixel positions over time;
- motion vectors to reproject previous samples;
- depth to detect disocclusion and foreground/background mismatch;
- history validation to decide whether old samples are still valid.

The TAA survey by Yang et al. frames temporal antialiasing as temporally amortized supersampling with sample accumulation and history validation as central problems: <https://research.nvidia.com/labs/rtr/publication/yang2020survey/>

## Core Failure Modes

- Ghosting: stale history survives after motion, disocclusion, or transparency changes.
- Smearing: valid detail is averaged too strongly in motion.
- Shimmer: accumulation/rejection toggles inconsistently over subpixel patterns.
- Edge crawl: silhouettes pick inconsistent foreground/background history.
- Reactive trails: particles/transparency keep old samples.
- HDR/exposure instability: color residuals become meaningless when exposure is wrong.
- UI/HUD contamination: screen-space overlays lack scene MVs/depth and should not be in the SR input path.

## Deterministic Sparse Temporal Attention

Modern neural SR suggests that quality improves when the algorithm selects better context. oSR's hardware target cannot afford broad learned attention, so the deterministic analog is:

1. Build a trust vector per pixel.
2. Classify tiles by risk.
3. Use cheap accumulation on stable tiles.
4. Use bounded candidate search on motion/shimmer-risk tiles.
5. Reject history quickly on disocclusion/reactive/reset tiles.
6. Gate sharpening/detail recovery by confidence.

Local evidence:

- `src/reconstruction/trust_field.*`
- `src/reconstruction/tile_classifier.*`
- `src/reconstruction/residual_search.*`
- `src/reconstruction/feature_locks.*`
- `src/reconstruction/shaders/temporal_resolve.hlsl`

## Why Feature Locks Exist

Readable text, rails, wires, and thin high-contrast features are where normal TAAU often trades clarity for stability. Feature locks are a conservative metadata path for stable detail:

- acquire only on strong stable edges;
- require low luma residual, high history trust, low motion, non-reactive and non-disoccluded evidence;
- decay or clear immediately when evidence breaks;
- boost detail recovery only while confidence is high.

This is not a license to sharpen everything. It is a way to avoid smearing stable high-frequency content while keeping bad-lock leakage measurable.

## Measurement Philosophy

The harness should answer "why did this frame look wrong?" not just "did it look sharp?"

Needed outputs:

- history weight;
- color residual;
- depth residual;
- feature-lock strength;
- reactive mask;
- MV X/Y views;
- disocclusion/rejection views;
- per-ROI metrics for text, motion, reactive, transparent, specular, and static regions;
- sequence metrics, not only single-frame captures.

Local evidence:

- `src/debug/capture_pack.*`
- `src/debug/capture_analysis.*`
- `src/debug/capture_compare.*`
- `profiles/capture_gate.cfg`
