#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_PATH="${JIXIA_M00_08_03_02_BUILD_DIR:-${ROOT_DIR}/build/m00-08-03-02}"
readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-10}"
readonly CLEAN_SMP1="${BUILD_PATH}/m00-08-03-02.smp1.log"
readonly CLEAN_SMP2="${BUILD_PATH}/m00-08-03-02.smp2.log"
readonly VERIFICATION_SEED="${JIXIA_VERIFICATION_SEED:-1}"
readonly VERIFICATION_TRACE_RECORDS="${JIXIA_VERIFICATION_TRACE_RECORDS:-8192}"

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
    -DJIXIA_M00_08_03_02_PROBE=ON \
    -DJIXIA_VERIFICATION=ON \
    -DJIXIA_VERIFICATION_JITTER=OFF \
    "-DJIXIA_VERIFICATION_SEED=${VERIFICATION_SEED}" \
    "-DJIXIA_VERIFICATION_TRACE_RECORDS=${VERIFICATION_TRACE_RECORDS}"

cmake --build "${BUILD_PATH}" --target jixia.elf

readonly FIRMWARE="${BUILD_PATH}/jixia-verify.bin"
if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware image not found: ${FIRMWARE}" >&2
    exit 1
fi

run_qemu() {
    local smp_count="$1"
    local clean_log="$2"
    local serial_log="${clean_log%.log}.serial.log"
    local qemu_log="${clean_log%.log}.qemu.log"

    rm -f "${serial_log}" "${qemu_log}"
    set +e
    timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
        qemu-system-riscv64 \
        -machine virt \
        -cpu rv64 \
        -m 128M \
        -smp "${smp_count}" \
        -bios "${FIRMWARE}" \
        -display none \
        -serial "file:${serial_log}" \
        -monitor none \
        >"${qemu_log}" 2>&1
    local status=$?
    set -e

    if [[ ${status} -ne 124 ]]; then
        if [[ -s "${qemu_log}" ]]; then
            cat "${qemu_log}" >&2
        fi
        echo "M00-08.03.02: unexpected QEMU status ${status} (smp=${smp_count})" >&2
        exit 1
    fi

    tr -d '\r' <"${serial_log}" >"${clean_log}"
    # Marker output before the final dump may interleave across harts. Keep the
    # complete log for the structured checker while suppressing trace volume.
    grep -v '^JIXIA_VERIFY_TRACE: ' "${clean_log}" || true
}

check_forbidden_markers() {
    local run_log="$1"
    for forbidden_marker in \
        "M00_08_HOSTBOOT_BOOTSTRAP: FAIL" \
        "Invalid task syscall" \
        "[Jixia][Microkernel][fatal trap]" \
        "M00_08_IPC_GENERATION_CEILING: FAIL"
    do
        if grep -Fq "${forbidden_marker}" "${run_log}"; then
            echo "M00-08.03.02: forbidden marker observed: ${forbidden_marker}" >&2
            exit 1
        fi
    done
}

# ---- Run 1: --smp 1 deterministic acceptance (C02/C05/C12/C13a + regressions) ----
run_qemu 1 "${CLEAN_SMP1}"
check_forbidden_markers "${CLEAN_SMP1}"

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
    "M00_08_IPC_GENERATION_CEILING: PASS" \
    "M00_08_IPC_SLOT_RETIREMENT: PASS" \
    "M00_08_IPC_C02_WOKE_DELIVERED: PASS" \
    "M00_08_IPC_C05_M1_TO_R1: PASS" \
    "M00_08_IPC_C05_M2_TO_R2: PASS" \
    "M00_08_IPC_C05_THIRD_PENDING: PASS" \
    "M00_08_IPC_C12_STALE_HANDLE: PASS" \
    "M00_08_IPC_C12_EIDRM: PASS" \
    "M00_08_IPC_C13A_PREEMPT_WHILE_BLOCKED: PASS" \
    "M00_08_IPC_C13A_RESUMED: PASS" \
    "M00_08_IPC_C19_SENDER_DONE: PASS" \
    "M00_08_IPC_C19_RECEIVER_DONE: PASS" \
    "M00_08_IPC_C19_DRAINED_EAGAIN: PASS" \
    "M00_08_IPC_CALL_REPLY_ENOSYS: PASS" \
    "M00_08_IPC_BLOCKING_RECV: PASS"
