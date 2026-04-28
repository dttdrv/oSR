@echo off
setlocal
cd /d "%~dp0\.."

set "NMS_BIN=%~1"
if "%NMS_BIN%"=="" set "NMS_BIN=C:\Program Files (x86)\Steam\steamapps\common\No Man's Sky\Binaries"

if not exist "%NMS_BIN%\NMS.exe" (
  echo No Man's Sky NMS.exe not found at "%NMS_BIN%".
  exit /b 2
)

call tools\run_xess_proxy_smoke.bat
if errorlevel 1 exit /b 1

if not exist "%NMS_BIN%\libxess_real.dll" (
  if not exist "%NMS_BIN%\libxess.dll" (
    echo Original libxess.dll not found at "%NMS_BIN%".
    exit /b 3
  )
  copy /Y "%NMS_BIN%\libxess.dll" "%NMS_BIN%\libxess_real.dll" >nul
  if errorlevel 1 exit /b 4
)

copy /Y "build\manual\xess_proxy\libxess.dll" "%NMS_BIN%\libxess.dll" >nul
if errorlevel 1 exit /b 5

echo Installed oSR XeSS proxy into "%NMS_BIN%".
echo Original XeSS runtime is "%NMS_BIN%\libxess_real.dll".
echo Launch No Man's Sky with XeSS enabled, then inspect "%NMS_BIN%\osr_logs\osr_xess_proxy.log".
