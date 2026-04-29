# Research Backlog

This backlog is ordered by leverage for making oSR better than a normal upscaler in the local harness. Each item needs a measurable gate before it can be called progress.

## R1: Complete Input Contract Parity

Add missing `FrameContext` fields:

- frame time delta;
- camera near/far/FOV/view-space scale;
- explicit upscale size separate from display size;
- sharpness/debug flags;
- nonlinear color/HDR flags;
- resource state/provenance.

Gate: bridge metadata dumps show all fields, and validation warns on missing/unknown data instead of guessing.

## R2: XeSS-Style Inspector Panel For DX12 Wind Tunnel

Add a test-facing settings/diagnostic layer:

- MV scale/sign override;
- exposure override;
- responsive/reactive clamp;
- jitter freeze/scale;
- depth reversed toggle;
- debug view switching;
- capture-around-frame.

Gate: manual app and headless capture write the same settings into capture packs.

## R3: MV Truth Table Expansion

Current wind tunnel has correct, zero, flip-x, flip-y, half/double scale, and jitter-contaminated modes. Expand metrics:

- reprojection luma error;
- depth disagreement at reprojected sample;
- trusted-bad-history percentage;
- ROI-labeled worst pixel.

Gate: correct MV passes; corrupted MV modes fail with named reasons.

## R4: Depth-Dilated Foreground MV

Implement a 3x3/5x5 foreground-depth dilation experiment and compare history rejection around silhouettes.

Gate: lower edge ghost score around rails/cubes without increasing stable-surface rejection.

## R5: Risk-Tile GPU Residual Search

Port bounded residual search to HLSL only for MotionRisk/ShimmerRisk tiles.

Candidate set:

- MV center;
- plus/minus 1 pixel cross;
- optional diagonals in quality mode.

Score:

- luma/color residual;
- depth residual;
- motion prior distance;
- reactive/disocclusion penalties;
- previous trust.

Gate: improves motion/shimmer metrics over current temporal GPU path at bounded GPU cost.

## R6: Variance-Guided Trust

Track first/second luma moments and build a small variance pyramid.

Use variance to control:

- accumulation weight;
- spatial fallback;
- sharpening;
- residual-search enablement;
- feature-lock acquire/decay.

Gate: reduces shimmer on thin/text targets without smearing static detail.

## R7: Static Detail Confidence Map

Separate true stable text/edge detail from transparent-pane/specular stripe false positives.

Gate: improve text/native contrast ratio while lowering `bad_lock_signal`.

## R8: Real Bridge Capture

For FSR-style sample apps first:

- wrapper loads;
- dispatch intercepted;
- `FrameContext` logged;
- pass-through/debug output produced;
- one selected frame capture pack written.

Gate: no unsafe resource dereference, no crash, and all metadata fields validated.

## R9: Optional Tiny Neural Residual

Only after R5-R7 pass.

Inputs:

- current color;
- accumulated color;
- depth gradient;
- MV magnitude/residual;
- reactive;
- trust/weight/feature lock.

Gate: model disabled by default; enabled path must improve a fixed capture set within the 760M GPU budget.
