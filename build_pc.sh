#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
BUILD_TYPE="${BUILD_TYPE:-Debug}"
PYTHON3="${PYTHON3:-python3}"
MEGADRIVE_ENVIRONMENT_DIR="${MEGADRIVE_ENVIRONMENT_DIR:-${ROOT_DIR}/../MegaDriveEnvironment}"

if [[ "${BUILD_DIR}" != /* ]]; then
    BUILD_DIR="${ROOT_DIR}/${BUILD_DIR}"
fi

"${PYTHON3}" "${ROOT_DIR}/tools/build_assets.py" \
    --output "${BUILD_DIR}/sample_game_assets.bin" \
    --font-data "${MEGADRIVE_ENVIRONMENT_DIR}/include/MegaDriveEnvironment/util/font/FontData.hpp" \
    --z80-source "${ROOT_DIR}/z80/boing_ball_sfx.s" \
    --boing-pcm "${ROOT_DIR}/assets/boing_pcm.bin" \
    --work-dir "${BUILD_DIR}/generated/assets" \
    --layout-header "${BUILD_DIR}/generated/AssetLayout.hpp" \
    --layout-json "${BUILD_DIR}/generated/asset_layout.json" \
    --pack-binary "${BUILD_DIR}/generated/asset_pack.bin"

cmake_args=(
    -DCMAKE_BUILD_TYPE="${BUILD_TYPE}"
    -DBUILD_TESTING=ON
    -DMEGADRIVE_ENVIRONMENT_DIR="${MEGADRIVE_ENVIRONMENT_DIR}"
    -DSAMPLE_ASSET_ROM_PREBUILT=ON
)

cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" "${cmake_args[@]}" "$@"
cmake --build "${BUILD_DIR}" --parallel
