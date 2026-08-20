#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_PATH="${JIXIA_M00_08_03_01_BUILD_DIR:-${ROOT_DIR}/build/m00-08-03-01}"
readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-10}"
readonly SERIAL_LOG="${BUILD_PATH}/m00-08-03-01.serial.log"
readonly LOG_FILE="${BUILD_PATH}/m00-08-03-01.log"
readonly QEMU_ERROR_LOG="${BUILD_PATH}/m00-08-03-01.qemu.log"

for command_name in \
    cmake \
    ninja \
    qemu-system-riscv64 \
    riscv64-unknown-elf-gcc \
    riscv64-unknown-elf-g++ \
    riscv64-unknown-elf-objcopy \
    riscv64-unknown-elf-objdump \
    riscv64-unknown-elf-readelf
do
    if ! command -v "${command_name}" >/dev/null 2>&1; then
        echo "Missing command: ${command_name}" >&2
        exit 2
    fi
done

cmake \
    -S "${ROOT_DIR}" \
    -B "${BUILD_PATH}" \
    -G Ninja \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_SYSTEM_NAME=Generic \
    -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
    -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
    -DCMAKE_C_COMPILER=riscv64-unknown-elf-gcc \
    -DCMAKE_CXX_COMPILER=riscv64-unknown-elf-g++ \
    -DCMAKE_ASM_COMPILER=riscv64-unknown-elf-gcc \
    -DCMAKE_OBJCOPY=riscv64-unknown-elf-objcopy \
    -DCMAKE_OBJDUMP=riscv64-unknown-elf-objdump \
    -DCMAKE_READELF=riscv64-unknown-elf-readelf \
    -DJIXIA_M00_08_03_01_PROBE=ON

cmake --build "${BUILD_PATH}" --target jixia.elf

readonly FIRMWARE="${BUILD_PATH}/jixia.bin"
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
    echo "M00-08.03.01: unexpected QEMU status ${QEMU_STATUS}" >&2
    exit 1
fi

for forbidden_marker in \
    "M00_08_HOSTBOOT_BOOTSTRAP: FAIL" \
    "Invalid task syscall" \
    "[Jixia][Microkernel][fatal trap]"
do
    if grep -Fq "${forbidden_marker}" "${LOG_FILE}"; then
        echo "M00-08.03.01: forbidden marker observed: ${forbidden_marker}" >&2
        exit 1
    fi
done

for required_marker in \
    "M00_07_CONTAINED_MEMORY: PASS" \
    "SMP_FOUNDATION_TEST: PASS" \
    "SMP_POPULATION_TEST: PASS" \
    "SMP_TIMER_TEST: PASS" \
    "KERNEL_PRINT_TEST: PASS" \
    "RECOVERABLE_TRAP_TEST: PASS" \
    "MACHINE_TIMER_TEST: PASS" \
    "M00_08_HOSTBOOT_BOOTSTRAP: PASS" \
    "M00_08_TASK_DISPATCH: PASS" \
    "M00_08_TASK_CREATE: PASS" \
    "M00_08_TASK_YIELD: PASS" \
    "M00_08_CONTEXT_SWITCH: PASS" \
    "M00_08_TASK_WAIT: PASS" \
    "M00_08_TASK_WAIT_BLOCK: PASS" \
    "M00_08_TASK_DETACH: PASS" \
    "M00_08_IDLE_TASK: PASS" \
    "M00_08_IPC_ENDPOINT_CREATE: PASS" \
    "M00_08_IPC_C01_A_SENT: PASS" \
    "M00_08_IPC_C01_B_GOT: PASS" \
    "M00_08_IPC_C01_SEND_BEFORE_RECV: PASS" \
    "M00_08_IPC_C03_GOT_1: PASS" \
    "M00_08_IPC_C03_GOT_2: PASS" \
    "M00_08_IPC_C03_GOT_3: PASS" \
    "M00_08_IPC_C03_FIFO: PASS" \
    "M00_08_IPC_C16_FULL: PASS" \
    "M00_08_IPC_C16_POP_OLDEST: PASS" \
    "M00_08_IPC_C16_RECOVER: PASS" \
    "M00_08_IPC_C14_MALFORMED: PASS" \
    "M00_08_IPC_DESTROY_NONOWNER: PASS" \
    "M00_08_IPC_ENDPOINT_DESTROY: PASS" \
    "M00_08_IPC_C14_STALE: PASS" \
    "M00_08_IPC_C15_RECYCLED_GENERATION: PASS" \
    "M00_08_IPC_C15_ISOLATION: PASS" \
    "M00_08_IPC_RESERVED_ENOSYS: PASS" \
    "M00_08_IPC_ENDPOINT_ENOSPC: PASS" \
    "M00_08_IPC_NONBLOCKING: PASS"
do
    if ! grep -Fxq "${required_marker}" "${LOG_FILE}"; then
        echo "M00-08.03.01: required marker not found: ${required_marker}" >&2
        exit 1
    fi
done

# Ordered-marker assertions: cross-case happened-before evidence.
marker_line() {
    grep -Fxn -- "${1}: PASS" "${LOG_FILE}" | head -n 1 | cut -d: -f1
}

check_marker_order() {
    local previous="$1"
    shift
    for current in "$@"; do
        local previous_line
        local current_line
        previous_line="$(marker_line "${previous}")"
        current_line="$(marker_line "${current}")"
        if [[ -z "${previous_line}" || -z "${current_line}" ]]; then
            echo "M00-08.03.01: order evidence missing (${previous} / ${current})" >&2
            exit 1
        fi
        if (( previous_line >= current_line )); then
            echo "M00-08.03.01: marker order violated: ${previous} >= ${current}" >&2
            exit 1
        fi
        previous="${current}"
    done
}

# C01: the send linearization point precedes the receive-side delivery and the
# consumer's own payload/sender assertion (send-before-recv persistence).
check_marker_order \
    "M00_08_IPC_C01_A_SENT" \
    "M00_08_IPC_C01_B_GOT" \
    "M00_08_IPC_C01_SEND_BEFORE_RECV"

# C03: FIFO pops surface in send order 1, 2, 3 before the child's summary.
check_marker_order \
    "M00_08_IPC_C03_GOT_1" \
    "M00_08_IPC_C03_GOT_2" \
    "M00_08_IPC_C03_GOT_3" \
    "M00_08_IPC_C03_FIFO"

# C16: queue-full rejection precedes the oldest-word drain and the recovery.
check_marker_order \
    "M00_08_IPC_C16_FULL" \
    "M00_08_IPC_C16_POP_OLDEST" \
    "M00_08_IPC_C16_RECOVER"

# C15: destroy precedes every stale-handle rejection, which precedes the
# recycled-generation recreate and the new-epoch delivery.
check_marker_order \
    "M00_08_IPC_ENDPOINT_DESTROY" \
    "M00_08_IPC_C14_STALE" \
    "M00_08_IPC_C15_RECYCLED_GENERATION" \
    "M00_08_IPC_C15_ISOLATION"

# The endpoint must exist before the first send can succeed on it.
check_marker_order \
    "M00_08_IPC_ENDPOINT_CREATE" \
    "M00_08_IPC_C01_A_SENT"

echo "M00-08.03.01 non-blocking IPC: PASS"
