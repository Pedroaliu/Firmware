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
    -DJIXIA_M00_08_03_02_PROBE=ON

cmake --build "${BUILD_PATH}" --target jixia.elf

readonly FIRMWARE="${BUILD_PATH}/jixia.bin"
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
    cat "${clean_log}"
}

for forbidden_marker in \
    "M00_08_HOSTBOOT_BOOTSTRAP: FAIL" \
    "Invalid task syscall" \
    "[Jixia][Microkernel][fatal trap]" \
    "M00_08_IPC_GENERATION_CEILING: FAIL"
do
    for run_log in "${CLEAN_SMP1}" "${CLEAN_SMP2}"; do
        if [[ ! -f "${run_log}" ]]; then
            continue
        fi
        if grep -Fq "${forbidden_marker}" "${run_log}"; then
            echo "M00-08.03.02: forbidden marker observed: ${forbidden_marker}" >&2
            exit 1
        fi
    done
done

# ---- Run 1: --smp 1 deterministic acceptance (C02/C05/C12/C13a + regressions) ----
run_qemu 1 "${CLEAN_SMP1}"

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
    "M00_08_IPC_C02_RECV_BLOCKED: PASS" \
    "M00_08_IPC_C02_SEND_WOKE: PASS" \
    "M00_08_IPC_C02_WOKE_DELIVERED: PASS" \
    "M00_08_IPC_C05_M1_TO_R1: PASS" \
    "M00_08_IPC_C05_M2_TO_R2: PASS" \
    "M00_08_IPC_C05_THIRD_PENDING: PASS" \
    "M00_08_IPC_C12_DESTROY_WOKE: PASS" \
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

# C02: the block linearization point precedes the wake, then the receiver's
# own full-register assertion (its END marker) proves the delivered payload.
check_marker_order \
    "M00_08_IPC_C02_RECV_BLOCKED" \
    "M00_08_IPC_C02_SEND_WOKE" \
    "M00_08_IPC_C02_WOKE_DELIVERED"

# C05: after the case-C02 receiver, the third send pends (drained by init) and
# each receiver's own payload assertion lands afterwards.
check_marker_order \
    "M00_08_IPC_C02_WOKE_DELIVERED" \
    "M00_08_IPC_C05_THIRD_PENDING" \
    "M00_08_IPC_C05_M1_TO_R1" \
    "M00_08_IPC_C05_M2_TO_R2"

# C12: destroy wakes the blocked receiver, the old handle then fails -EINVAL,
# and the receiver resumes with -EIDRM.
check_marker_order \
    "M00_08_IPC_C12_DESTROY_WOKE" \
    "M00_08_IPC_C12_STALE_HANDLE" \
    "M00_08_IPC_C12_EIDRM"

# C13a: the timer preempted the CPU-bound task while the receiver stayed
# blocked; the receiver resumes strictly afterwards.
check_marker_order \
    "M00_08_IPC_C13A_PREEMPT_WHILE_BLOCKED" \
    "M00_08_IPC_C13A_RESUMED"

# C19 stress completes fully drained before the summary, which precedes the
# final PASS. The summary line is numeric evidence, not a ": PASS" marker.
drained_line="$(grep -Fxn -- 'M00_08_IPC_C19_DRAINED_EAGAIN: PASS' "${CLEAN_SMP1}" | head -n 1 | cut -d: -f1)"
summary_line="$(grep -Fn 'M00_08_IPC_C19_SUMMARY:' "${CLEAN_SMP1}" | head -n 1 | cut -d: -f1)"
final_line="$(grep -Fxn -- 'M00_08_IPC_BLOCKING_RECV: PASS' "${CLEAN_SMP1}" | head -n 1 | cut -d: -f1)"
if [[ -z "${drained_line}" || -z "${summary_line}" || -z "${final_line}" ]] ||
    (( drained_line >= summary_line || summary_line >= final_line )); then
    echo "M00-08.03.02 (smp1): C19 drained -> summary -> final order violated" >&2
    exit 1
fi

