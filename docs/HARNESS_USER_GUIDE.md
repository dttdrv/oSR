# Harness User Guide

The harness is the center of the project. It exists so visual artifacts can be
turned into reproducible data.

## Harnesses

## Portable Wind Tunnel

Use this on Linux, elementaryOS, CI-like runs, or any machine where DX12 is not
the right dependency.

```bash
bash tools/run_portable_wind_tunnel.sh
```

Default behavior:

- builds `osr_portable_wind_tunnel`
- runs `16` frames at `1280x800`
- uses Quality scale
- captures frame `12`
- writes `build/manual/captures/portable_manual`
- runs sequence and capture-analysis gates

Fast smoke:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 8 --display-size 320x200 --quality quality --capture-run-name portable_smoke --capture-frame 8 --overwrite
```

Important: small `320x200` captures are smoke tests, not final quality evidence.
The text-history capture gate is calibrated for normal lab sizes such as
`1280x800`.

Useful options:

```text
--frames N
--start-frame N
--display-size WIDTHxHEIGHT
--quality native|ultra-quality-plus|ultra-quality|quality|balanced|performance|ultra-performance
--render-scale FLOAT
--capture-run-name NAME
--capture-frame N
--capture-root PATH
--thresholds PATH
--metric-gate
--overwrite
--no-jitter
--mv-mode correct|zero|flip-x|flip-y|half-scale|double-scale|jitter-contaminated
--history-weight FLOAT
--motion-rejection FLOAT
--color-rejection FLOAT
--depth-rejection FLOAT
--history-clip-margin FLOAT
--sharpening FLOAT
```

Expected metric-gated baseline:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 16 --display-size 1280x800 --quality quality --capture-run-name portable_quality_gate --capture-frame 12 --overwrite --metric-gate
```

Expected failing corrupted-MV run:

```bash
bash tools/run_portable_wind_tunnel.sh --frames 16 --display-size 320x200 --quality quality --mv-mode flip-x --metric-gate
```

The corrupted run should exit with code `3` and print `Metric gate failed`.

## DX12 Wind Tunnel

Use this on Windows when GPU resource ownership and shader parity matter.

```powershell
tools\run_dx12_wind_tunnel.bat
```

Deterministic headless temporal GPU capture:

```powershell
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name dx12_temporal_manual --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

Motion-vector truth-table sweep:

```powershell
tools\run_dx12_mv_sweep.bat
```

The sweep should pass correct motion vectors and reject corrupted modes. This is
one of the most important guardrails in the project.

## Native 3D Manual Harness

Use this for quick eye testing on Windows.

```powershell
tools\run_3d_wind_tunnel.bat
```

Controls:

- Move: `W/A/S/D`, `Q/E`
- Look: arrow keys or hold left mouse and drag
- Presets: `1` through `7`
- Custom scale: `-` / `=`
- Debug view: `V`
- Jitter: `J`
- Jitter length: `K`
- Freeze: `F`
- Linear/nearest: `L`
- Particles: `P`
- Rails: `T`
- Reset: `R`
- Overlay: `H`

This harness is for human perception checks. Treat conclusions as leads, then
confirm them with a capture pack.

## Capture Analyzer

Analyze an existing capture frame:

```powershell
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012
```

Analyze and gate:

```powershell
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012 --gate --thresholds profiles\capture_gate.cfg
```

Compare captures:

```powershell
tools\run_capture_compare.bat build\manual\captures\run_a build\manual\captures\run_b
```

## Reading Console Output

Key fields:

- `Temporal/spatial delta ratio`: lower is more temporally stable. The canonical
  gate currently expects `<= 0.80`.
- `Thin/text contrast`: should stay near `1.0`. Too low means blur; too high can
  mean ringing.
- `Material leak spec/trans`: history trust in specular or transparent stress
  regions. Lower is safer.
- `sr_ready=1`: the frame has all mandatory controlled SR inputs.
- `motion_history_trusted_pct`: should be very low for moving regions.
- `static_history_trusted_pct`: should be high in stable regions.
- `text_native_contrast_ratio`: text contrast relative to native reference.

## Exit Codes

- `0`: success
- `1`: runtime or script failure
- `2`: invalid arguments or validation failure
- `3`: metric gate failure
- `4`: capture-pack write failure

## Good Manual Workflow

1. Run `tools\run_3d_wind_tunnel.bat` and look for obvious artifacts.
2. Reproduce the same class of artifact in `osr_portable_wind_tunnel` or
   `osr_dx12_wind_tunnel`.
3. Save a capture pack.
4. Run capture analyzer with gates.
5. Change one reconstruction knob.
6. Compare capture packs.
7. Promote the change only if gates and visual inspection agree.
