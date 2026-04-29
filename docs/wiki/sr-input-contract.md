# Temporal SR Input Contract

FSR, XeSS, Streamline/DLSS, and DirectSR converge on a similar input contract. This is the strongest evidence that oSR's first replacement layer should normalize renderer-provided temporal SR inputs rather than infer them from a final backbuffer.

## Required Core

| Field | Why it matters | Source |
| --- | --- | --- |
| Input color | Current jittered low-resolution frame. | AMD FSR manuals; Intel XeSS-SR guide; DirectSR spec. |
| Output color | Upscaled target texture. | AMD FSR manuals; Intel XeSS-SR guide; DirectSR spec. |
| Depth | Disocclusion, foreground selection, MV dilation, history rejection. | AMD FSR manuals; Intel XeSS-SR guide; DirectSR spec. |
| Motion vectors | Reproject previous history into the current frame. | AMD FSR manuals; Intel XeSS-SR guide; Streamline guide. |
| Render size | Input resolution for sampling, jitter, and quality mode. | AMD FSR dispatch, Intel XeSS execute params, DirectSR. |
| Output/display size | Target dimensions and scaling ratio. | AMD FSR dispatch, Intel XeSS quality settings, DirectSR. |
| Jitter offset | Provides subpixel samples over time; must match camera projection. | AMD FSR manuals; Intel XeSS-SR guide; Streamline constants. |
| Motion-vector scale/convention | Avoids sign/range bugs that create ghosting instantly. | AMD FSR manuals; Intel XeSS-SR guide. |
| Reset/history invalidation | Prevents stale history across cuts, resolution changes, and invalid resources. | AMD FSR dispatch, XeSS execute params, DirectSR ignore-history behavior. |

## Important Optional Inputs

| Field | oSR handling |
| --- | --- |
| Exposure / pre-exposure | Validate and log. Incorrect exposure changes color residuals and can cause false history rejection or ghosting. |
| Reactive/responsive mask | Prefer current-frame data for particles, alpha, changing shading, and content without reliable MVs. |
| Transparency/composition mask | Treat as a soft history/lock/clamp modifier, not always as full rejection. |
| HDR/color-space flags | Must be explicit; tonemapped vs linear mistakes break residual tests. |
| Reversed/infinite depth flags | Must be explicit; depth disagreement tests invert under reversed depth. |
| Sharpness | Runtime setting, not a quality claim. It must be confidence gated. |
| Camera near/far/FOV/view-space scale | Missing in current oSR `FrameContext`; FSR exposes these because depth interpretation and reactive logic may need them. |
| Frame time delta | Missing in current oSR `FrameContext`; needed for temporal consistency, animation scale, and vendor API parity. |

## Local Mapping

`src/core/frame_context.h` already contains:

- color input/output, depth, motion vectors
- optional reactive mask and exposure texture
- render/display dimensions
- jitter and motion-vector scale
- motion-vector space
- reset, depth inverted, infinite depth, jittered MV, display-resolution MV flags
- HDR/color-space metadata

`src/interop/fsr2_bridge/fsr_bridge.cpp` normalizes FSR-style dispatch data into this contract. The next contract upgrade should add frame time, camera parameters, explicit upscale size, sharpness/debug flags, and resource state/provenance.

## Non-Negotiable Validation

oSR must never silently guess:

- MV sign or scale.
- Whether MVs include jitter.
- Whether depth is reversed.
- Whether color is linear, HDR, scRGB, PQ, or already tonemapped.
- Whether exposure is valid.

The correct behavior is to validate, log, visualize, and fall back conservatively.
