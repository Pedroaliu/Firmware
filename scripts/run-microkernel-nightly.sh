#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"
readonly RUN_ID="${JIXIA_NIGHTLY_RUN_ID:-$(date -u +%Y%m%dT%H%M%SZ)}"
readonly ARTIFACT_ROOT="${JIXIA_NIGHTLY_ARTIFACT_DIR:-${ROOT_DIR}/build/nightly/${RUN_ID}}"
readonly SEED_TEXT="${JIXIA_NIGHTLY_SEEDS:-1 7 42 99 31337 65537}"
readonly HART_TEXT="${JIXIA_NIGHTLY_HARTS:-2 4}"
readonly HOST_MESSAGES="${JIXIA_NIGHTLY_HOST_MESSAGES:-20000}"
readonly QEMU_TIMEOUT="${JIXIA_NIGHTLY_QEMU_TIMEOUT_SECONDS:-15}"
readonly TRACE_RECORDS="${JIXIA_NIGHTLY_TRACE_RECORDS:-4096}"

mkdir -p "${ARTIFACT_ROOT}"

cat >"${ARTIFACT_ROOT}/manifest.txt" <<EOF
run_id=${RUN_ID}
git_head=$(git -C "${ROOT_DIR}" rev-parse HEAD)
git_branch=$(git -C "${ROOT_DIR}" branch --show-current)
seeds=${SEED_TEXT}
harts=${HART_TEXT}
host_messages=${HOST_MESSAGES}
trace_records=${TRACE_RECORDS}
EOF

bash "${ROOT_DIR}/scripts/test-microkernel-verification-tools.sh" |
    tee "${ARTIFACT_ROOT}/models.log"

JIXIA_HOST_TORTURE_BUILD_DIR="${ARTIFACT_ROOT}/host" \
JIXIA_TORTURE_MESSAGES="${HOST_MESSAGES}" \
JIXIA_TORTURE_SEEDS="${SEED_TEXT}" \
JIXIA_HOST_TSAN=1 \
    bash "${ROOT_DIR}/scripts/test-microkernel-host-torture.sh" |
    tee "${ARTIFACT_ROOT}/host.log"

read -r -a seeds <<<"${SEED_TEXT}"
read -r -a harts <<<"${HART_TEXT}"

for hart_count in "${harts[@]}"; do
    if [[ ! "${hart_count}" =~ ^[1-4]$ ]]; then
        echo "invalid nightly hart count: ${hart_count}" >&2
        exit 2
    fi

    for seed in "${seeds[@]}"; do
        readonly_case_dir="${ARTIFACT_ROOT}/qemu-smp${hart_count}-seed${seed}"
        mkdir -p "${readonly_case_dir}"
        echo "NIGHTLY_CASE_BEGIN: smp=${hart_count} seed=${seed}" |
            tee "${readonly_case_dir}/case.log"

        JIXIA_M00_08_03_01_BUILD_DIR="${readonly_case_dir}/build" \
        JIXIA_QEMU_TIMEOUT_SECONDS="${QEMU_TIMEOUT}" \
        JIXIA_QEMU_SMP_HARTS="${hart_count}" \
        JIXIA_QEMU_TCG_THREAD=multi \
        JIXIA_VERIFICATION=1 \
        JIXIA_VERIFICATION_JITTER=1 \
        JIXIA_VERIFICATION_SEED="${seed}" \
        JIXIA_VERIFICATION_TRACE_RECORDS="${TRACE_RECORDS}" \
            bash "${ROOT_DIR}/scripts/test-m00-08-03-01-ipc-nonblocking.sh" 2>&1 |
            tee -a "${readonly_case_dir}/case.log"

        echo "NIGHTLY_CASE_END: smp=${hart_count} seed=${seed} result=PASS" |
            tee -a "${readonly_case_dir}/case.log"
    done
done

for seed in "${seeds[@]}"; do
    blocking_case_dir="${ARTIFACT_ROOT}/qemu-blocking-seed${seed}"
    mkdir -p "${blocking_case_dir}"
    echo "NIGHTLY_BLOCKING_BEGIN: smp=1,2 seed=${seed}" |
        tee "${blocking_case_dir}/case.log"

    JIXIA_M00_08_03_02_BUILD_DIR="${blocking_case_dir}/build" \
    JIXIA_QEMU_TIMEOUT_SECONDS="${QEMU_TIMEOUT}" \
    JIXIA_VERIFICATION_SEED="${seed}" \
    JIXIA_VERIFICATION_TRACE_RECORDS="${TRACE_RECORDS}" \
        bash "${ROOT_DIR}/scripts/test-m00-08-03-02-ipc-blocking-recv.sh" 2>&1 |
        tee -a "${blocking_case_dir}/case.log"

    echo "NIGHTLY_BLOCKING_END: smp=1,2 seed=${seed} result=PASS" |
        tee -a "${blocking_case_dir}/case.log"
done

echo "JIXIA_MICROKERNEL_NIGHTLY: PASS run_id=${RUN_ID} artifacts=${ARTIFACT_ROOT}"
