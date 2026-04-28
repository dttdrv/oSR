@echo off
setlocal
cd /d "%~dp0\.."
set "CXX=C:\Users\deyan\AppData\Local\Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin\c++.exe"
if not exist build\manual mkdir build\manual

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/tests/quality_mode_tests.cpp src/core/quality_mode.cpp -o build\manual\osr_quality_mode_tests.exe && build\manual\osr_quality_mode_tests.exe
if errorlevel 1 goto fail

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/tests/temporal_oracle_tests.cpp src/reconstruction/temporal_oracle.cpp src/reconstruction/trust_field.cpp -o build\manual\osr_temporal_oracle_tests.exe && build\manual\osr_temporal_oracle_tests.exe
if errorlevel 1 goto fail

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/tests/tile_classifier_tests.cpp src/reconstruction/tile_classifier.cpp src/reconstruction/temporal_oracle.cpp src/reconstruction/trust_field.cpp -o build\manual\osr_tile_classifier_tests.exe && build\manual\osr_tile_classifier_tests.exe
if errorlevel 1 goto fail

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/tests/residual_search_tests.cpp src/reconstruction/residual_search.cpp -o build\manual\osr_residual_search_tests.exe && build\manual\osr_residual_search_tests.exe
if errorlevel 1 goto fail

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/tests/sharpening_tests.cpp src/reconstruction/sharpening.cpp -o build\manual\osr_sharpening_tests.exe && build\manual\osr_sharpening_tests.exe
if errorlevel 1 goto fail

"%CXX%" -IC:/Users/deyan/Projects/oSR/src -std=c++20 src/tests/wind_tunnel_synthetic_frame_tests.cpp src/demo/wind_tunnel/synthetic_frame.cpp src/core/frame_context.cpp src/core/quality_mode.cpp -o build\manual\osr_wind_tunnel_synthetic_frame_tests.exe && build\manual\osr_wind_tunnel_synthetic_frame_tests.exe
if errorlevel 1 goto fail

echo.
echo All manual tests passed.
pause
exit /b 0

:fail
echo.
echo Manual tests failed.
pause
exit /b 1
