#!/usr/bin/env bash
set -euo pipefail

# Clean, configure, and build in Debug mode.
# Place this script next to your CMakeLists.txt.

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${PROJECT_ROOT}/build"

# Determine parallelism
if command -v nproc >/dev/null 2>&1; then
  JOBS="$(nproc)"
elif command -v getconf >/dev/null 2>&1; then
  JOBS="$(getconf _NPROCESSORS_ONLN || echo 4)"
else
  JOBS=4
fi

echo ">> Removing ${BUILD_DIR}"
rm -rf "${BUILD_DIR}"

echo ">> Configuring (Debug)"
cmake -S "${PROJECT_ROOT}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Debug

echo ">> Building (Debug, -j ${JOBS})"
cmake --build "${BUILD_DIR}" --config Debug -j "${JOBS}"

BIN="${BUILD_DIR}/torus"
if [[ -x "${BIN}" ]]; then
  echo ">> Build complete."
  echo "   Run: ${BIN}"
else
  echo ">> Build complete (binary may have a generator-specific path)."
fi
