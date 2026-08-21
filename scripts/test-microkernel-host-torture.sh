#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly BUILD_DIR="${JIXIA_HOST_TORTURE_BUILD_DIR:-${ROOT_DIR}/build/host-torture}"
readonly MESSAGES="${JIXIA_TORTURE_MESSAGES:-2000}"
readonly SEED_TEXT="${JIXIA_TORTURE_SEEDS:-1 7 42}"
readonly HOST_CXX="${JIXIA_HOST_CXX:-g++}"
readonly TSAN_CXX="${JIXIA_HOST_TSAN_CXX:-${HOST_CXX}}"
readonly TSAN_DISABLE_ASLR="${JIXIA_TSAN_DISABLE_ASLR:-auto}"

if [[ "${TSAN_DISABLE_ASLR}" != "auto" && "${TSAN_DISABLE_ASLR}" != "0" &&
      "${TSAN_DISABLE_ASLR}" != "1" ]]; then
    echo "JIXIA_TSAN_DISABLE_ASLR must be auto, 0, or 1" >&2
    exit 2
fi

if ! command -v "${HOST_CXX}" >/dev/null 2>&1; then
    echo "Missing host C++ compiler: ${HOST_CXX}" >&2
    exit 2
fi
if [[ "${JIXIA_HOST_TSAN:-0}" == "1" ]] && ! command -v "${TSAN_CXX}" >/dev/null 2>&1; then
    echo "Missing TSan C++ compiler: ${TSAN_CXX}" >&2
    exit 2
fi

mkdir -p "${BUILD_DIR}"

readonly COMMON_FLAGS=(
    -std=c++20
    -pthread
    -Wall
    -Wextra
    -Werror
    -DJIXIA_HOST_IPC_TORTURE=1
    -I"${ROOT_DIR}"
)

readonly HOST_SOURCES=(
    "${ROOT_DIR}/verification/host/ipc_torture.cpp"
    "${ROOT_DIR}/verification/host/ipc_kernel_stubs.cpp"
    "${ROOT_DIR}/microkernel/core/ipc_manager.cpp"
)

# Recreate linker outputs so a copied/restored build directory cannot retain a
# non-executable mode bit and turn a valid host binary into a false test failure.
rm -f \
    "${BUILD_DIR}/ipc_torture" \
    "${BUILD_DIR}/ipc_torture_asan" \
    "${BUILD_DIR}/ipc_torture_tsan"

"${HOST_CXX}" \
    "${COMMON_FLAGS[@]}" \
    -O2 \
    "${HOST_SOURCES[@]}" \
    -o "${BUILD_DIR}/ipc_torture"

"${HOST_CXX}" \
    "${COMMON_FLAGS[@]}" \
    -O1 \
    -g \
    -fno-omit-frame-pointer \
    -fsanitize=address,undefined \
    "${HOST_SOURCES[@]}" \
    -o "${BUILD_DIR}/ipc_torture_asan"

read -r -a seeds <<<"${SEED_TEXT}"
for seed in "${seeds[@]}"; do
    "${BUILD_DIR}/ipc_torture" "${seed}" "${MESSAGES}" |
        tee "${BUILD_DIR}/seed-${seed}.log"
done

# Sanitizers are intentionally a separate lane from the optimized contention
# run.  One representative seed keeps the fast gate bounded.
# LeakSanitizer needs /proc task inspection and fails under some CI/container
# ptrace profiles.  This harness has no production dynamic allocation path to
# validate, so ASan/UBSan remain mandatory while leak scanning is opt-in.
ASAN_OPTIONS=detect_leaks=${JIXIA_ASAN_DETECT_LEAKS:-0}:halt_on_error=1 \
UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
    "${BUILD_DIR}/ipc_torture_asan" "${seeds[0]}" "${MESSAGES}" |
    tee "${BUILD_DIR}/asan-seed-${seeds[0]}.log"

if [[ "${JIXIA_HOST_TSAN:-0}" == "1" ]]; then
    "${TSAN_CXX}" \
        "${COMMON_FLAGS[@]}" \
        -O1 \
        -g \
        -fno-omit-frame-pointer \
        -fsanitize=thread \
        "${HOST_SOURCES[@]}" \
        -o "${BUILD_DIR}/ipc_torture_tsan"

    tsan_runner=()
    tsan_aslr_mode="native"
    if [[ "$(uname -s)" == "Linux" && "${TSAN_DISABLE_ASLR}" != "0" ]]; then
        if command -v setarch >/dev/null 2>&1 &&
           setarch "$(uname -m)" --addr-no-randomize true >/dev/null 2>&1; then
            tsan_runner=(setarch "$(uname -m)" --addr-no-randomize)
            tsan_aslr_mode="disabled-for-child"
        elif [[ "${TSAN_DISABLE_ASLR}" == "1" ]]; then
            echo "HOST_IPC_TSAN: FAIL setarch cannot disable ASLR for the child process" >&2
            exit 2
        else
            tsan_aslr_mode="native-setarch-unavailable"
        fi
    fi

    {
        echo "compiler=${TSAN_CXX}"
        "${TSAN_CXX}" --version | head -n 1
        uname -a
        echo "tsan_aslr_mode=${tsan_aslr_mode}"
        if [[ -r /proc/sys/vm/mmap_rnd_bits ]]; then
            echo "vm.mmap_rnd_bits=$(</proc/sys/vm/mmap_rnd_bits)"
        fi
        if [[ -r /proc/sys/vm/legacy_va_layout ]]; then
            echo "vm.legacy_va_layout=$(</proc/sys/vm/legacy_va_layout)"
        fi
        if [[ -r /proc/sys/kernel/randomize_va_space ]]; then
            echo "kernel.randomize_va_space=$(</proc/sys/kernel/randomize_va_space)"
        fi
    } | tee "${BUILD_DIR}/tsan-environment.log"

    set +e
    TSAN_OPTIONS="${JIXIA_TSAN_OPTIONS:-halt_on_error=1:second_deadlock_stack=1}" \
        "${tsan_runner[@]}" \
        "${BUILD_DIR}/ipc_torture_tsan" "${seeds[0]}" "${MESSAGES}" 2>&1 |
        tee "${BUILD_DIR}/tsan-seed-${seeds[0]}.log"
    readonly TSAN_STATUS=${PIPESTATUS[0]}
    set -e
    if [[ ${TSAN_STATUS} -ne 0 ]]; then
        echo "HOST_IPC_TSAN: FAIL status=${TSAN_STATUS}" >&2
        echo "diagnostics: ${BUILD_DIR}/tsan-environment.log" >&2
        echo "diagnostics: ${BUILD_DIR}/tsan-seed-${seeds[0]}.log" >&2
        exit "${TSAN_STATUS}"
    fi
    echo "HOST_IPC_TSAN: PASS compiler=${TSAN_CXX}"
fi

echo "Jixia host microkernel torture: PASS"
