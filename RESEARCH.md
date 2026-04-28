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

## 2026 Research Update: What Actually Pushes Us Forward

The current high-end direction is not merely "AI upscaling." It is context selection.

NVIDIA says DLSS 4 moved Super Resolution/Ray Reconstruction/DLAA from CNNs to transformers because attention over broader spatial-temporal context improves temporal stability, ghosting, detail in motion, and edge quality. NVIDIA's DLSS 4.5 material says the second-generation transformer uses much more compute and has more intelligent use of pixel sampling and motion vectors:

- <https://www.nvidia.com/en-us/geforce/news/gfecnt/20251/dlss4-multi-frame-generation-ai-innovations/>
- <https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/>

That is not directly portable to Radeon 760M as the core path. The useful lesson is to replace fixed heuristics with better sample selection, but keep it deterministic and shader-cheap:

```text
Transformer lesson: compare many possible histories and choose useful context.
oSR translation: score a tiny set of plausible histories using depth/MV/color/reactive evidence, and only do this where tile risk says it is worth the cost.
```

The most important theory pivot is therefore:

```text
oSR is not a sharper scaler. oSR is a low-cost temporal evidence system.
```

If the evidence system is good, spatial upscale and sharpening can be modest. If the evidence system is bad, no sharpening policy saves it.

## April 2026 Research Update: The Real Target Has Moved

The landscape is moving even harder toward ML, but the hardware split matters for oSR.

AMD's current FSR SDK page lists FSR Upscaling 4.1.0 and Ray Regeneration 1.1.0 under the FSR "Redstone" SDK, with FSR Upscaling 4 requiring Radeon RX 9000-class hardware and automatically falling back to FSR 3.1.5 on other hardware. That means our Radeon 760M target remains in the analytical/low-cost class even inside AMD's own stack: <https://gpuopen.com/amd-fsr-sdk/>

The implication is blunt: oSR cannot beat the newest transformer/ML paths by pretending to be one of them on weak hardware. The credible target is to beat ordinary spatial upscaling and naive temporal accumulation by using the renderer data better than generic post-process scalers:

- Validate the game-provided motion vectors instead of trusting them blindly.
- Treat depth disagreement, disocclusion, reactive/transparency masks, exposure mismatch, and reset flags as first-class evidence.
- Spend extra samples only on risk tiles.
- Make every confidence decision visible in captures and logs.

That is the practical breakthrough target: not "bigger model," but "better evidence accounting per millisecond."

## Proxy/Replacement Research Notes

OptiScaler validates the replacement-layer strategy but remains GPL research input only. Its public README describes the same middleware shape oSR is using: game upscaler input calls are intercepted and redirected to another backend, with DLSS/XeSS/FSR inputs normalized into an internal path. It also calls out that FSR 3.1 is the first forward-looking standardized FSR API, while some older FSR2/FSR3 integrations are more game-specific: <https://github.com/optiscaler/OptiScaler>

For No Man's Sky, local executable inspection found a simpler path: `NMS.exe` imports `libxess.dll` and references the Vulkan XeSS entrypoints `xessVKCreateContext`, `xessVKInit`, `xessVKExecute`, plus `xessGetProperties` and `xessGetInputResolution`. The current oSR proxy is therefore intentionally diagnostic and pass-through. It proves the loading and call boundary before we attempt resource decoding or replacement.

## Source-Backed Constraints

- The TAA survey frames temporal upscaling as sample accumulation plus history validation. This makes history validation the central bottleneck, not edge interpolation: <https://research.nvidia.com/labs/rtr/publication/yang2020survey/>
- XeSS-SR requires jitter, input color, motion vectors, depth for low-res MV mode, exposure, responsive masks, and history reset. It also documents debugging via static scenes, jitter sign/scale checks, MV scale/sign checks, and longer jitter sequences: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- FSR2/FSR temporal docs identify the same core machinery: reactive masks, transparency/composition masks, depth/MV reconstruct-and-dilate, depth clip, locks, reproject-and-accumulate, and RCAS. AMD specifically calls out alpha-blended objects/particles as needing reactive handling: <https://gpuopen.com/manuals/fidelityfx_sdk/techniques/super-resolution-temporal/>
- DirectSR standardizes the same input contract: target/source color, source depth, motion vectors, MV scale, camera jitter, exposure/pre-exposure, ignore-history mask, reactive mask, and sharpness: <https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html>

