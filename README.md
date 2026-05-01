# oSR

oSR is an early clean-room prototype for a harness-first temporal super-resolution replacement layer. The current repository version is `v0.2.0`: the lab SR path exists, the XeSS-style quality ladder is implemented, and the No Man's Sky XeSS proxy can load, forward to the real runtime, and decode Vulkan init/execute metadata into oSR's normalized `FrameContext` contract.

Current phase: `phase_4`. This is not a `v1.0` release yet; `v1.0` should mean a real game frame reaches oSR-owned reconstruction instead of pass-through forwarding. See `ARCHITECTURE.md`, `ROADMAP.md`, `STATE.yaml`, and `LOG.md`.

Harness plan: see `HARNESS.md` for the dual-mode eye-test plus logging/capture design.

## Documentation

- `docs/README.md`: documentation index.
- `docs/PROJECT_STATUS.md`: current capability, limits, and latest verified results.
- `docs/HARNESS_USER_GUIDE.md`: how to run manual, portable, and DX12 harnesses.
- `docs/CAPTURE_PACKS.md`: capture-pack layout and metric interpretation.
- `docs/LINUX.md`: elementaryOS/Linux development path.
- `docs/DEVELOPMENT.md`: build targets, verification, and engineering rules.
- `docs/wiki/README.md`: source-backed SR research wiki.

## Build

Portable Linux/core path:

```bash
bash tools/run_linux_core_tests.sh
```

Portable CPU wind-tunnel harness with capture pack and metric gate:

```bash
bash tools/run_portable_wind_tunnel.sh
```

Or manually:

```bash
cmake --preset linux-core
cmake --build --preset linux-core
ctest --preset linux-core
```

See `docs/LINUX.md` for elementaryOS notes and the current Linux/Windows target split.

Windows DX12 path:

```powershell
cmake --preset ninja-debug
cmake --build --preset ninja-debug
ctest --preset ninja-debug
```

On a Visual Studio 2022 environment:

```powershell
cmake --preset vs2022-debug
cmake --build --preset vs2022-debug
```

## Manual Smoke Tests

Current sandbox note: if CMake/Ninja stalls, the same tests can be compiled directly with the MinGW compiler used in this workspace.

For double-click testing in Explorer, use:

```text
tools\run_manual_console.bat
```

That launcher builds `build\manual\osr_manual_console.exe`, opens an interactive menu, and pauses before closing. The menu lets you:

- list XeSS-style quality modes
- enter a custom render-scale slider value
- change display resolution
- run trust oracle scenarios
- run tile risk scenarios

For a quick non-interactive quality-mode printout:

```text
tools\run_quality_mode_demo.bat
```

No Man's Sky XeSS proxy helper:

```bat
tools\osr_nms_xess_tool.bat status
tools\osr_nms_xess_tool.bat build
tools\osr_nms_xess_tool.bat install
tools\osr_nms_xess_tool.bat log
tools\osr_nms_xess_tool.bat restore
```

The proxy currently installs as `libxess.dll`, preserves the original Intel runtime as `libxess_real.dll`, and logs to the game's `Binaries\osr_logs\osr_xess_proxy.log`. It is still an opt-in diagnostic bridge: it observes and normalizes the XeSS boundary, then forwards to the real XeSS runtime.

Proxy execution mode is controlled with `OSR_XESS_MODE`:

- unset / `passthrough`: decode and forward to the real XeSS runtime
- `observe`: explicit diagnostic pass-through
- `osr`: experimental takeover policy; currently logs whether oSR replacement would be allowed, then forwards unless a safe Vulkan writer is available

`OSR_XESS_MODE=osr` is not a finished in-game replacement yet. It is the guarded path that prevents us from silently guessing motion-vector scale or writing an output texture before the Vulkan backend exists.

For a visual 3D edge and render-scale check:

```text
tools\run_3d_wind_tunnel.bat
```

