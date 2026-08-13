#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_PATH="${JIXIA_M00_07_03_BUILD_DIR:-${ROOT_DIR}/build/m00-07-03}"
readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-5}"
readonly SERIAL_LOG="${BUILD_PATH}/m00-07-03.serial.log"
readonly LOG_FILE="${BUILD_PATH}/m00-07-03.log"
readonly QEMU_ERROR_LOG="${BUILD_PATH}/m00-07-03.qemu.log"
readonly IMAGE_BUILD_LOG="${BUILD_PATH}/m00-07-03.image.log"
readonly PFLASH_IMAGE="${BUILD_PATH}/jixia.pflash"

for command_name in \
    cmake \
    ninja \
    python3 \
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
    -DJIXIA_M00_07_03_PROBE=ON

cmake --build "${BUILD_PATH}" --target \
    jixia.elf \
    jixia_stage0.elf \
    jixia_pageable_probe.elf

python3 "${ROOT_DIR}/scripts/build-m00-07-pflash.py" \
    --stage0 "${BUILD_PATH}/jixia_stage0.bin" \
    --base "${BUILD_PATH}/jixia.bin" \
    --extended "${BUILD_PATH}/jixia_pageable_probe.bin" \
    --layout-header "${ROOT_DIR}/boot/qemu_virt/pflash_layout.h" \
    --output "${PFLASH_IMAGE}" \
    | tee "${IMAGE_BUILD_LOG}"

readonly PFLASH_SIZE="$(stat -c '%s' "${PFLASH_IMAGE}")"
if [[ "${PFLASH_SIZE}" != "33554432" ]]; then
    echo "M00-07.03: pflash image must be exactly 32 MiB, got ${PFLASH_SIZE}" >&2
    exit 1
fi

if ! grep -Fq "extended_size=4096" "${IMAGE_BUILD_LOG}"; then
    echo "M00-07.03: expected exactly one 4 KiB Extended page" >&2
    exit 1
fi

rm -f "${SERIAL_LOG}" "${LOG_FILE}" "${QEMU_ERROR_LOG}"

set +e
timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
    qemu-system-riscv64 \
        -machine virt,pflash0=pflash0 \
        -cpu rv64 \
        -m 128M \
        -smp 1 \
        -bios none \
        -blockdev "node-name=pflash0,driver=file,read-only=on,filename=${PFLASH_IMAGE}" \
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
    echo "M00-07.03: unexpected QEMU status ${QEMU_STATUS}" >&2
    exit 1
fi

for forbidden_marker in \
    "M00_07_PFLASH_HEADER: FAIL" \
    "M00_07_CONTAINED_MEMORY: FAIL" \
    "M00_07_PRE_DDR_PAGING: FAIL" \
    "[Jixia][Microkernel][fatal trap]"
do
    if grep -Fq "${forbidden_marker}" "${LOG_FILE}"; then
        echo "M00-07.03: forbidden marker observed: ${forbidden_marker}" >&2
        exit 1
    fi
done

for required_marker in \
    "M00_07_PFLASH_STAGE0: PASS" \
    "M00_07_BASE_TRANSFER: PASS" \
    "M00_07_CONTAINED_MEMORY: PASS" \
    "SMP_FOUNDATION_TEST: PASS" \
    "SMP_POPULATION_TEST: PASS" \
    "SMP_TIMER_TEST: PASS" \
    "KERNEL_PRINT_TEST: PASS" \
    "RECOVERABLE_TRAP_TEST: PASS" \
    "MACHINE_TIMER_TEST: PASS" \
    "M00_07_PRE_DDR_PAGING_ARMED: PASS" \
    "M00_07_PRE_DDR_PAGE_FAULT: PASS" \
    "M00_07_PRE_DDR_FLASH_READ: PASS" \
    "M00_07_PRE_DDR_BACKING_EARLY: PASS" \
    "M00_07_PRE_DDR_PAGING_RESUME: PASS" \
    "M00-07.03 pre-DDR flash-backed paging: PASS"
do
    if ! grep -Fxq "${required_marker}" "${LOG_FILE}"; then
        echo "M00-07.03: required marker not found: ${required_marker}" >&2
        exit 1
    fi
done

echo "M00-07.03 resident pager + pflash + EarlyMemory: PASS"
