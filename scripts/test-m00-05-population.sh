#!/usr/bin/env bash

set -euo pipefail


readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

readonly BUILD_DIR="${ROOT_DIR}/build/clion-debug"
readonly FIRMWARE="${BUILD_DIR}/jixia.bin"

readonly TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-5}"


exec > >(tee "${BUILD_DIR}/m00-05-population-report.log") 2>&1


echo "========================================"
echo " Jixia M00-05 Population Matrix"
echo "========================================"

date


echo
echo "[0] Build"
echo

cmake --build "${BUILD_DIR}" --target jixia.elf


if [[ ! -f "${FIRMWARE}" ]]; then
    echo "ERROR: firmware not found: ${FIRMWARE}"
    exit 1
fi


run_supported_case()
{
    local smp="$1"

    local raw_log="${BUILD_DIR}/m00-05-population-smp${smp}.raw.log"
    local serial_log="${BUILD_DIR}/m00-05-population-smp${smp}.log"
    local qemu_log="${BUILD_DIR}/m00-05-population-smp${smp}.qemu.log"


    echo
    echo "========================================"
    echo " CASE: -smp ${smp}"
    echo "========================================"


    rm -f \
        "${raw_log}" \
        "${serial_log}" \
        "${qemu_log}"


    set +e

    timeout \
        --kill-after=1s \
        "${TIMEOUT_SECONDS}s" \
        qemu-system-riscv64 \
            -machine virt \
            -cpu rv64 \
            -m 128M \
            -smp "${smp}" \
            -bios "${FIRMWARE}" \
            -display none \
            -serial "file:${raw_log}" \
            -monitor none \
            >"${qemu_log}" 2>&1

    local status=$?

    set -e


    if [[ ${status} -ne 124 ]]; then
        echo "QEMU_STATUS: FAIL (${status})"

        if [[ -s "${qemu_log}" ]]; then
            cat "${qemu_log}"
        fi

        exit 1
    fi


    tr -d '\r' <"${raw_log}" >"${serial_log}"


    cat "${serial_log}"


    echo
    echo "[verify -smp ${smp}]"


    if grep -Fq \
        "[Jixia][Microkernel][fatal trap]" \
        "${serial_log}"
    then
        echo "FATAL_TRAP: FAIL"
        exit 1
    fi


    if ! grep -Fq \
        "smp capacity: 4 hart(s)" \
        "${serial_log}"
    then
        echo "CAPACITY: FAIL"
        exit 1
    fi


    if ! grep -Fq \
        "smp present : ${smp} hart(s)" \
        "${serial_log}"
    then
        echo "PRESENT_COUNT: FAIL"
        exit 1
    fi


    if ! grep -Fxq \
        "SMP_POPULATION_TEST: PASS" \
        "${serial_log}"
    then
        echo "POPULATION_MARKER: FAIL"
        exit 1
    fi


    if ! grep -Fxq \
        "SMP_FOUNDATION_TEST: PASS" \
        "${serial_log}"
    then
        echo "SMP_FOUNDATION: FAIL"
        exit 1
    fi


    python3 - "${serial_log}" "${smp}" <<'PY'
import re
import sys


log_path = sys.argv[1]
expected_count = int(sys.argv[2])


pattern = re.compile(
    r"^slot\s+(\d+)\s*:\s*"
    r"hart=(0x[0-9a-fA-F]+)\s+"
    r"role=(boot|secondary)\s+"
    r"stack=\[(0x[0-9a-fA-F]+),\s*"
    r"(0x[0-9a-fA-F]+)\)"
)


records = []


with open(log_path, "r", encoding="utf-8") as log:
    for line in log:
        match = pattern.match(line.strip())

        if match is None:
            continue

        records.append(
            {
                "slot": int(match.group(1)),
                "hart": int(match.group(2), 16),
                "role": match.group(3),
                "bottom": int(match.group(4), 16),
                "top": int(match.group(5), 16),
            }
        )


if len(records) != expected_count:
    raise SystemExit(
        "POPULATION_LAYOUT: FAIL "
        f"(expected {expected_count} records, "
        f"got {len(records)})"
    )


expected_slots = set(range(expected_count))

actual_slots = {
    record["slot"]
    for record in records
}


if actual_slots != expected_slots:
    raise SystemExit(
        "POPULATION_LAYOUT: FAIL "
        f"(slots={sorted(actual_slots)}, "
        f"expected={sorted(expected_slots)})"
    )


harts = [
    record["hart"]
    for record in records
]


if len(set(harts)) != expected_count:
    raise SystemExit(
        "POPULATION_LAYOUT: FAIL "
        "(duplicate hart identity)"
    )


boot = next(
    record
    for record in records
    if record["slot"] == 0
)


if boot["hart"] != 0:
    raise SystemExit(
        "POPULATION_LAYOUT: FAIL "
        "(boot slot is not hart 0)"
    )


if boot["role"] != "boot":
    raise SystemExit(
        "POPULATION_LAYOUT: FAIL "
        "(slot 0 role is not boot)"
    )


for record in records:

    size = record["top"] - record["bottom"]

    if size != 16384:
        raise SystemExit(
            "POPULATION_LAYOUT: FAIL "
            f"(slot {record['slot']} stack={size})"
        )


ranges = sorted(
    (
        record["bottom"],
        record["top"],
        record["slot"],
    )
    for record in records
)


for previous, current in zip(ranges, ranges[1:]):

    if previous[1] > current[0]:
        raise SystemExit(
            "POPULATION_LAYOUT: FAIL "
            f"(stack overlap slot {previous[2]} "
            f"and slot {current[2]})"
        )


print(f"present harts : {expected_count}")
print("dense slots   : PASS")
print("unique harts  : PASS")
print("boot identity : PASS")
print("private stack : PASS")
print("POPULATION_LAYOUT: PASS")
PY


    for marker in \
        "KERNEL_PRINT_TEST: PASS" \
        "RECOVERABLE_TRAP_TEST: PASS" \
        "MACHINE_TIMER_TEST: PASS" \
        "TRAP_FRAME_TEST: PASS"
    do
        if ! grep -Fxq "${marker}" "${serial_log}"; then
            echo "REGRESSION: FAIL"
            echo "missing: ${marker}"
            exit 1
        fi
    done


    echo "-smp ${smp}: PASS"
}


