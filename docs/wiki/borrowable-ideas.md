# Borrowable Ideas

This page separates useful public ideas from things oSR must not copy.

## Borrow Now

### FSR-Style Input Normalization

Normalize every bridge into `FrameContext`: color, output, depth, MVs, render/display size, jitter, exposure, masks, reset, depth/MV/color-space flags. This is already the right architecture and is validated by FSR, XeSS, Streamline, and DirectSR.

### Debug-First Validation

Borrow the vendor mindset that SR quality is often input correctness. Build tools that catch:

- wrong MV sign/scale;
- jitter-contaminated MVs;
- reversed depth mismatch;
- missing/invalid reactive masks;
- bad exposure;
- reset/history mistakes;
- color-space/HDR mismatches.

### Risk-Tile Routing

Only spend expensive work where the frame is ambiguous. Stable tiles should stay cheap. Motion/shimmer-risk tiles can run bounded candidate search. Reactive/disocclusion/reset regions should usually reject, not search.

### Confidence-Gated Detail Recovery

Sharpening must be downstream of confidence. Stable opaque high-trust detail can sharpen; reactive, disoccluded, reset, or low-trust pixels should not.

### Inspector-Style Harness

Borrow XeSS Inspector's product idea:

- state display;
- overrides;
- debug overlays;
- histograms;
- frame dumps;
- capture-around-frame workflows.

This is how manual eye testing becomes actionable engineering data.

## Borrow Later

### Variance-Guided Multi-Scale Trust

SVGF motivates tracking luma moments/variance over time and across scales. oSR should test whether a small variance pyramid can control accumulation weight, residual-search enablement, and sharpening.

Source: <https://research.nvidia.com/labs/rtr/publication/schied2017spatiotemporal/>

### Reservoir-Inspired Candidate Reuse

ReSTIR and Area ReSTIR are not SR algorithms, but their lesson is useful: select/reuse good candidates instead of blindly filtering everything. oSR can test a deterministic mini candidate pool:

- current bilinear sample;
- current edge-aware taps;
- reprojected history sample;
- nearby previous-frame candidates;
- jitter candidate.

Sources:

- <https://research.nvidia.com/labs/rtr/publication/bitterli2020spatiotemporal/>
- <https://research.nvidia.com/labs/rtr/publication/zhang2024area/>

### History Resurrection

Unreal TSR exposes history resurrection and rich debug views. oSR can later keep a tiny older-history metadata path for recurring occlusion/readable details, but only under strict trust scoring.

Source: <https://dev.epicgames.com/documentation/unreal-engine/temporal-super-resolution-in-unreal-engine>

### Tiny Neural Residual

A tiny optional residual model is only interesting after deterministic trust works. It should consume trust features and output a small correction, not replace the reconstruction core.

## Do Not Borrow

- Proprietary model weights, inference kernels, shaders, or reverse-engineered behavior.
- GPL implementation code from OptiScaler unless the project deliberately changes licensing posture.
- Vendor marketing claims as oSR claims.
- Heavy transformer inference as the main path on Radeon 760M.
- Silent heuristics that guess MV/depth/color conventions.
