# NVIDIA DLSS And Streamline Notes

DLSS is the clearest signal for where top-end SR quality is going, but it is the least directly borrowable. The model internals are proprietary. Streamline is the useful public integration concept; NVIDIA's public DLSS 4/4.5 material is useful for the research direction.

## Streamline Integration Shape

Streamline standardizes feature integration around application-provided constants and resources. Public docs describe the usual temporal SR inputs: color, depth, motion vectors, jitter, render/output sizes, reset, exposure/HDR context, and feature options.

Primary source:

- <https://github.com/NVIDIA-RTX/Streamline/blob/main/docs/ProgrammingGuide.md>

oSR should treat Streamline as a contract reference, not a dependency for v0. The first practical bridge remains FSR-style/DX12 and diagnostic XeSS proxy work.

## DLSS 4 And 4.5 Public Claims

NVIDIA publicly states that DLSS 4 moved Super Resolution, Ray Reconstruction, and DLAA from CNNs to transformer models, with claimed improvements in temporal stability, ghosting, motion detail, and edge quality. NVIDIA's DLSS 4.5 developer article describes a second-generation transformer model and emphasizes more intelligent use of pixel sampling and motion vectors.

Primary sources:

- <https://www.nvidia.com/en-us/geforce/news/gfecnt/20251/dlss4-multi-frame-generation-ai-innovations/>
- <https://developer.nvidia.com/blog/nvidia-dlss-4-5-delivers-super-resolution-upgrades-and-new-dynamic-multi-frame-generation/>

Important boundary: those articles do not publish the model topology, weights, training set, or inference kernels. We can use them to infer the direction of travel, not reproduce DLSS.

## CNN vs Transformer Lesson

The implementation-relevant lesson is not "use a transformer on Radeon 760M." The useful lesson is context selection:

- CNNs are efficient local filters; broad context requires depth/layer stacking.
- Attention/transformer-style models can select information from broader spatial/temporal neighborhoods.
- Temporal SR quality depends heavily on selecting valid history while rejecting stale history.

For oSR, translate that into:

```text
learned broad attention -> deterministic sparse temporal attention
```

Meaning:

- score a small candidate set around motion-vector reprojection;
- route work by tile risk;
- reject history aggressively on depth/reactive/disocclusion/reset evidence;
- preserve stable thin/readable details with feature locks only while evidence stays stable;
- expose the confidence map instead of hiding it in a model.

## What To Borrow

Safe to borrow:

- The public input-contract shape.
- The design principle that context selection matters more than simple sharpening.
- The need for temporal stability, motion clarity, and edge quality as separate metrics.
- Streamline's idea of feature constants/resources, without depending on NVIDIA-only paths.

Do not borrow:

- Proprietary DLSS binaries, model weights, or reverse-engineered internals.
- Claims that oSR is "better than DLSS" globally.
- Heavy transformer inference as the default path on Radeon 760M.

## oSR Competitive Reality

The realistic target is not to beat DLSS 4/4.5 universally. The serious target is narrower:

```text
beat normal spatial upscaling and naive temporal accumulation on our harness,
approach FSR/XeSS-class behavior in selected cases,
and do it with better observability and lower hardware requirements.
```

Only measured capture gates can promote that from a hypothesis to a claim.
