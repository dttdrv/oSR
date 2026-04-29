@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
if not exist build\manual mkdir build\manual
if not exist build\manual\xess_proxy mkdir build\manual\xess_proxy

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 -shared src/interop/xess_bridge/xess_proxy.cpp src/interop/xess_bridge/xess_replacement_policy.cpp src/interop/xess_bridge/xess_vk_frame_context.cpp src/core/frame_context.cpp src/core/logging.cpp -o build\manual\xess_proxy\libxess.dll
if errorlevel 1 exit /b 1

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 -O2 src/tests/xess_proxy_smoke.cpp -o build\manual\osr_xess_proxy_smoke.exe
if errorlevel 1 exit /b 1

build\manual\osr_xess_proxy_smoke.exe build\manual\xess_proxy\libxess.dll