## Breakthrough Candidate: Trust-Guided Sparse Temporal Attention

The proposed oSR differentiator is a hybrid between TAAU and attention:

1. Build a per-pixel trust vector, not a scalar only.
2. Classify each 8x8 or 16x8 tile into stable, shimmer-risk, motion-risk, reactive-risk, disocclusion-risk, or reset.
3. Stable tiles take a very cheap path.
4. Risk tiles run bounded residual search around the MV reprojection.
5. Disocclusion/reactive/reset regions reject history quickly instead of searching stale history.
6. Sharpening is gated by confidence, never applied blindly.

This gives us the thing transformers are good at, contextual selection, but in a bounded form:

```text
Full transformer: broad learned attention across pixels/frames.
oSR v0-v1: deterministic sparse attention over a few local candidates, gated by tile risk and confidence.
```

The breakthrough threshold is not "beats DLSS everywhere." The real target is:

```text
FSR-class or better temporal stability on unsupported/low-end hardware,
with explainable debug views and a lower cost than neural SR.
```

## Trust Field v2 Research Target

The current trust field is a good first scaffold, but it is still closer to strong TAAU than to the kind of context selection that makes modern SR convincing. The next research target is Trust Field v2:

1. Compute a per-pixel confidence vector from motion validity, depth consistency, luma/color residual, reactive suppression, exposure validity, disocclusion risk, MV divergence, and previous trust.
2. Add feature locks for stable thin edges and readable text. Locks should preserve detail only while evidence stays stable, then decay over the jitter sequence and unlock immediately on luma instability, reactive pixels, disocclusion, reset, or large residuals.
3. Replace RGB-only neighborhood clipping with YCoCg/luma variance clipping. This should reduce hue shifts and over-rejection on high-frequency readable patterns.
4. Port bounded residual search to HLSL, but only for MotionRisk/ShimmerRisk tiles. Stable tiles must stay cheap.
5. Add a small super-history metadata path for static high-frequency tiles: confidence/age/lock data first, not an expensive extra full-color history by default.
6. Move sharpening toward confidence-gated CAS/RCAS-style behavior so stale history, reactive particles, and new disocclusions are not sharpened into visible trails.

The testable claim we are aiming for is narrow and serious:

```text
On deterministic wind-tunnel scenes, Trust Field v2 should preserve static thin/text detail and reduce motion/disocclusion history misuse versus the current trust field at similar or bounded additional cost.
```

## Theory Experiments To Implement Next

These are ordered by leverage and testability in the DX12 wind tunnel.

### 1. Motion-Vector Truth Table

Create wind-tunnel modes that deliberately flip or scale motion vectors:

- correct pixel-space current-to-previous MV
- X sign flipped
- Y sign flipped
- half scale
- double scale
- jitter-contaminated MV
- zero MV

Expected output: validation logs, MV debug view, and a numeric "reprojection error" metric. This comes directly from XeSS debugging guidance around MV scale/sign and static-scene tests.

### 1A. Baseline History Validators

The trust field must compete against sane traditional baselines, not a toy fixed-alpha blend.

Implement at least three history validators:

- YCoCg neighborhood clamp.
- Luma variance clamp.
- oSR trust-field clamp using color, depth, MV, reactive/disocclusion, and previous trust.

Expected output: side-by-side debug views for current color, reprojected history, accepted history, rejected history, and final output. This is the first place where "better than normal upscalers" becomes measurable rather than rhetorical.

### 2. Depth-Dilated Foreground MV

Implement the FSR/XeSS-style 3x3 foreground dilation experiment:

- Select nearest foreground depth in a neighborhood.
- Carry that pixel's MV into a dilated MV buffer.
- Compare disocclusion classification with and without dilation.

Expected output: fewer edge ghosts around rails/cubes, better disocclusion mask stability, and a measurable reduction in invalid history use around silhouettes.

### 3. Reactive-Mask Synthesis

Use the synthetic scene's particles and alpha-like objects to compare:

- supplied reactive mask
- no reactive mask
- luminance-delta synthesized reactive mask
- clamped reactive mask at 0.8/0.9

FSR and XeSS both say reactive/responsive masks are important for particles/transparency. The research question is whether oSR can synthesize a good-enough mask when games do not provide one.

### 4. Trust-Field Heatmap And Numeric Metrics

The trust field must become visible and measurable:

- history trust
- evidence trust
- accumulation weight
- disocclusion
- reactive value
- residual-search confidence

Metrics per frame:

- percent stable pixels
- percent history-rejected pixels
- percent residual-search pixels
- max/mean MV magnitude
- mean depth disagreement at reprojected samples
- sharpen amount by tile class

This is our observability advantage over black-box SR.

### 4A. Variance-Guided Multi-Scale Trust

SVGF is a denoising paper rather than an SR paper, but its core lesson is directly useful: use temporal accumulation plus luminance variance to distinguish noise/instability from real detail across scales.

Source: <https://research.nvidia.com/labs/rtr/publication/schied2017spatiotemporal/>

Experiment:

- Track first and second luma moments.
- Build a small variance pyramid.
- Use variance to control accumulation weight, spatial fallback radius, sharpening amount, and residual-search enablement.

This targets thin-rail shimmer, checkerboard shimmer, and readable texture/detail stability.

### 5. Bounded Residual Search On Risk Tiles

Port CPU `residual_search.*` into an HLSL pass, but only for MotionRisk/ShimmerRisk tiles.

Search pattern:

```text
center MV candidate
plus/minus 1 pixel cross
optional diagonals for quality mode
score = luma error + depth error + motion-prior distance + reactive penalty
```

This is the first concrete "attention-like" GPU path.

### 5A. Reservoir-Inspired Candidate Reuse

ReSTIR is not an SR algorithm, but its important transferable idea is selective spatial/temporal reuse of candidates instead of blind post-filtering. Area ReSTIR is especially interesting because it explicitly addresses subpixel/film-space reuse for antialiasing.

Sources:

- <https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/>
- <https://research.nvidia.com/labs/rtr/publication/zhang2024area/>

oSR should not copy ReSTIR reservoirs literally in v0. The near-term experiment is a deterministic candidate pool:

- current bilinear sample
- current edge-aware taps
- reprojected history sample
- previous-frame neighbor candidates
- subpixel jitter candidate

Then score by depth, MV, color/luma, trust, reactive, and motion-prior distance. Tile risk decides whether the candidate pool runs.

### 6. Confidence-Gated Sharpening In GPU Path

Move `ConfidenceGatedSharpness` into HLSL after accumulation:

- strong on stable/high-trust opaque detail
- weak on reactive pixels
- off on disocclusions/reset
- reduced on low trust

The goal is to avoid the common failure mode where upscalers look sharp in screenshots but smear or leave sharp trails in motion.

### 7. Tiny Neural Refinement Only After Trust Works

A tiny neural pass is not the breakthrough by itself. It only becomes interesting if it uses trust-field features:

Inputs could be:

- current color
- accumulated color
- depth gradient
- MV magnitude
- reactive
- trust/weight

Output should be a small residual, not a full reconstruction. It must be optional and disabled by default until the deterministic path is proven.

## Measurement Philosophy

We need to stop asking "does it look better?" first. The harness should produce repeatable evidence:

- fixed camera path with deterministic jitter
- fixed particle path
- ground-truth native render mode where possible
- low-res input + reconstructed output
- per-frame metadata and debug views
- CSV metrics for ghosting/disocclusion/motion clarity

The first "better than normal upscalers" claim we are allowed to make should be narrow:

```text
On oSR wind-tunnel scenes, trust-guided sparse temporal attention reduces measured history misuse around motion/disocclusion/particles compared with fixed-alpha temporal accumulation at similar cost.
```

That is a real claim, testable in our harness, and a credible stepping stone.

## Longer-Term Theory: History Resurrection

Unreal TSR exposes a useful non-neural idea: history resurrection. It keeps older persistent frames and can use an older history if it better matches the current frame than the immediate previous frame. Epic's TSR docs also expose debug views for accumulated samples, parallax disocclusion, history rejection, clamping, resurrection, spatial AA, and flickering temporal analysis.

Source: <https://dev.epicgames.com/documentation/unreal-engine/temporal-super-resolution-in-unreal-engine>

oSR version:

- Keep a tiny "graveyard" history, probably 2-4 sparse frames or low-resolution confidence data.
- Only try resurrection on ShimmerRisk/MotionRisk tiles where the immediate history has low trust.
- Reject resurrection immediately on reactive/disocclusion/reset.
- Compare candidates using the same trust score used for residual search.

This is not a Phase 1 feature, but it is a serious future differentiator for recurring occlusion and readable details that disappear/reappear.
