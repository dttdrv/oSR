# oSR

oSR is an early clean-room prototype for a DX12 temporal super-resolution replacement layer. V0 targets SDK/sample applications that already provide FSR/DLSS/XeSS-style temporal upscaler inputs.

Current phase: `phase_0`. See `ARCHITECTURE.md`, `ROADMAP.md`, `STATE.yaml`, and `LOG.md`.

Harness plan: see `HARNESS.md` for the dual-mode eye-test plus logging/capture design.

## Build

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

- list DLSS-style quality modes
- enter a custom render-scale slider value
- change display resolution
- run trust oracle scenarios
- run tile risk scenarios

For a quick non-interactive quality-mode printout:

```text
tools\run_quality_mode_demo.bat
```

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
```

By default it opens a DX12 window and presents the current display-sized color
debug output. For deterministic lab runs:

```text
tools\run_dx12_wind_tunnel.bat --headless
tools\run_dx12_wind_tunnel.bat --present-frames 3
```

Temporal experiments can be run without recompiling:

```text
tools\run_dx12_wind_tunnel.bat --headless --frames 32 --metric-gate --history-weight 0.98 --motion-rejection 2 --color-rejection 0.16 --depth-rejection 0.035 --history-clip-margin 0.04 --sharpening 0.28
```

To build and run the current manual test suite:

```text
tools\run_manual_tests.bat
```

```powershell
& 'C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe' -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/demo/quality_mode_demo.cpp src/core/quality_mode.cpp -o build/manual/osr_quality_mode_demo.exe
build\manual\osr_quality_mode_demo.exe
```

The quality-mode demo prints DLSS-style render sizes for a 1920x1200 output, including `Quality`, `Balanced`, `Performance`, and a custom slider value.
