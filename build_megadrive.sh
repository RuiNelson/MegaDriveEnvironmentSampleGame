#!/usr/bin/env bash
set -euo pipefail

repository="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
python3="${PYTHON3:-python3}"

exec "${python3}" "${repository}/tools/build_megadrive_rom.py" "$@"
