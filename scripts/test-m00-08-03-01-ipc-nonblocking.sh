#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_PATH="${JIXIA_M00_08_03_01_BUILD_DIR:-${ROOT_DIR}/build/m00-08-03-01}"
readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-10}"
readonly SMP_HARTS="${JIXIA_QEMU_SMP_HARTS:-1}"
readonly VERIFICATION="${JIXIA_VERIFICATION:-0}"
readonly VERIFICATION_JITTER="${JIXIA_VERIFICATION_JITTER:-0}"
readonly VERIFICATION_SEED="${JIXIA_VERIFICATION_SEED:-1}"
readonly VERIFICATION_TRACE_RECORDS="${JIXIA_VERIFICATION_TRACE_RECORDS:-1024}"
readonly TCG_THREAD_MODE="${JIXIA_QEMU_TCG_THREAD:-default}"
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

if [[ ! "${SMP_HARTS}" =~ ^[1-4]$ ]]; then
    echo "JIXIA_QEMU_SMP_HARTS must be in [1, 4]" >&2
    exit 2
fi
if [[ ! "${VERIFICATION}" =~ ^[01]$ || ! "${VERIFICATION_JITTER}" =~ ^[01]$ ]]; then
    echo "JIXIA_VERIFICATION and JIXIA_VERIFICATION_JITTER must be 0 or 1" >&2
    exit 2
fi
if [[ ! "${VERIFICATION_SEED}" =~ ^[0-9]+$ ||
      ! "${VERIFICATION_TRACE_RECORDS}" =~ ^[1-9][0-9]*$ ]]; then
    echo "invalid verification seed or trace-record count" >&2
    exit 2
fi
if [[ "${VERIFICATION_JITTER}" == "1" && "${VERIFICATION}" != "1" ]]; then
    echo "JIXIA_VERIFICATION_JITTER requires JIXIA_VERIFICATION=1" >&2
    exit 2
fi
if (( SMP_HARTS > 1 )) && [[ "${VERIFICATION}" != "1" ]]; then
    echo "SMP acceptance requires JIXIA_VERIFICATION=1; concurrent printk is not a record-atomic oracle" >&2
    exit 2
fi
if [[ "${VERIFICATION}" == "1" ]] && ! command -v python3 >/dev/null 2>&1; then
    echo "Missing command: python3" >&2
    exit 2
fi
if [[ "${TCG_THREAD_MODE}" != "default" && "${TCG_THREAD_MODE}" != "single" &&
      "${TCG_THREAD_MODE}" != "multi" ]]; then
    echo "JIXIA_QEMU_TCG_THREAD must be default, single, or multi" >&2
    exit 2
fi

verification_cmake_args=()
if [[ "${VERIFICATION}" == "1" ]]; then
    verification_cmake_args+=(
        -DJIXIA_VERIFICATION=ON
        "-DJIXIA_VERIFICATION_SEED=${VERIFICATION_SEED}"
        "-DJIXIA_VERIFICATION_TRACE_RECORDS=${VERIFICATION_TRACE_RECORDS}"
    )
    if [[ "${VERIFICATION_JITTER}" == "1" ]]; then
        verification_cmake_args+=(-DJIXIA_VERIFICATION_JITTER=ON)
    else
        verification_cmake_args+=(-DJIXIA_VERIFICATION_JITTER=OFF)
    fi
else
    verification_cmake_args+=(
        -DJIXIA_VERIFICATION=OFF
        -DJIXIA_VERIFICATION_JITTER=OFF
    )
fi

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
    -DJIXIA_M00_08_03_01_PROBE=ON \
    "${verification_cmake_args[@]}"

cmake --build "${BUILD_PATH}" --target jixia.elf

if [[ "${VERIFICATION}" == "1" ]]; then
    readonly FIRMWARE="${BUILD_PATH}/jixia-verify.bin"
else
    readonly FIRMWARE="${BUILD_PATH}/jixia.bin"
fi
if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware image not found: ${FIRMWARE}" >&2
    exit 1
fi

rm -f "${SERIAL_LOG}" "${LOG_FILE}" "${QEMU_ERROR_LOG}"

set +e
qemu_accel_args=()
if [[ "${TCG_THREAD_MODE}" != "default" ]]; then
    qemu_accel_args=(-accel "tcg,thread=${TCG_THREAD_MODE}")
fi
timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
    qemu-system-riscv64 \
        "${qemu_accel_args[@]}" \
        -machine virt \
        -cpu rv64 \
        -m 128M \
        -smp "${SMP_HARTS}" \
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

if [[ "${VERIFICATION}" == "1" ]]; then
    # Keep the machine-readable trace in LOG_FILE without flooding the terminal.
    # The one-line checker result below is the user-facing verdict.
    grep -v '^JIXIA_VERIFY_TRACE: ' "${LOG_FILE}" || true
else
    cat "${LOG_FILE}"
fi

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

