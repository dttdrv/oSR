@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
set "OUT=build\manual\osr_dx12_wind_tunnel.exe"
if not exist build\manual mkdir build\manual
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/demo/dx12_wind_tunnel/main.cpp src/demo/dx12_wind_tunnel/display_upscale.cpp src/demo/dx12_wind_tunnel/dx12_texture_io.cpp src/demo/dx12_wind_tunnel/presenter.cpp src/demo/wind_tunnel/synthetic_frame.cpp src/core/frame_context.cpp src/core/quality_mode.cpp src/core/logging.cpp src/debug/capture_pack.cpp src/debug/validation.cpp src/backends/dx12/dx12_backend.cpp src/backends/dx12/debug_upscale_pass.cpp src/backends/dx12/descriptor_helpers.cpp -ld3d12 -ldxgi -ldxguid -luser32 -o "%OUT%"
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)
"%OUT%" %*
echo.
pause
