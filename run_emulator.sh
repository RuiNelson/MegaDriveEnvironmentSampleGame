#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROM="${ROM_PATH:-${ROOT_DIR}/build/megadrive/MegaDriveEnvironmentSampleGame.bin}"
EMULATOR="/Applications/Genesis Plus.app"

if [[ ! -d "${EMULATOR}" ]]; then
    echo "Genesis Plus was not found at ${EMULATOR}." >&2
    exit 1
fi

if [[ ! -f "${ROM}" ]]; then
    echo "Mega Drive ROM is not built; building it now..."
    "${ROOT_DIR}/build_megadrive.sh"
fi

exec open -a "${EMULATOR}" "${ROM}"
