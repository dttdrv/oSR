@echo off
setlocal
cd /d "%~dp0\.."

set "NMS_BIN=%~1"
if "%NMS_BIN%"=="" set "NMS_BIN=C:\Program Files (x86)\Steam\steamapps\common\No Man's Sky\Binaries"

if not exist "%NMS_BIN%\libxess_real.dll" (
  echo Backup libxess_real.dll not found at "%NMS_BIN%".
  exit /b 2
)

copy /Y "%NMS_BIN%\libxess_real.dll" "%NMS_BIN%\libxess.dll" >nul
if errorlevel 1 exit /b 3

echo Restored original XeSS runtime to "%NMS_BIN%\libxess.dll".
