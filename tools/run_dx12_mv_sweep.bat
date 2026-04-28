@echo off
setlocal
cd /d "%~dp0\.."
set "OSR_NO_PAUSE=1"

call tools\run_dx12_wind_tunnel.bat --headless --mv-mode correct --metric-gate
if errorlevel 1 goto fail

set "OSR_SKIP_BUILD=1"
call :expect_fail zero
if errorlevel 1 goto fail
call :expect_fail flip-x
if errorlevel 1 goto fail
call :expect_fail flip-y
if errorlevel 1 goto fail
call :expect_fail half-scale
if errorlevel 1 goto fail
call :expect_fail double-scale
if errorlevel 1 goto fail
call :expect_fail jitter-contaminated
if errorlevel 1 goto fail

echo.
echo DX12 MV truth sweep passed.
exit /b 0

:expect_fail
call tools\run_dx12_wind_tunnel.bat --headless --mv-mode %1 --metric-gate
if errorlevel 1 (
  echo Expected metric-gate failure observed for %1.
  exit /b 0
)
echo Expected metric-gate failure did not occur for %1.
exit /b 1

:fail
echo.
echo DX12 MV truth sweep failed.
exit /b 1
