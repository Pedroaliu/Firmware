#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_PATH="${JIXIA_M00_08_02_BUILD_DIR:-${ROOT_DIR}/build/m00-08-02}"
readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-10}"
readonly SERIAL_LOG="${BUILD_PATH}/m00-08-02.serial.log"
readonly LOG_FILE="${BUILD_PATH}/m00-08-02.log"
readonly QEMU_ERROR_LOG="${BUILD_PATH}/m00-08-02.qemu.log"

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
    -DJIXIA_M00_08_02_PROBE=ON

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
    echo "M00-08.02: unexpected QEMU status ${QEMU_STATUS}" >&2
    exit 1
fi

for forbidden_marker in \
    "M00_08_HOSTBOOT_BOOTSTRAP: FAIL" \
    "[Jixia][Microkernel][fatal trap]"
do
    if grep -Fq "${forbidden_marker}" "${LOG_FILE}"; then
        echo "M00-08.02: forbidden marker observed: ${forbidden_marker}" >&2
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
    "M00_08_TIMER_PREEMPT: PASS" \
    "M00_08_TASK_WAIT: PASS" \
    "M00_08_TASK_WAIT_BLOCK: PASS" \
    "M00_08_TASK_SLEEP: PASS" \
    "M00_08_SLEEP_WAKE: PASS" \
    "M00_08_SLEEP_RESUME: PASS" \
    "M00_08_TASK_DETACH: PASS" \
    "M00_08_IDLE_TASK: PASS" \
    "M00_08_PREEMPTIVE_SCHEDULER: PASS"
do
    if ! grep -Fxq "${required_marker}" "${LOG_FILE}"; then
        echo "M00-08.02: required marker not found: ${required_marker}" >&2
        exit 1
    fi
done

echo "M00-08.02 Hostboot-shaped preemptive scheduler: PASS"
