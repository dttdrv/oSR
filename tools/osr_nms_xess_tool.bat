@echo off
setlocal EnableExtensions
cd /d "%~dp0\.."

set "CMD=%~1"
set "NMS_BIN=%~2"
if "%CMD%"=="" set "CMD=status"
if "%NMS_BIN%"=="" set "NMS_BIN=C:\Program Files (x86)\Steam\steamapps\common\No Man's Sky\Binaries"
set "PROXY=build\manual\xess_proxy\libxess.dll"
set "LOG=%NMS_BIN%\osr_logs\osr_xess_proxy.log"

if /I "%CMD%"=="help" goto help
if /I "%CMD%"=="status" goto status
if /I "%CMD%"=="build" goto build
if /I "%CMD%"=="install" goto install
if /I "%CMD%"=="restore" goto restore
if /I "%CMD%"=="log" goto log
echo Unknown command "%CMD%".
goto help

:help
echo oSR No Man's Sky XeSS proxy tool
echo.
echo Usage:
echo   tools\osr_nms_xess_tool.bat status  [NMS_Binaries_Path]
echo   tools\osr_nms_xess_tool.bat build
echo   tools\osr_nms_xess_tool.bat install [NMS_Binaries_Path]
echo   tools\osr_nms_xess_tool.bat restore [NMS_Binaries_Path]
echo   tools\osr_nms_xess_tool.bat log     [NMS_Binaries_Path]
echo.
echo Default path:
echo   %NMS_BIN%
exit /b 0

:build
call tools\run_xess_proxy_smoke.bat
exit /b %ERRORLEVEL%

:install
call tools\install_xess_proxy_nms.bat "%NMS_BIN%"
exit /b %ERRORLEVEL%

:restore
call tools\restore_xess_proxy_nms.bat "%NMS_BIN%"
exit /b %ERRORLEVEL%

:status
echo NMS binaries: "%NMS_BIN%"
if exist "%NMS_BIN%\NMS.exe" (
  echo NMS.exe: present
) else (
  echo NMS.exe: missing
)
if exist "%NMS_BIN%\libxess.dll" (
  for %%F in ("%NMS_BIN%\libxess.dll") do echo libxess.dll: present size=%%~zF
) else (
  echo libxess.dll: missing
)
if exist "%NMS_BIN%\libxess_real.dll" (
  for %%F in ("%NMS_BIN%\libxess_real.dll") do echo libxess_real.dll: present size=%%~zF
) else (
  echo libxess_real.dll: missing
)
if exist "%PROXY%" (
  for %%F in ("%PROXY%") do echo built proxy: present size=%%~zF
) else (
  echo built proxy: missing, run "tools\osr_nms_xess_tool.bat build"
)
if exist "%LOG%" (
  echo proxy log: "%LOG%"
) else (
  echo proxy log: missing, launch No Man's Sky with the proxy installed to create it
)
exit /b 0

:log
if not exist "%LOG%" (
  echo Proxy log not found at "%LOG%".
  exit /b 2
)
set "OSR_NMS_LOG_PATH=%LOG%"
powershell -NoProfile -ExecutionPolicy Bypass -Command "$p = [Environment]::GetEnvironmentVariable('OSR_NMS_LOG_PATH'); Get-Content -Tail 120 -LiteralPath $p"
exit /b %ERRORLEVEL%
