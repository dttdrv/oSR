@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
set "OUT=build\manual\osr_dx12_wind_tunnel.exe"
if not exist build\manual mkdir build\manual
if "%OSR_SKIP_BUILD%"=="1" if exist "%OUT%" goto run
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/demo/dx12_wind_tunnel/main.cpp src/demo/dx12_wind_tunnel/display_upscale.cpp src/demo/dx12_wind_tunnel/dx12_texture_io.cpp src/demo/dx12_wind_tunnel/presenter.cpp src/demo/wind_tunnel/debug_dumps.cpp src/demo/wind_tunnel/synthetic_frame.cpp src/demo/wind_tunnel/synthetic_roi.cpp src/demo/wind_tunnel/temporal_diagnostics.cpp src/demo/wind_tunnel/temporal_resolve.cpp src/demo/wind_tunnel/sequence_metrics.cpp src/reconstruction/trust_field.cpp src/core/frame_context.cpp src/core/quality_mode.cpp src/core/logging.cpp src/debug/capture_pack.cpp src/debug/validation.cpp src/backends/dx12/dx12_backend.cpp src/backends/dx12/debug_upscale_pass.cpp src/backends/dx12/temporal_resolve_pass.cpp src/backends/dx12/descriptor_helpers.cpp -ld3d12 -ldxgi -ldxguid -ld3dcompiler -luser32 -o "%OUT%"
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)
:run
"%OUT%" %*
set "OSR_EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%OSR_NO_PAUSE%"=="1" pause
exit /b %OSR_EXIT_CODE%