run_unsupported_case()
{
    local smp=5

    local raw_log="${BUILD_DIR}/m00-05-population-smp${smp}.raw.log"
    local serial_log="${BUILD_DIR}/m00-05-population-smp${smp}.log"
    local qemu_log="${BUILD_DIR}/m00-05-population-smp${smp}.qemu.log"


    echo
    echo "========================================"
    echo " CASE: -smp ${smp} (expected reject)"
    echo "========================================"


    rm -f \
        "${raw_log}" \
        "${serial_log}" \
        "${qemu_log}"


    set +e

    timeout \
        --kill-after=1s \
        "${TIMEOUT_SECONDS}s" \
        qemu-system-riscv64 \
            -machine virt \
            -cpu rv64 \
            -m 128M \
            -smp "${smp}" \
            -bios "${FIRMWARE}" \
            -display none \
            -serial "file:${raw_log}" \
            -monitor none \
            >"${qemu_log}" 2>&1

    local status=$?

    set -e


    if [[ ${status} -ne 124 ]]; then
        echo "QEMU_STATUS: FAIL (${status})"
        exit 1
    fi


    tr -d '\r' <"${raw_log}" >"${serial_log}"


    cat "${serial_log}"


    if grep -Fq \
        "[Jixia][Microkernel][fatal trap]" \
        "${serial_log}"
    then
        echo "OVER_CAPACITY_FATAL_TRAP: FAIL"
        exit 1
    fi


    if ! grep -Fq \
        "unsupported CPU count 5" \
        "${serial_log}"
    then
        echo "OVER_CAPACITY_DIAGNOSTIC: FAIL"
        exit 1
    fi


    if ! grep -Fxq \
        "SMP_POPULATION_TEST: FAIL" \
        "${serial_log}"
    then
        echo "OVER_CAPACITY_MARKER: FAIL"
        exit 1
    fi


    echo "CONTROLLED_OVER_CAPACITY: PASS"
}


run_supported_case 1
run_supported_case 2
run_supported_case 4

run_unsupported_case


echo
echo "========================================"
echo " M00-05 POPULATION MATRIX: PASS"
echo "========================================"