This builds and launches a native `.exe` with a simple 3D scene, free camera
movement, a right-side settings panel, render-scale presets, debug views, jitter
controls, and crisp overlay diagnostics. Controls:

- Move: `W/A/S/D`, `Q/E`
- Look: arrow keys or hold left mouse and drag
- Presets: `1` native, `2` ultra quality, `3` quality, `4` balanced, `5` performance, `6` ultra performance
- Custom scale: `-` / `=`
- Debug/settings: `V` view mode, `J` jitter, `K` jitter length, `L` linear/nearest upscale, `F` freeze, `P` particles, `T` rails, `R` reset, `H` overlay

The same core settings are also exposed as native controls in the panel: quality
preset, render-scale slider, debug view, animation, jitter, freeze, upsample
filter, rails, particles, and reset.

The older standalone WebGL sketch remains available:

```text
tools\run_3d_scene.bat
```

The native executable is now the primary manual visual test; the WebGL page is
kept as a quick browser fallback while the DX12 harness is being wired.

For the first DX12 harness proof of life:

```text
tools\run_dx12_wind_tunnel.bat
```

This creates real D3D12 resources for synthetic color, output, depth, motion
vectors, and reactive mask buffers, exports them through `FrameContext`, calls
the current DX12 debug-upscale path, and writes readable metadata to
`build\manual\osr_dx12_wind_tunnel_metadata.txt`. It also writes a capture pack
under `build\manual\captures\...` with `session.json`, `frames.csv`,
`metrics.csv`, `warnings.jsonl`, `bookmarks.jsonl`, and
`frame_000001\frame_context.json`. Frame folders also include no-dependency
debug artifacts such as `color_input.ppm`, `color_output.ppm`, `depth.pgm`,
`motion_vectors_magnitude.pgm`, signed `motion_vectors_x.pgm` /
`motion_vectors_y.pgm`, `reactive_mask.pgm`, exact `.raw` buffers, and
`artifacts.json`. Temporal captures also include raw R32F history/residual maps.

To summarize a captured frame without rerunning DX12:

```text
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012 --gate
tools\run_capture_analyzer.bat build\manual\captures\<session>\frame_000012 --gate --thresholds profiles\capture_gate.cfg
```

The analyzer reports global temporal trust plus motion/static and synthetic ROI
splits for text, specular, transparent, and reactive regions.

By default it opens a DX12 window and presents the current display-sized color
debug output. For deterministic lab runs:

```text
tools\run_dx12_wind_tunnel.bat --headless
tools\run_dx12_wind_tunnel.bat --present-frames 3
```

Temporal experiments can be run without recompiling:

```text
tools\run_dx12_wind_tunnel.bat --headless --frames 32 --metric-gate --history-weight 0.98 --motion-rejection 2 --color-rejection 0.16 --depth-rejection 0.035 --history-clip-margin 0.04 --sharpening 0.28
tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --metric-gate --capture-gate-thresholds profiles\capture_gate.cfg
```

To build and run the current manual test suite:

```text
tools\run_manual_tests.bat
```

For the cross-platform CPU harness on Windows:

```text
tools\run_portable_wind_tunnel.bat
```

The portable harness writes sequence metrics to
`build/manual/osr_portable_wind_tunnel_metrics.csv` and, by default, a capture
pack to `build/manual/captures/portable_manual`. It is the Linux-safe path for
capture analysis and metric gates while the DX12 wind tunnel remains the Windows
GPU reference.

```powershell
& 'C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe' -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/demo/quality_mode_demo.cpp src/core/quality_mode.cpp -o build/manual/osr_quality_mode_demo.exe
build\manual\osr_quality_mode_demo.exe
```

The quality-mode demo prints XeSS-style render sizes for a 1920x1200 output, including `Native`, `UltraQualityPlus`, `UltraQuality`, `Quality`, `Balanced`, `Performance`, `UltraPerformance`, and a custom slider value.
