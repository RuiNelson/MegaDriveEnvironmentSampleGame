#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"

cmake_args=(
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DBUILD_TESTING=ON
)

if [[ -n "${MEGADRIVE_ENVIRONMENT_DIR:-}" ]]; then
    cmake_args+=(
        -DMEGADRIVE_ENVIRONMENT_DIR="${MEGADRIVE_ENVIRONMENT_DIR}"
    )
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" "${cmake_args[@]}" "$@"
cmake --build "${BUILD_DIR}" --parallel
