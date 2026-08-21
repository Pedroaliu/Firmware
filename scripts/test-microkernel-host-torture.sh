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
    -I"${ROOT_DIR}"
)

"${HOST_CXX}" \
    "${COMMON_FLAGS[@]}" \
    -O2 \
    "${ROOT_DIR}/verification/host/ipc_torture.cpp" \
    "${ROOT_DIR}/microkernel/core/ipc_manager.cpp" \
    -o "${BUILD_DIR}/ipc_torture"

"${HOST_CXX}" \
    "${COMMON_FLAGS[@]}" \
    -O1 \
    -g \
    -fno-omit-frame-pointer \
    -fsanitize=address,undefined \
    "${ROOT_DIR}/verification/host/ipc_torture.cpp" \
    "${ROOT_DIR}/microkernel/core/ipc_manager.cpp" \
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
        "${ROOT_DIR}/verification/host/ipc_torture.cpp" \
        "${ROOT_DIR}/microkernel/core/ipc_manager.cpp" \
        -o "${BUILD_DIR}/ipc_torture_tsan"

    {
        echo "compiler=${TSAN_CXX}"
        "${TSAN_CXX}" --version | head -n 1
        uname -a
    } | tee "${BUILD_DIR}/tsan-environment.log"

    set +e
    TSAN_OPTIONS="${JIXIA_TSAN_OPTIONS:-halt_on_error=1:second_deadlock_stack=1}" \
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
