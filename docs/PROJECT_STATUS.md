# Project Status

Last updated: 2026-05-02

oSR is a harness-first temporal super-resolution prototype. It is not a finished
game replacement layer yet. The current useful product is the controlled harness
and capture-analysis loop.

## Version And Phase

- Repository status: `v0.2.0`
- Roadmap phase: `phase_4` with early `phase_5` diagnostics and rejection gates
- Release stance: not `v1.0`
- `v1.0` meaning: a real game frame reaches an oSR-owned reconstruction path
  instead of pass-through forwarding, with validation and restore tooling.

## What Works

- Portable core data model: `FrameContext`, resources, quality modes, config,
  logging, validation.
- Synthetic SR frame generator with color, depth, motion vectors, reactive mask,
  exposure placeholder, jitter, reset state, readable text, thin rails,
  particles, transparent regions, and specular stress regions.
- CPU temporal resolve with conservative history trust, depth/color residual
  rejection, YCoCg history clipping, reactive suppression, and confidence-gated
  sharpening.
- DX12 temporal GPU proof path in the Windows wind tunnel.
- Portable CPU wind tunnel for Linux-safe capture packs and metric gates.
- Capture packs with metadata, CSVs, raw buffers, PPM/PGM views, temporal debug
  maps, SR readiness, and analysis JSON.
- Capture analyzer with ROI splits for text, static text, moving text, specular,
  transparent, reactive, motion, and static regions.
- XeSS-style quality ladder:
  - Native: `1.0`
  - Ultra Quality Plus: `1 / 1.3`
  - Ultra Quality: `1 / 1.5`
  - Quality: `1 / 1.7`
  - Balanced: `1 / 2.0`
  - Performance: `1 / 2.3`
  - Ultra Performance: `1 / 3.0`
- Diagnostic XeSS proxy can load in No Man's Sky, forward to the real runtime,
  and normalize Vulkan init/execute metadata into `FrameContext`.

## What Does Not Work Yet

- oSR does not replace No Man's Sky's real upscaler output.
- There is no Vulkan writer backend yet.
- There is no Linux interactive graphics harness yet.
- The FSR bridge is still DX12-shaped and Windows-only.
- The Win32 manual 3D wind tunnel is useful for eye testing but does not own the
  full temporal GPU path.
- No anti-cheat game support is planned.
- No heavy neural path is part of the core prototype.

## Current Best Test Commands

Portable Linux-safe harness:

```bash
bash tools/run_portable_wind_tunnel.sh
```

Portable test suite:

```bash
bash tools/run_linux_core_tests.sh
```

Windows DX12 temporal capture:

```powershell
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

Windows manual visual harness:

```powershell
tools\run_3d_wind_tunnel.bat
```

## Most Recent Verified Results

- `ctest --preset linux-core`: `22/22` passing.
- `tools/run_manual_tests.bat`: passing.
- `osr_portable_wind_tunnel` 1280x800 Quality metric-gated capture:
  - `sr_ready=1`
  - capture-analysis gate: `ok`
  - text/native contrast ratio: `0.93776`
  - motion history trusted: `0`
  - static history trusted: `98.8316%`
- Corrupted `flip-x` motion-vector mode is rejected by the portable metric gate.

## Current Priority

The project focus is the harness, not game injection. The next useful milestone
is a Linux-native interactive harness path, likely SDL2 plus Vulkan, while the
Windows DX12 wind tunnel remains the GPU reference.

## Reality Check

oSR is not XeSS/DLSS/FSR quality yet in real games. The credible path is to make
controlled scenes measurable, inspectable, and hard to fool, then move the
winning reconstruction ideas into real bridge paths.
