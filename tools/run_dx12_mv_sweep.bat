@echo off
setlocal
cd /d "%~dp0\.."
set "OSR_NO_PAUSE=1"

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode correct
if errorlevel 1 goto fail

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode zero
if errorlevel 1 goto fail

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode flip-x
if errorlevel 1 goto fail

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode flip-y
if errorlevel 1 goto fail

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode half-scale
if errorlevel 1 goto fail

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode double-scale
if errorlevel 1 goto fail

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode jitter-contaminated
if errorlevel 1 goto fail

echo.
echo DX12 MV truth sweep passed.
exit /b 0

:fail
echo.
echo DX12 MV truth sweep failed.
exit /b 1
