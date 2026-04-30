#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")/.."

build_dir="build/linux-core"
generator_args=()
if command -v ninja >/dev/null 2>&1; then
  generator_args=(-G Ninja)
fi

cmake -S . -B "$build_dir" "${generator_args[@]}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DOSR_BUILD_TESTS=ON \
  -DOSR_BUILD_DX12=OFF \
  -DOSR_BUILD_WIN32_HARNESS=OFF \
  -DOSR_BUILD_XESS_PROXY=OFF

cmake --build "$build_dir"
ctest --test-dir "$build_dir" --output-on-failure
