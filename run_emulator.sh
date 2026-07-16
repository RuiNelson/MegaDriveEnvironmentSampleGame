#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${ROOT_DIR}/scripts/common.sh"

ROM="$(sample_resolve_path "${ROM_PATH:-build/megadrive/MegaDriveEnvironmentSampleGame.bin}")"

if [[ ! -f "${ROM}" ]]; then
    if [[ -n "${ROM_PATH:-}" ]]; then
        echo "ROM_PATH does not point to an existing image: ${ROM}" >&2
        exit 1
    fi
    echo "Mega Drive ROM is not built; building it now..."
    "${ROOT_DIR}/build_megadrive.sh"
fi

if [[ ! -f "${ROM}" ]]; then
    echo "ROM was not produced at ${ROM}. Set ROM_PATH to an existing image." >&2
    exit 1
fi

if [[ -n "${EMULATOR:-}" ]]; then
    if [[ ! -x "${EMULATOR}" ]]; then
        echo "Emulator is not executable: ${EMULATOR}" >&2
        exit 1
    fi
    exec "${EMULATOR}" "${ROM}"
fi

if [[ "$(uname -s)" == "Darwin" ]]; then
    EMULATOR_APP="${EMULATOR_APP:-/Applications/Genesis Plus.app}"
    if [[ -d "${EMULATOR_APP}" ]]; then
        exec open -a "${EMULATOR_APP}" "${ROM}"
    fi
fi

echo "No emulator was found." >&2
echo "Set EMULATOR=/path/to/emulator, or EMULATOR_APP=/path/to/app on macOS." >&2
exit 1