do
    if ! grep -Fxq "${required_marker}" "${CLEAN_SMP1}"; then
        echo "M00-08.03.02 (smp1): required marker not found: ${required_marker}" >&2
        exit 1
    fi
done

marker_line() {
    grep -Fxn -- "${1}: PASS" "${CLEAN_SMP1}" | head -n 1 | cut -d: -f1
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
            echo "M00-08.03.02 (smp1): order evidence missing (${previous} / ${current})" >&2
            exit 1
        fi
        if (( previous_line >= current_line )); then
            echo "M00-08.03.02 (smp1): marker order violated: ${previous} >= ${current}" >&2
            exit 1
        fi
        previous="${current}"
    done
}

# C05: after the case-C02 receiver, the third send pends (drained by init) and
# each receiver's own payload assertion lands afterwards.
check_marker_order \
    "M00_08_IPC_C02_WOKE_DELIVERED" \
    "M00_08_IPC_C05_THIRD_PENDING" \
    "M00_08_IPC_C05_M1_TO_R1" \
    "M00_08_IPC_C05_M2_TO_R2"

# C12: after teardown the old handle fails -EINVAL, and the receiver resumes
# with -EIDRM. The trace checker below proves destroy-wake preceded both.
check_marker_order \
    "M00_08_IPC_C12_STALE_HANDLE" \
    "M00_08_IPC_C12_EIDRM"

# C13a: the timer preempted the CPU-bound task while the receiver stayed
# blocked; the receiver resumes strictly afterwards.
check_marker_order \
    "M00_08_IPC_C13A_PREEMPT_WHILE_BLOCKED" \
    "M00_08_IPC_C13A_RESUMED"

# C19 stress completes fully drained before the final PASS.
drained_line="$(grep -Fxn -- 'M00_08_IPC_C19_DRAINED_EAGAIN: PASS' "${CLEAN_SMP1}" | head -n 1 | cut -d: -f1)"
final_line="$(grep -Fxn -- 'M00_08_IPC_BLOCKING_RECV: PASS' "${CLEAN_SMP1}" | head -n 1 | cut -d: -f1)"
if [[ -z "${drained_line}" || -z "${final_line}" ]] || (( drained_line >= final_line )); then
    echo "M00-08.03.02 (smp1): C19 drained -> final order violated" >&2
    exit 1
fi

# The independent checker is the concurrency oracle. It reconstructs endpoint
# waiter/message state and proves every block has exactly one wake/result/READY
# publication without exposing test-only counters through the production API.
python3 "${ROOT_DIR}/scripts/check-microkernel-trace.py" \
    --expected-harts 1 \
    --require-blocking-ipc \
    "${CLEAN_SMP1}"

# ---- Run 2: --smp 2 C19 litmus (real cross-hart block/wake evidence) ----
run_qemu 2 "${CLEAN_SMP2}"
check_forbidden_markers "${CLEAN_SMP2}"

# SMP correctness is trace-authoritative. UART marker records are deliberately
# not an oracle because production printk remains lock-free and concurrent
# writers may interleave. The checker proves waiter FIFO, exactly-once wake,
# result-before-READY publication, hart participation, and a cross-hart wake.
python3 "${ROOT_DIR}/scripts/check-microkernel-trace.py" \
    --expected-harts 2 \
    --require-blocking-ipc \
    --require-cross-hart-wake \
    "${CLEAN_SMP2}"

echo "M00-08.03.02 blocking IPC: PASS"