check_wake_accounting() {
    local run_log="$1"
    local summary
    summary="$(grep -F 'M00_08_IPC_C19_SUMMARY:' "${run_log}" | head -n 1)"
    if [[ -z "${summary}" ]]; then
        echo "M00-08.03.02: C19 summary line missing in ${run_log}" >&2
        exit 1
    fi

    local blocks send_wakes destroy_wakes
    blocks="$(sed -E 's/.*blocks=([0-9]+).*/\1/' <<<"${summary}")"
    send_wakes="$(sed -E 's/.*send_wakes=([0-9]+).*/\1/' <<<"${summary}")"
    destroy_wakes="$(sed -E 's/.*destroy_wakes=([0-9]+).*/\1/' <<<"${summary}")"

    if (( blocks != send_wakes + destroy_wakes )); then
        echo "M00-08.03.02: wake accounting broken: ${summary}" >&2
        exit 1
    fi
}

# Exactly-once: every block is resolved by exactly one send or destroy wake.
check_wake_accounting "${CLEAN_SMP1}"

# ---- Run 2: --smp 2 C19 litmus (real cross-hart block/wake evidence) ----
run_qemu 2 "${CLEAN_SMP2}"

for required_marker in \
    "M00_08_HOSTBOOT_BOOTSTRAP: PASS" \
    "M00_08_TASK_DISPATCH: PASS" \
    "M00_08_IPC_C19_SENDER_DONE: PASS" \
    "M00_08_IPC_C19_RECEIVER_DONE: PASS" \
    "M00_08_IPC_C19_DRAINED_EAGAIN: PASS" \
    "M00_08_IPC_CALL_REPLY_ENOSYS: PASS" \
    "M00_08_IPC_BLOCKING_RECV: PASS"
do
    if ! grep -Fxq "${required_marker}" "${CLEAN_SMP2}"; then
        echo "M00-08.03.02 (smp2): required marker not found: ${required_marker}" >&2
        exit 1
    fi
done

check_wake_accounting "${CLEAN_SMP2}"

# Hart participation: blocking recv and waking send evidence must come from
# both harts across the stress run.
block_harts="$(
    grep -oE 'M00_08_IPC_RECV_BLOCK_EVIDENCE: tid=[0-9]+ hart=[0-9]+' "${CLEAN_SMP2}" |
        sed -E 's/.*hart=([0-9]+)/\1/' | sort -u | tr '\n' ' '
)"
wake_harts="$(
    grep -oE 'M00_08_IPC_SEND_WAKE_EVIDENCE: tid=[0-9]+ hart=[0-9]+' "${CLEAN_SMP2}" |
        sed -E 's/.*hart=([0-9]+)/\1/' | sort -u | tr '\n' ' '
)"
for hart_id in 0 1; do
    if [[ "${block_harts} ${wake_harts}" != *"${hart_id}"* ]]; then
        echo "M00-08.03.02 (smp2): hart ${hart_id} produced no block/wake evidence" >&2
        exit 1
    fi
done
echo "M00-08.03.02 (smp2): block harts [${block_harts}] wake harts [${wake_harts}]"

# Cross-hart wake: at least one task must have blocked on one hart and been
# woken from the other hart.
grep -oE 'M00_08_IPC_RECV_BLOCK_EVIDENCE: tid=[0-9]+ hart=[0-9]+' "${CLEAN_SMP2}" |
    sed -E 's/.*tid=([0-9]+) hart=([0-9]+)/\1 \2/' | sort -u >"${BUILD_PATH}/smp2-blocks.txt"
grep -oE 'M00_08_IPC_SEND_WAKE_EVIDENCE: tid=[0-9]+ hart=[0-9]+' "${CLEAN_SMP2}" |
    sed -E 's/.*tid=([0-9]+) hart=([0-9]+)/\1 \2/' | sort -u >"${BUILD_PATH}/smp2-wakes.txt"
cross_hart_wakes="$(
    join "${BUILD_PATH}/smp2-blocks.txt" "${BUILD_PATH}/smp2-wakes.txt" |
        awk '$2 != $3 { print $1 }' | sort -u | wc -l
)"
if (( cross_hart_wakes < 1 )); then
    echo "M00-08.03.02 (smp2): no cross-hart block/wake pair observed" >&2
    exit 1
fi
echo "M00-08.03.02 (smp2): cross-hart woken tasks: ${cross_hart_wakes}"

echo "M00-08.03.02 blocking IPC: PASS"
