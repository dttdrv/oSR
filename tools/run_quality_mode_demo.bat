@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
set "OUT=build\manual\osr_quality_mode_demo.exe"
if not exist build\manual mkdir build\manual
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/demo/quality_mode_demo.cpp src/core/quality_mode.cpp -o "%OUT%"
if errorlevel 1 (
  echo.
  echo Build failed.
  pause
  exit /b 1
)
"%OUT%"
echo.
pause

