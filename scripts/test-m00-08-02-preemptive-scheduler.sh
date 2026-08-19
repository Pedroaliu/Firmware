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

# Quantitative preemption evidence: the per-hart counter must have advanced.
readonly preemption_evidence="$(
    grep -E '^M00_08_PREEMPTION_COUNT: [0-9]+$' "${LOG_FILE}" | head -n 1 || true
)"
if [[ -z "${preemption_evidence}" ]]; then
    echo "M00-08.02: preemption count evidence missing" >&2
    exit 1
fi
readonly preemption_count="${preemption_evidence##*: }"
if (( preemption_count < 1 )); then
    echo "M00-08.02: scheduler_preemption_count did not advance (${preemption_count})" >&2
    exit 1
fi

# Deadline-aware idle evidence: the sleeper must wake near its own deadline
# (20000 ticks), strictly below one task timeslice. Both polling regressions
# fail this bound deterministically because the timer is re-armed at the
# scheduling point: a fixed task-slice timer wakes the sleeper at roughly
# task_slice (100000) ticks after sleep entry, a fixed idle-slice timer at
# roughly idle_slice (1000000). The bound is derived from the kernel's own
# arming constants, published as M00_08_SCHED_SLICES (no magic constant here).
readonly slice_evidence="$(
    grep -E '^M00_08_SCHED_SLICES: task=[0-9]+ idle=[0-9]+$' "${LOG_FILE}" | head -n 1 || true
)"
if [[ -z "${slice_evidence}" ]]; then
    echo "M00-08.02: scheduler slice constants evidence missing" >&2
    exit 1
fi
readonly task_timeslice_ticks="$(printf '%s' "${slice_evidence}" | sed -E 's/.*task=([0-9]+).*/\1/')"
readonly idle_timeslice_ticks="$(printf '%s' "${slice_evidence}" | sed -E 's/.*idle=([0-9]+)$/\1/')"
if (( task_timeslice_ticks == 0 || idle_timeslice_ticks == 0 ||
      task_timeslice_ticks >= idle_timeslice_ticks )); then
    echo "M00-08.02: invalid slice constants (task=${task_timeslice_ticks} idle=${idle_timeslice_ticks})" >&2
    exit 1
fi
readonly sleep_evidence="$(
    grep -E '^M00_08_SLEEP_WAKE_EVIDENCE: elapsed=[0-9]+ requested=[0-9]+$' "${LOG_FILE}" |
        head -n 1 || true
)"
if [[ -z "${sleep_evidence}" ]]; then
    echo "M00-08.02: sleep wake timing evidence missing" >&2
    exit 1
fi
readonly sleep_elapsed="$(printf '%s' "${sleep_evidence}" | sed -E 's/.*elapsed=([0-9]+).*/\1/')"
readonly sleep_requested="$(printf '%s' "${sleep_evidence}" | sed -E 's/.*requested=([0-9]+)$/\1/')"
if (( sleep_requested == 0 || sleep_requested >= task_timeslice_ticks ||
      sleep_elapsed < sleep_requested || sleep_elapsed >= task_timeslice_ticks )); then
    echo "M00-08.02: sleeper wake outside deadline-aware bounds" \
        "(elapsed=${sleep_elapsed} requested=${sleep_requested}" \
        "task_slice=${task_timeslice_ticks})" >&2
    exit 1
fi

echo "M00-08.02 Hostboot-shaped preemptive scheduler: PASS"
