@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
if not exist build\manual mkdir build\manual
set "OUT=build\manual\osr_capture_analyzer.exe"
"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/tools/capture_analyzer.cpp src/debug/capture_analysis.cpp src/core/frame_context.cpp -o "%OUT%"
if errorlevel 1 goto fail
"%OUT%" %*
exit /b %errorlevel%

:fail
echo.
echo Capture analyzer build failed.
exit /b 1
