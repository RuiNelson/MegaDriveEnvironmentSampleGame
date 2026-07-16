#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "${ROOT_DIR}/scripts/common.sh"

BUILD_DIR="$(sample_resolve_path "${BUILD_DIR:-build}")"
BUILD_DIR="${BUILD_DIR}" "${ROOT_DIR}/build_pc.sh" "$@"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "All sample-game checks passed."
