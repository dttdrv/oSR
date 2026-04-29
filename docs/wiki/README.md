# oSR LLM Wiki

This directory is the project memory for super-resolution research. It is written for future agents and humans: short enough to search, strict enough to keep claims tied to sources or local code.

## Index

- [source-index.md](source-index.md) - primary sources and local code evidence.
- [sr-input-contract.md](sr-input-contract.md) - the common temporal SR input contract across FSR, XeSS, Streamline/DLSS, and DirectSR.
- [fsr.md](fsr.md) - AMD FSR2/FSR3/FSR4 public architecture notes.
- [xess.md](xess.md) - Intel XeSS-SR SDK notes and diagnostic lessons.
- [dlss-streamline.md](dlss-streamline.md) - public DLSS/Streamline concepts, including DLSS 4/4.5 transformer claims.
- [temporal-reconstruction.md](temporal-reconstruction.md) - why temporal SR beats single-frame scaling and where it fails.
- [borrowable-ideas.md](borrowable-ideas.md) - ideas oSR can use legally and technically.
- [research-backlog.md](research-backlog.md) - experiments that should move quality beyond a normal upscaler.

## Current Thesis

oSR is not trying to clone DLSS, XeSS, or FSR. The credible breakthrough target for Radeon 760M-class hardware is a low-cost temporal evidence system:

```text
validate renderer inputs -> score temporal history -> spend work only on risky tiles -> expose every decision in captures
```

Modern ML SR appears to win because it selects better temporal/spatial context. oSR should borrow that principle, not the heavy inference path. The near-term architecture is deterministic, shader-friendly, and observable: trust fields, feature locks, YCoCg/luma variance clipping, reactive/disocclusion suppression, bounded residual search, and confidence-gated sharpening.

## Research Rules

- Prefer primary sources: vendor SDK docs, public headers, official blogs, papers, and direct local code evidence.
- Do not copy proprietary shaders, weights, or reverse-engineered behavior.
- Treat OptiScaler and GPL projects as clean-room architecture references only.
- A claim that affects implementation must include a source link or a local file/code reference.
