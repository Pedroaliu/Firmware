#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly DEFAULT_BUILD_PATH="${ROOT_DIR}/build/clion-debug"
readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-3}"

resolve_build_path()
{
    if [[ $# -eq 0 || -z "${1}" ]]; then
        printf '%s\n' "${DEFAULT_BUILD_PATH}"
        return
    fi

    local requested="${1}"
    local candidate

    if [[ "${requested}" = /* ]]; then
        candidate="${requested}"
        if [[ -f "${candidate}/CMakeCache.txt" ]]; then
            printf '%s\n' "${candidate}"
            return
        fi
    else
        for candidate in \
            "${ROOT_DIR}/${requested}" \
            "${ROOT_DIR}/build/${requested}"
        do
            if [[ -f "${candidate}/CMakeCache.txt" ]]; then
                printf '%s\n' "${candidate}"
                return
            fi
        done
    fi

    echo "Configured build directory not found: ${requested}" >&2
    echo "Expected a directory containing CMakeCache.txt." >&2
    echo "Examples:" >&2
    echo "  bash scripts/test-trap-frame.sh" >&2
    echo "  bash scripts/test-trap-frame.sh build/clion-debug" >&2
    echo "  bash scripts/test-trap-frame.sh clion-debug" >&2
    exit 2
}

readonly BUILD_PATH="$(resolve_build_path "${1:-}")"
readonly FIRMWARE="${BUILD_PATH}/jixia.bin"
readonly LOG_FILE="${BUILD_PATH}/trap-frame-test.log"

cmake --build "${BUILD_PATH}" --target jixia.elf

if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware image not found: ${FIRMWARE}" >&2
    exit 1
fi

set +e
timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
    qemu-system-riscv64 \
        -machine virt \
        -cpu rv64 \
        -m 128M \
        -smp 1 \
        -bios "${FIRMWARE}" \
        -display none \
        -serial stdio \
        -monitor none \
        >"${LOG_FILE}" 2>&1
readonly QEMU_STATUS=$?
set -e

cat "${LOG_FILE}"

if [[ ${QEMU_STATUS} -ne 124 ]]; then
    echo "TrapFrame test: unexpected QEMU status ${QEMU_STATUS}" >&2
    exit 1
fi

if grep -Fq "TRAP_FRAME_TEST: FAIL" "${LOG_FILE}"; then
    echo "TrapFrame test: FAIL" >&2
    exit 1
fi

if ! grep -Fxq "TRAP_FRAME_TEST: PASS" "${LOG_FILE}"; then
    echo "TrapFrame test: PASS marker not found" >&2
    exit 1
fi

echo "TrapFrame test: PASS"
