@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
set "OUT=build\manual\osr_3d_wind_tunnel.exe"
if not exist build\manual mkdir build\manual
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/demo/manual_3d_wind_tunnel.cpp src/demo/wind_tunnel/synthetic_frame.cpp src/demo/wind_tunnel/synthetic_roi.cpp src/core/frame_context.cpp src/core/quality_mode.cpp -lgdi32 -luser32 -lcomctl32 -o "%OUT%"
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)
start "" "%OUT%"
