# oSR Research Direction

This document tracks the current research hypothesis for moving beyond a normal upscaler without overclaiming universal superiority.

## Landscape Summary

The industry direction is clear: high-end SR is moving toward learned temporal reconstruction.

- NVIDIA DLSS 4 moved Super Resolution, Ray Reconstruction, and DLAA from CNNs to transformer models; NVIDIA says the transformer improves temporal stability, reduces ghosting, improves detail in motion, and smooths edges. DLSS 4.5 adds a second-generation transformer model and NVIDIA says it uses much more compute than the first transformer model: <https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/>
- AMD FSR 4 / FSR "Redstone" is ML-powered and focused on newer RDNA 4 hardware, while broad fallback hardware still uses FSR 3.1-class upscaling: <https://www.amd.com/en/products/graphics/technologies/fidelityfx/super-resolution.html>
- Intel XeSS-SR is AI temporal supersampling that depends on accurate jitter, color, motion vectors, depth in low-res-MV mode, exposure, responsive masks, and reset handling: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- DirectSR standardizes the input contract instead of defining one reconstruction model: <https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html>

## Why Temporal Beats Pure Spatial Upscaling

Pure spatial upscaling only sees one low-resolution image. It can sharpen, interpolate edges, and infer local detail, but it cannot recover subpixel samples that were only visible in adjacent frames.

Temporal SR uses jitter plus history. Each frame samples the scene slightly differently; motion vectors and depth let the algorithm reproject previous samples into the current frame. The 2020 TAA survey describes temporal antialiasing as temporally amortized supersampling and identifies sample accumulation plus history validation as the core problems: <https://research.nvidia.com/labs/rtr/publication/yang2020survey/>

The hard part is deciding when history is trustworthy. Bad history causes ghosting, trails, shimmer, and smearing.

## Why AI/Transformers Are Winning Quality

CNNs are efficient local filters. They are good at texture extraction and compact inference, but they build wider context only by stacking layers.

Transformers and attention-style models can compare broader spatial/temporal regions more directly. Public DLSS material says the shift from CNN to transformer improved stability and motion clarity. Research systems such as SwinIR and video transformer/recurrent models show the same trend in image/video restoration: more context and better temporal alignment improve reconstruction quality.

The cost problem is severe. Full attention or video SR transformer inference is not a sensible default for Radeon 760M-class iGPU hardware. The useful lesson for oSR is the selection principle, not the full model.

## oSR Hypothesis

oSR should implement attention-like selection as deterministic shader logic:

- Reproject previous history with motion vectors.
- Score history with depth consistency, motion consistency, local color/luma agreement, reactive mask, disocclusion, reset state, and previous trust.
- Store a compact trust/age field.
- Let the trust field drive accumulation weight, reactive synthesis, sharpening strength, and debug visualization.

This is a "trust-field" temporal upscaler: every major temporal decision is observable and tunable. The goal is to win quality-per-millisecond and debugging control on Radeon 760M, not to claim universal superiority over proprietary neural models.

The trust field must not be a one-bit reject mask. Research and engine docs point to a multi-signal, soft policy: stable opaque surfaces should rebuild trust after temporary uncertainty, while disocclusion, reactive particles, exposure/color mismatch, invalid motion vectors, and reset events must immediately reduce or clear history weight.

## Current Oracle

The CPU oracle in `src/reconstruction/temporal_oracle.*` is the correctness reference for the shader implementation. It tests:

- canonical stable-surface trust and history weight
- reset dominance over all other signals
- disocclusion current-frame fallback
- reactive/current-frame fallback
- monotonic history-weight reduction as motion grows
- shimmer variance reduction in stable noisy content
- fuzz invariants for finite, bounded trust/current/history weights

The first robust shimmer test found a real bug: pure previous-trust decay slowly killed stable history even when current evidence was good. The policy now separates evidence trust from memory and allows stable evidence to rebuild trust.

## Tile-Gated Work

Radeon 760M cannot afford expensive validation everywhere. The tile classifier in `src/reconstruction/tile_classifier.*` is the first pass toward redundancy-aware reconstruction:

- stable tiles take the cheap path
- motion-risk tiles can run residual search around the motion-vector reprojection
- reactive-risk tiles prefer current frame and suppress sharpening
- disocclusion-risk tiles reject history more aggressively
- reset tiles clear history immediately

This is where attention-like behavior becomes practical: spend additional samples only where the trust field says the frame is ambiguous.

Tile risk priority is intentional and tested:

```text
Reset > DisocclusionRisk > ReactiveRisk > MotionRisk > ShimmerRisk > Stable
```

The priority exists to prevent expensive or stale history paths from overriding hard invalidation events.

## Bounded Residual Search

The first attention-like candidate selector is `src/reconstruction/residual_search.*`. It does not estimate optical flow. Instead, it searches a tiny window around the game-provided motion-vector prediction and chooses the history candidate with the best luma/depth/prior score.

This is deliberately gated by tile risk:

- stable tiles should skip it
- motion-risk/shimmer-risk tiles can use it to correct small motion-vector residuals
- disocclusion/reactive/reset tiles should usually prefer rejection over search

The prototype is CPU-only for now so shader behavior can be tested against exact candidate choices before GPU implementation.

## Confidence-Gated Sharpening

The first sharpening policy is in `src/reconstruction/sharpening.*`. It suppresses sharpening in disocclusions, damps sharpening in reactive regions, and scales sharpening upward with history trust.

This directly targets the "sharp ghost" failure mode: sharpening should not amplify stale reprojected history, particles, or newly revealed geometry.

## Hardware Implication

Radeon 760M is a small RDNA 3 integrated GPU with shared system memory. It has enough compute for lightweight compute passes but limited bandwidth and thermal headroom. The default path should target:

- 900p to 1200p/1080p under roughly 1.0-1.2 ms.
- 1920x1200 output under 1.5 ms for default mode.
- Opt-in temporal quality mode capped around 2.5 ms.
- FP16-friendly math, compact confidence/reactive formats, and minimal full-resolution history surfaces.

## Breakthrough Criteria

The research track counts as a breakthrough only if measured captures show at least one of:

- less ghosting than fixed-alpha TAAU/FSR-style baseline at equal or lower cost
- clearer motion than spatial-only upscaling at similar cost
- fewer sharpened trails from confidence-gated sharpening
- more stable particles/transparencies from synthesized or supplied reactive masks
- better debugability through trust/weight visualizations
