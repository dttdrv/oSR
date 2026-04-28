@echo off
setlocal
cd /d "%~dp0\.."
set "OSR_NO_PAUSE=1"

call tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name sweep_balanced --capture-gate-thresholds profiles\capture_gate.cfg --history-weight 0.98 --reactive-penalty 0.90 --motion-rejection 2.0 --color-rejection 0.16 --depth-rejection 0.035 --history-clip-margin 0.03
if errorlevel 1 exit /b 1

call tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name sweep_conservative --capture-gate-thresholds profiles\capture_gate.cfg --history-weight 0.92 --reactive-penalty 0.95 --motion-rejection 1.25 --color-rejection 0.12 --depth-rejection 0.025 --history-clip-margin 0.02
if errorlevel 1 exit /b 1

call tools\run_dx12_wind_tunnel.bat --headless --reconstruction temporal-gpu --frames 16 --capture-frame 12 --capture-run-name sweep_detail --capture-gate-thresholds profiles\capture_gate.cfg --history-weight 0.99 --reactive-penalty 0.85 --motion-rejection 2.5 --color-rejection 0.18 --depth-rejection 0.04 --history-clip-margin 0.04
if errorlevel 1 exit /b 1

call tools\run_capture_compare.bat build\manual\captures\sweep_balanced build\manual\captures\sweep_conservative build\manual\captures\sweep_detail
