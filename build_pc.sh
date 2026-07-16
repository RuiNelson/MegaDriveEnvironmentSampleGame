#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${ROOT_DIR}/scripts/common.sh"

BUILD_DIR="$(sample_resolve_path "${BUILD_DIR:-build}")"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
MEGADRIVE_ENVIRONMENT_DIR="$(sample_resolve_path "${MEGADRIVE_ENVIRONMENT_DIR:-../MegaDriveEnvironment}")"

cmake_args=(
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DBUILD_TESTING=ON
    -DMEGADRIVE_ENVIRONMENT_DIR="${MEGADRIVE_ENVIRONMENT_DIR}"
)

if [[ -n "${PYTHON3:-}" ]]; then
    cmake_args+=("-DPython3_EXECUTABLE=${PYTHON3}")
fi

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" "${cmake_args[@]}" "$@"
cmake --build "${BUILD_DIR}" --parallel
