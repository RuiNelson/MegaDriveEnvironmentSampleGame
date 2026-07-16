#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
"${ROOT_DIR}/build_megadrive.sh"
exec "${ROOT_DIR}/run_emulator.sh" "$@"
