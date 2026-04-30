@echo off
setlocal
cd /d "%~dp0\.."

cmake --preset ninja-debug
if errorlevel 1 goto fail
cmake --build --preset ninja-debug --target osr_portable_wind_tunnel
if errorlevel 1 goto fail

if not "%~1"=="" (
  build\ninja-debug\osr_portable_wind_tunnel.exe %*
) else (
  build\ninja-debug\osr_portable_wind_tunnel.exe --frames 16 --display-size 1280x800 --quality quality --capture-run-name portable_manual --capture-frame 12 --capture-root build\manual\captures --overwrite --metric-gate
)
if errorlevel 1 goto fail
exit /b 0

:fail
echo.
echo Portable wind tunnel failed.
if not "%OSR_NO_PAUSE%"=="1" pause
exit /b 1
