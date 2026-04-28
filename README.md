# oSR

oSR is an early clean-room prototype for a DX12 temporal super-resolution replacement layer. V0 targets SDK/sample applications that already provide FSR/DLSS/XeSS-style temporal upscaler inputs.

Current phase: `phase_0`. See `ARCHITECTURE.md`, `ROADMAP.md`, `STATE.yaml`, and `LOG.md`.

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

```powershell
& 'C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe' -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/demo/quality_mode_demo.cpp src/core/quality_mode.cpp -o build/manual/osr_quality_mode_demo.exe
build\manual\osr_quality_mode_demo.exe
```

The quality-mode demo prints DLSS-style render sizes for a 1920x1200 output, including `Quality`, `Balanced`, `Performance`, and a custom slider value.