for boot_marker in \
    "M00_07_CONTAINED_MEMORY: PASS" \
    "SMP_FOUNDATION_TEST: PASS" \
    "SMP_POPULATION_TEST: PASS" \
    "SMP_TIMER_TEST: PASS" \
    "KERNEL_PRINT_TEST: PASS" \
    "RECOVERABLE_TRAP_TEST: PASS" \
    "MACHINE_TIMER_TEST: PASS" \
    "M00_08_HOSTBOOT_BOOTSTRAP: PASS" \
    "M00_08_TASK_DISPATCH: PASS" \
    "M00_08_IPC_GENERATION_CEILING: PASS" \
    "M00_08_IPC_SLOT_RETIREMENT: PASS"
do
    if ! grep -Fxq "${boot_marker}" "${LOG_FILE}"; then
        echo "M00-08.03.01: boot marker not found: ${boot_marker}" >&2
        exit 1
    fi
done

if (( SMP_HARTS == 1 )); then
    for required_marker in \
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
    "M00_08_IPC_C01_SENDER_TRACKED: PASS" \
    "M00_08_IPC_C01_SEND_BEFORE_RECV: PASS" \
    "M00_08_IPC_C03_GOT_1: PASS" \
    "M00_08_IPC_C03_GOT_2: PASS" \
    "M00_08_IPC_C03_GOT_3: PASS" \
    "M00_08_IPC_C03_FIFO: PASS" \
    "M00_08_IPC_C16_FULL: PASS" \
    "M00_08_IPC_C16_POP_OLDEST: PASS" \
    "M00_08_IPC_C16_RECOVER: PASS" \
    "M00_08_IPC_C14_MALFORMED: PASS" \
    "M00_08_IPC_C14_BIT63_REJECTED: PASS" \
    "M00_08_IPC_DESTROY_NONOWNER: PASS" \
    "M00_08_IPC_ENDPOINT_DESTROY: PASS" \
    "M00_08_IPC_C14_STALE: PASS" \
    "M00_08_IPC_C14B_RECYCLED_GENERATION: PASS" \
    "M00_08_IPC_C14B_ISOLATION: PASS" \
    "M00_08_IPC_C15_EMPTY_EAGAIN: PASS" \
    "M00_08_IPC_C15_INTERLEAVED_SEND: PASS" \
    "M00_08_IPC_C15_NONBLOCKING: PASS" \
    "M00_08_IPC_RESERVED_ENOSYS: PASS" \
    "M00_08_IPC_ENDPOINT_ENOSPC: PASS" \
    "M00_08_IPC_NONBLOCKING: PASS"
    do
        if ! grep -Fxq "${required_marker}" "${LOG_FILE}"; then
            echo "M00-08.03.01: required marker not found: ${required_marker}" >&2
            exit 1
        fi
    done

    # Ordered-marker assertions: single-hart cross-case happened-before evidence.
    # Multi-hart runs use the structured history below: printk emits characters,
    # not atomic records, so concurrent marker lines can legitimately interleave.
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

# C01: the send linearization point precedes the receive-side delivery, the
# second-sender delivery (sender TaskId tracking), and the consumer's own
# full-register assertion (send-before-recv persistence).
    check_marker_order \
        "M00_08_IPC_C01_A_SENT" \
        "M00_08_IPC_C01_B_GOT" \
        "M00_08_IPC_C01_SENDER_TRACKED" \
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

# C14b: destroy precedes every stale-handle rejection, which precedes the
# recycled-generation recreate and the new-epoch delivery.
    check_marker_order \
        "M00_08_IPC_ENDPOINT_DESTROY" \
        "M00_08_IPC_C14_STALE" \
        "M00_08_IPC_C14B_RECYCLED_GENERATION" \
        "M00_08_IPC_C14B_ISOLATION"

# C15 (research acceptance plan): the empty-queue -EAGAIN precedes the
# interleaved send, which precedes the post-drain -EAGAIN. Every marker is
# emitted after the syscall returned, so the chain is the never-blocks proof.
    check_marker_order \
        "M00_08_IPC_C15_EMPTY_EAGAIN" \
        "M00_08_IPC_C15_INTERLEAVED_SEND" \
        "M00_08_IPC_C15_NONBLOCKING"

# Generation ceiling: the epoch-cap evidence precedes the slot-retirement
# evidence (boot-time white-box probe on the boot hart).
    check_marker_order \
        "M00_08_IPC_GENERATION_CEILING" \
        "M00_08_IPC_SLOT_RETIREMENT"

# The endpoint must exist before the first send can succeed on it.
    check_marker_order \
        "M00_08_IPC_ENDPOINT_CREATE" \
        "M00_08_IPC_C01_A_SENT"
else
    for trace_marker in \
        "JIXIA_VERIFY_TRACE_BEGIN: " \
        "JIXIA_VERIFY_TRACE_END: "
    do
        if ! grep -Fq "${trace_marker}" "${LOG_FILE}"; then
            echo "M00-08.03.01: structured trace marker not found: ${trace_marker}" >&2
            exit 1
        fi
    done
    echo "M00-08.03.01: SMP legacy marker oracle skipped (concurrent printk records are not atomic)"
fi

if [[ "${VERIFICATION}" == "1" ]]; then
    python3 "${ROOT_DIR}/scripts/check-microkernel-trace.py" \
        --expected-harts "${SMP_HARTS}" \
        "${LOG_FILE}"
fi

echo "M00-08.03.01 non-blocking IPC: PASS"
