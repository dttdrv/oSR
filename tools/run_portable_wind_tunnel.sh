#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

cmake --preset linux-core
cmake --build --preset linux-core --target osr_portable_wind_tunnel

exe="build/linux-core/osr_portable_wind_tunnel"
if [[ -x "${exe}.exe" ]]; then
  exe="${exe}.exe"
fi

if [[ "$#" -gt 0 ]]; then
  "$exe" "$@"
else
  "$exe" \
    --frames 16 \
    --display-size 1280x800 \
    --quality quality \
    --capture-run-name portable_manual \
    --capture-frame 12 \
    --capture-root build/manual/captures \
    --overwrite \
    --metric-gate
fi
