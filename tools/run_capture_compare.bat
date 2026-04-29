@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
if not exist build\manual mkdir build\manual
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/tools/capture_compare.cpp src/debug/capture_compare.cpp src/debug/capture_pack.cpp src/debug/frame_context_readiness.cpp src/core/frame_context.cpp -o build\manual\osr_capture_compare.exe
if errorlevel 1 exit /b 1
build\manual\osr_capture_compare.exe %*
