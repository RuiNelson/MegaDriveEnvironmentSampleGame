#!/usr/bin/env bash

# Shared path handling for the repository entry-point scripts. Callers set
# ROOT_DIR before sourcing this file.
if [[ -z "${ROOT_DIR:-}" ]]; then
    echo "scripts/common.sh must be sourced after ROOT_DIR is set" >&2
    return 1
fi

sample_resolve_path() {
    local path="$1"
    if [[ "${path}" == /* ]]; then
        printf '%s\n' "${path}"
    else
        printf '%s/%s\n' "${ROOT_DIR}" "${path}"
    fi
}
