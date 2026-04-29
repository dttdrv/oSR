# AMD FSR Notes

FSR is the most useful public reference for oSR because AMD exposes a detailed temporal SR contract, pass taxonomy, debug philosophy, and open SDK headers. oSR should use those as clean-room design input, not copy shader internals blindly.

## Public Architecture Shape

FSR2/FSR3 temporal upscaling is a multi-pass temporal reconstruction pipeline. Local SDK headers expose pass names that match the expected machinery:

- prepare/reconstruct depth
- depth clip
- lock creation/update
- luma pyramid / luminance instability
- reactivity preparation or reactive generation
- reproject and accumulate
- RCAS/sharpen
- debug view

Local evidence:

- `external/FidelityFX-SDK/Kits/FidelityFX/upscalers/fsr3/include/ffx_fsr2.h`
- `external/FidelityFX-SDK/Kits/FidelityFX/upscalers/fsr3/include/ffx_fsr3upscaler.h`

The public lesson for oSR is the pass decomposition: separate input preparation, history validation, accumulation, reactive handling, detail recovery, and debug output.

## Dispatch Contract

FSR-style dispatch includes color, depth, motion vectors, optional exposure, optional reactive and transparency/composition masks, output, jitter, MV scale, render/upscale size, pre-exposure, reset, sharpness, frame time, and camera parameters. Local evidence is in:

- `external/FidelityFX-SDK/Kits/FidelityFX/upscalers/include/ffx_upscale.h`
- `external/FidelityFX-SDK/Kits/FidelityFX/upscalers/fsr3/include/ffx_fsr3upscaler.h`

oSR currently maps the core subset in `src/interop/fsr2_bridge/fsr_bridge.cpp`; gaps are tracked in [sr-input-contract.md](sr-input-contract.md).

## Motion Vectors

FSR expects current-to-previous 2D motion vectors in screen-pixel range, with `motionVectorScale` available when an engine emits another range such as NDC. AMD documents that render-resolution inputs should be jittered, while motion vectors should not include jitter unless the jitter-cancellation flag is set.

oSR implication: validation and debug views for MV sign, scale, jitter contamination, and resolution are not optional. The existing DX12 wind tunnel MV truth-table modes are directly aligned with this requirement.

## Reactive And Transparency Masks

AMD explicitly treats alpha-blended objects and particles as hard cases for temporal accumulation. Reactive masks increase current-frame influence where depth/MV/color alone cannot explain changing transparent content. Transparency/composition is a softer mask that influences locks/clamping/luma stability.

oSR implication:

- Supplied masks should be trusted only after dimension/format validation.
- Missing masks should trigger conservative synthesis, not blind history.
- Reactive regions should suppress feature locks and sharpening.

Local evidence:

- `src/reconstruction/reactive_mask_synthesis.*`
- `src/reconstruction/feature_locks.*`
- `src/reconstruction/shaders/temporal_resolve.hlsl`

## FSR4 / Redstone Boundary

AMD's FSR SDK page and ML upscaling documentation show that the newer ML path is hardware-gated and distributed through AMD's SDK/runtime path. The architecture and weights are not open. The credible oSR stance is:

- Use FSR4 public docs to understand the integration contract.
- Do not claim FSR4 compatibility unless using official AMD runtime paths.
- Do not attempt to reverse engineer signed binaries.
- Keep oSR's core path deterministic and lightweight for Radeon 760M-class hardware.

## What To Borrow

Safe to borrow:

- Input contract.
- Jitter/MV convention discipline.
- Pass boundaries.
- Reactive/T&C mask semantics.
- Debug checker and overlay mindset.
- Context lifetime/resource ownership model.
- Negative mip-bias and quality-mode control concepts.
- Shared dilated depth/MV as a future interoperability idea.

Not safe or not useful for v0:

- Copying proprietary binaries or model behavior.
- Depending on FSR4 ML internals.
- Treating FSR as a universal quality ceiling.
