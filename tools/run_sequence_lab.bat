@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
set "OUT=build\manual\osr_sequence_lab.exe"
if not exist build\manual mkdir build\manual
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/demo/sequence_lab.cpp src/demo/wind_tunnel/sequence_metrics.cpp src/demo/wind_tunnel/temporal_resolve.cpp src/demo/wind_tunnel/synthetic_frame.cpp src/demo/wind_tunnel/synthetic_roi.cpp src/demo/dx12_wind_tunnel/display_upscale.cpp src/core/frame_context.cpp src/core/quality_mode.cpp -o "%OUT%"
if errorlevel 1 (
  echo.
  echo Build failed.
  if not "%OSR_NO_PAUSE%"=="1" pause
  exit /b 1
)
"%OUT%" %*
set "OSR_EXIT_CODE=%ERRORLEVEL%"
echo.
if not "%OSR_NO_PAUSE%"=="1" pause
exit /b %OSR_EXIT_CODE%
