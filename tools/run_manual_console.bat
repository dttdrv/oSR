@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
set "OUT=build\manual\osr_manual_console.exe"
if not exist build\manual mkdir build\manual
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/demo/manual_console.cpp src/core/quality_mode.cpp src/reconstruction/temporal_oracle.cpp src/reconstruction/trust_field.cpp src/reconstruction/tile_classifier.cpp -o "%OUT%"
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)
"%OUT%"
echo.
pause

