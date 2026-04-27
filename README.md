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

