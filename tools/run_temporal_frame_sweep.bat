@echo off
setlocal
cd /d "%~dp0\.."
set "OSR_NO_PAUSE=1"

call tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 20 --capture-frame 10 --capture-run-name default_frame_10 --capture-gate-thresholds profiles\capture_gate.cfg
if errorlevel 1 exit /b 1

call tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 20 --capture-frame 12 --capture-run-name default_frame_12 --capture-gate-thresholds profiles\capture_gate.cfg
if errorlevel 1 exit /b 1

call tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 20 --capture-frame 14 --capture-run-name default_frame_14 --capture-gate-thresholds profiles\capture_gate.cfg
if errorlevel 1 exit /b 1

call tools\run_capture_compare.bat build\manual\captures\default_frame_10 build\manual\captures\default_frame_12 build\manual\captures\default_frame_14
