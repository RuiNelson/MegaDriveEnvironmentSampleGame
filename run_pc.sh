#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-${ROOT_DIR}/build}"
GAME="${BUILD_DIR}/mega_drive_environment_sample_game"

if [[ ! -x "${GAME}" ]]; then
    echo "PC game is not built. Run ${ROOT_DIR}/build_pc.sh first." >&2
    exit 1
fi

cd "${ROOT_DIR}"
exec "${GAME}" "$@"
