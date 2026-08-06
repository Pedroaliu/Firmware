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
    echo "  bash scripts/test-recoverable-trap.sh" >&2
    echo "  bash scripts/test-recoverable-trap.sh build/clion-debug" >&2
    echo "  bash scripts/test-recoverable-trap.sh clion-debug" >&2
    exit 2
}

readonly BUILD_PATH="$(resolve_build_path "${1:-}")"
readonly FIRMWARE="${BUILD_PATH}/jixia.bin"
readonly SERIAL_LOG="${BUILD_PATH}/recoverable-trap-test.serial.log"
readonly LOG_FILE="${BUILD_PATH}/recoverable-trap-test.log"
readonly QEMU_ERROR_LOG="${BUILD_PATH}/recoverable-trap-test.qemu.log"

cmake --build "${BUILD_PATH}" --target jixia.elf

if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware image not found: ${FIRMWARE}" >&2
    exit 1
fi

rm -f "${SERIAL_LOG}" "${LOG_FILE}" "${QEMU_ERROR_LOG}"

set +e
timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
    qemu-system-riscv64 \
        -machine virt \
        -cpu rv64 \
        -m 128M \
        -smp 1 \
        -bios "${FIRMWARE}" \
        -display none \
        -serial "file:${SERIAL_LOG}" \
        -monitor none \
        >"${QEMU_ERROR_LOG}" 2>&1
readonly QEMU_STATUS=$?
set -e

if [[ -f "${SERIAL_LOG}" ]]; then
    tr -d '\r' <"${SERIAL_LOG}" >"${LOG_FILE}"
else
    : >"${LOG_FILE}"
fi

cat "${LOG_FILE}"

if [[ ${QEMU_STATUS} -ne 124 ]]; then
    if [[ -s "${QEMU_ERROR_LOG}" ]]; then
        cat "${QEMU_ERROR_LOG}" >&2
    fi
    echo "Recoverable trap test: unexpected QEMU status ${QEMU_STATUS}" >&2
    exit 1
fi

if grep -Fq "[Jixia][Microkernel][fatal trap]" "${LOG_FILE}"; then
    echo "Recoverable trap test: fatal trap observed" >&2
    exit 1
fi

if ! grep -Fxq "standard   : resumed after 32-bit EBREAK" "${LOG_FILE}"; then
    echo "Recoverable trap test: standard EBREAK did not resume" >&2
    exit 1
fi

if ! grep -Fxq "compressed : resumed after 16-bit C.EBREAK" "${LOG_FILE}"; then
    echo "Recoverable trap test: C.EBREAK did not resume" >&2
    exit 1
fi

if ! grep -Fxq "RECOVERABLE_TRAP_TEST: PASS" "${LOG_FILE}"; then
    echo "Recoverable trap test: PASS marker not found" >&2
    exit 1
fi

if grep -Fq "TRAP_FRAME_TEST: FAIL" "${LOG_FILE}"; then
    echo "TrapFrame regression: FAIL" >&2
    exit 1
fi

if ! grep -Fxq "TRAP_FRAME_TEST: PASS" "${LOG_FILE}"; then
    echo "TrapFrame regression: PASS marker not found" >&2
    exit 1
fi

echo "Recoverable trap test: PASS"
