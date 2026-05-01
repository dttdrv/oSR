# oSR Architecture

oSR is a clean-room prototype for a harness-first temporal super-resolution replacement layer. The first runtime bridge targets Windows games or sample applications that already expose DLSS2+/FSR2+/XeSS-style temporal upscaler inputs, but the core reconstruction lab is kept portable so Linux development can proceed in parallel. The prototype does not attempt to recover missing renderer data from arbitrary games.

Practical operation docs live under `docs/`: `PROJECT_STATUS.md`,
`HARNESS_USER_GUIDE.md`, `CAPTURE_PACKS.md`, `LINUX.md`, and
`DEVELOPMENT.md`.

## Design Goals

- Capture a normalized temporal SR frame contract from an existing upscaler integration.
- Keep vendor-facing bridges isolated from reconstruction code.
- Prefer deterministic, debuggable compute passes before visual quality.
- Make reconstruction decisions explainable through confidence/trust fields instead of opaque fixed heuristics.
- Keep all major claims source-backed in `LOG.md` and design docs.
- Avoid anti-cheat titles and online multiplayer targets.

## Source-Backed Constraints

- OptiScaler demonstrates the practical shape of an upscaler replacement layer: game input API calls are intercepted and routed to another backend, described as `Inputs -> OptiScaler -> Outputs` in its README: <https://github.com/optiscaler/OptiScaler>
- AMD FSR Super Resolution requires current-frame color, depth, motion vectors, output, jitter, motion-vector scale, exposure handling, reset, and optional masks: <https://gpuopen.com/manuals/fsr_sdk/techniques/super-resolution-upscaler/>
- FSR3 upscaler dispatch exposes `color`, `depth`, `motionVectors`, `exposure`, `reactive`, `transparencyAndComposition`, `output`, `jitterOffset`, `motionVectorScale`, `renderSize`, `upscaleSize`, `preExposure`, and `reset`: <https://gpuopen.com/manuals/fidelityfx_sdk/reference_documentation/structs/ffx_fsr3_upscaler_dispatch_description/>
- Intel XeSS-SR similarly requires jitter, input color, motion vectors, optional depth for low-resolution motion vectors, exposure handling, responsive masks, and history reset: <https://www.intel.com/content/www/us/en/developer/articles/technical/xess-sr-developer-guide.html>
- NVIDIA Streamline is useful as an integration model for per-frame resource tags, constants, and explicit evaluation without depending on NVIDIA-only paths: <https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md>
- Microsoft DirectSR documents a vendor-neutral SR input model with source/target color, source depth, motion vectors, motion-vector scale, jitter, exposure, ignore-history/reactive masks, and image regions: <https://microsoft.github.io/DirectX-Specs/DirectSR/DirectSR.html>
- NVIDIA reports that DLSS 4/4.5 moved Super Resolution from CNNs to transformer models to improve temporal stability, ghosting, detail in motion, and anti-aliasing, with DLSS 4.5 using substantially more compute than the first transformer model: <https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/>
- AMD positions FSR "Redstone"/FSR Upscaling as ML-powered on newer RDNA 4 hardware, while FSR 3.1 remains the broad hardware fallback: <https://www.amd.com/en/products/graphics/technologies/fidelityfx/super-resolution.html>
- The TAA survey frames temporal antialiasing and temporal upsampling around two core problems: sample accumulation and history validation: <https://research.nvidia.com/labs/rtr/publication/yang2020survey/>

## Data Flow

```text
Game FFX call
  -> interop/fsr2_bridge
  -> core/FrameContext
  -> debug/validation
  -> backends/dx12
  -> reconstruction/debug spatial pass
  -> output resource
```

The first bridge path is FSR3.1/FidelityFX API style DX12 input. The name `fsr2_bridge` is retained because the temporal input contract spans FSR2-style and FSR3 upscaler integrations.

## Normalized Frame Contract

`core::FrameContext` is the central contract. It records:

- input color, output color, depth, motion vectors
- render size and display/output size
- jitter offset
- motion-vector scale and convention
- exposure scalar, exposure texture, and pre-exposure
- reactive/responsive mask
- transparency/composition mask when available
- reset/history invalidation flag
- HDR/color-space flags
- depth inverted/infinite flags

Internal/debug resources may also be tracked as typed metadata:

- history color
- trust field
- synthesized reactive mask
- debug visualization output

The bridge must never silently infer motion-vector scale. If scale or convention is missing, validation must log the uncertainty and keep the frame in a conservative/debug-only mode.

## Breakthrough Track: Trust-Field Reconstruction

oSR's research direction is not to out-muscle DLSS 4.5/FSR 4/XeSS with a larger model on a small iGPU. The target is narrower and testable: beat current upscalers in quality-per-millisecond and motion clarity on Radeon 760M-class hardware for selected scenes by exploiting the temporal inputs games already provide.

The proposed mechanism is a per-pixel trust field:

```text
history_trust =
  previous_trust
  * depth_consistency
  * motion_consistency
  * color/neighborhood_consistency
  * non_reactive
  * non_disoccluded
```

The trust field drives:

- history accumulation weight
- conservative fallback to current frame
- reactive-mask synthesis when no engine mask is supplied
- confidence-gated sharpening
- debug views for history trust and accumulation weight

This borrows the useful idea behind transformer attention: select which past/context samples deserve influence. It avoids the cost of full attention or heavy neural inference by using bounded, shader-friendly confidence signals.

Near-term hypotheses:

- Confidence-gated accumulation can beat fixed-alpha TAAU in ghosting/shimmer tradeoffs.
- Confidence-gated sharpening can avoid the common "sharp ghost" artifact.
- A compact trust/age field can recover much of the benefit of heavier temporal models on low-power hardware.
- Optional tiny neural modules may become a later detail prior, but the v0/v1 core must remain deterministic and fast.

## Modules

- `src/core`: API-neutral data model, config, logging, resource registry.
- `src/backends/dx12`: DX12 resource handling, barriers, descriptors, and debug copy/upscale dispatch.
- Future `src/backends/vulkan`: Linux/Proton-friendly resource handling and writer experiments. This is intentionally separate from DX12 rather than hidden behind a premature abstraction.
- `src/interop/fsr2_bridge`: FFX/FSR-style input normalization and exported wrapper skeleton.
- `src/interop/xess_bridge`: v0 stub only; documents future XeSS-SR boundary.
- `src/interop/dlss_bridge`: v0 stub only; documents future DLSS/NVNGX boundary.
- `src/reconstruction`: spatial, temporal, history rejection, disocclusion, and sharpening passes.
- `src/reconstruction/trust_field.*`: CPU-testable trust policy mirrored by future HLSL.
- `src/reconstruction/reactive_mask_synthesis.*`: fallback reactivity estimate when the game provides no mask.
- `src/profiles`: per-game/profile settings.
- `src/debug`: metadata capture, overlay placeholder, and validation.
- `src/tests`: unit tests for API-neutral behavior.

## Supported V0 Path

V0 supports a local DX12 sample-app proof of life:

1. A wrapper library loads.
2. FFX upscaler context creation/dispatch is intercepted or called through the sample-controlled bridge.
3. Dispatch data is normalized into `FrameContext`.
4. Validation logs metadata.
5. A trivial DX12 debug copy/upscale path is selected.

## Explicit Non-Goals

- Frame generation.
- Anti-cheat or online multiplayer games.
- DLSS/NVNGX spoofing.
- XeSS runtime replacement.
- Vulkan or DX11.
- Pattern scanning older statically linked FSR2 integrations.
- Heavy neural inference as the core reconstruction path.
- Claims of being better than DLSS/XeSS/FSR in general.
