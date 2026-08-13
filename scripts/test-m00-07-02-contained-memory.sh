#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_PATH="${JIXIA_M00_07_02_BUILD_DIR:-${ROOT_DIR}/build/m00-07-02}"
readonly LOG_FILE="${BUILD_PATH}/m00-07-01.log"

JIXIA_M00_07_01_BUILD_DIR="${BUILD_PATH}" \
    bash "${ROOT_DIR}/scripts/test-m00-07-01-pflash-stage0.sh"

for required_marker in \
    "M00_07_PFLASH_STAGE0: PASS" \
    "M00_07_BASE_TRANSFER: PASS" \
    "M00_07_CONTAINED_MEMORY: PASS"
do
    if ! grep -Fxq "${required_marker}" "${LOG_FILE}"; then
        echo "M00-07.02: required marker not found: ${required_marker}" >&2
        exit 1
    fi
done

if grep -Fq "M00_07_CONTAINED_MEMORY: FAIL" "${LOG_FILE}"; then
    echo "M00-07.02: contained-memory invariant failure observed" >&2
    exit 1
fi

echo "M00-07.02 explicit contained EarlyMemory state: PASS"
