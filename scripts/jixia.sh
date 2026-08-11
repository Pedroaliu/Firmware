#!/usr/bin/env bash
#
# Jixia developer command.
#
# Examples:
#   bash scripts/jixia.sh env
#   bash scripts/jixia.sh configure
#   bash scripts/jixia.sh build
#   bash scripts/jixia.sh run --smp 4
#   bash scripts/jixia.sh run --smp 4 --timeout 10
#   bash scripts/jixia.sh run --smp 4 -- -d int,guest_errors
#   bash scripts/jixia.sh debug --smp 4
#   bash scripts/jixia.sh debug --break jixia_microkernel_boot_main
#   bash scripts/jixia.sh debug --server-only
#
set -euo pipefail

readonly SCRIPT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")"
    pwd
)"
readonly ROOT_DIR="$(
    cd "${SCRIPT_DIR}/.."
    pwd
)"

# Keep the default build directory aligned with CMakePresets.json, CLion, and
# the existing milestone test scripts.
BUILD_DIR="${JIXIA_BUILD_DIR:-${ROOT_DIR}/build/clion-debug}"
BUILD_TYPE="${JIXIA_BUILD_TYPE:-Debug}"

MACHINE="${JIXIA_QEMU_MACHINE:-virt}"
CPU="${JIXIA_QEMU_CPU:-rv64}"
MEMORY="${JIXIA_QEMU_MEMORY:-128M}"
SMP="${JIXIA_QEMU_SMP:-4}"
TIMEOUT_SECONDS="${JIXIA_QEMU_TIMEOUT_SECONDS:-5}"
GDB_PORT="${JIXIA_GDB_PORT:-1234}"

QEMU="${JIXIA_QEMU:-qemu-system-riscv64}"
CC="${JIXIA_CC:-riscv64-unknown-elf-gcc}"
CXX="${JIXIA_CXX:-riscv64-unknown-elf-g++}"
OBJCOPY="${JIXIA_OBJCOPY:-riscv64-unknown-elf-objcopy}"
OBJDUMP="${JIXIA_OBJDUMP:-riscv64-unknown-elf-objdump}"
READELF="${JIXIA_READELF:-riscv64-unknown-elf-readelf}"

NO_BUILD=0
RECONFIGURE=0
CONSOLE=1
SERVER_ONLY=0
BREAK_SYMBOL=""
declare -a QEMU_EXTRA=()

usage()
{
    cat <<'EOF'
Jixia developer command

Usage:
  jixia.sh <command> [options] [-- <extra QEMU args>]

Commands:
  env          Print detected development tools.
  configure    Configure a Ninja debug build.
  build        Configure if needed, then build jixia.elf.
  run          Build and boot Jixia under QEMU; capture logs.
  debug        Build and start QEMU halted with a GDB server.
  clean        Remove the selected build directory.
  help         Show this help.

Common options:
  --build-dir DIR       Build directory (default: build/clion-debug)
  --build-type TYPE     CMake build type (default: Debug)
  --smp N               QEMU hart count (default: 4)
  --memory SIZE         QEMU memory size (default: 128M)
  --machine NAME        QEMU machine (default: virt)
  --cpu NAME            QEMU CPU model (default: rv64)
  --no-build            Do not build before run/debug.
  --reconfigure         Force CMake configuration.

Run options:
  --timeout SEC         Stop QEMU after SEC seconds; 0 disables timeout.
  --no-console          Do not stream UART to terminal; log to file only.

Debug options:
  --gdb-port PORT       GDB TCP port (default: 1234)
  --server-only         Start QEMU GDB server and print attach command.
  --break SYMBOL        Set a temporary breakpoint and continue to SYMBOL.

Environment overrides:
  JIXIA_BUILD_DIR
  JIXIA_BUILD_TYPE
  JIXIA_QEMU
  JIXIA_CC
  JIXIA_CXX
  JIXIA_GDB_PORT
  JIXIA_QEMU_SMP
  JIXIA_QEMU_MEMORY
  JIXIA_QEMU_TIMEOUT_SECONDS

Examples:
  bash scripts/jixia.sh build
  bash scripts/jixia.sh run --smp 1
  bash scripts/jixia.sh run --smp 4 --timeout 10
  bash scripts/jixia.sh run --smp 4 -- -d int,guest_errors
  bash scripts/jixia.sh debug --smp 4
  bash scripts/jixia.sh debug --break jixia_microkernel_boot_main
EOF
}

info()
{
    printf '[Jixia] %s\n' "$*"
}

warn()
{
    printf '[Jixia][WARN] %s\n' "$*" >&2
}

die()
{
    printf '[Jixia][ERROR] %s\n' "$*" >&2
    exit 1
}

have()
{
    command -v "$1" >/dev/null 2>&1
}

require_command()
{
    have "$1" || die "missing command: $1; run bash scripts/setup-dev-env.sh"
}

firmware_elf()
{
    printf '%s/jixia.elf\n' "${BUILD_DIR}"
}

firmware_bin()
{
    printf '%s/jixia.bin\n' "${BUILD_DIR}"
}

configured()
{
    [[ -f "${BUILD_DIR}/CMakeCache.txt" && -f "${BUILD_DIR}/build.ninja" ]]
}

configure()
{
    require_command cmake
    require_command ninja
    require_command "${CC}"
    require_command "${CXX}"
    require_command "${OBJCOPY}"
    require_command "${OBJDUMP}"
    require_command "${READELF}"

    if configured && [[ ${RECONFIGURE} -eq 0 ]]; then
        info "already configured: ${BUILD_DIR}"
        return
    fi

    mkdir -p "${BUILD_DIR}"

    info "configuring ${BUILD_TYPE} build"
    info "build dir: ${BUILD_DIR}"

    cmake \
        -S "${ROOT_DIR}" \
        -B "${BUILD_DIR}" \
        -G Ninja \
        -DCMAKE_BUILD_TYPE="${BUILD_TYPE}" \
        -DCMAKE_SYSTEM_NAME=Generic \
        -DCMAKE_SYSTEM_PROCESSOR=riscv64 \
        -DCMAKE_TRY_COMPILE_TARGET_TYPE=STATIC_LIBRARY \
        -DCMAKE_C_COMPILER="${CC}" \
        -DCMAKE_CXX_COMPILER="${CXX}" \
        -DCMAKE_ASM_COMPILER="${CC}" \
        -DCMAKE_OBJCOPY="${OBJCOPY}" \
        -DCMAKE_OBJDUMP="${OBJDUMP}" \
        -DCMAKE_READELF="${READELF}"
}

build()
{
    configure
    info "building jixia.elf"
    cmake --build "${BUILD_DIR}" --target jixia.elf

    [[ -f "$(firmware_elf)" ]] || die "missing ELF: $(firmware_elf)"
    [[ -f "$(firmware_bin)" ]] || die "missing binary: $(firmware_bin)"

    info "ELF: $(firmware_elf)"
    info "BIN: $(firmware_bin)"
}

new_log_dir()
{
    local mode="$1"
    local timestamp
    timestamp="$(date '+%Y%m%d-%H%M%S')"

    printf '%s/logs/%s-%s\n' "${BUILD_DIR}" "${mode}" "${timestamp}"
}

base_qemu_args()
{
    local -n out_array="$1"

    out_array=(
        -machine "${MACHINE}"
        -cpu "${CPU}"
        -m "${MEMORY}"
        -smp "${SMP}"
        -bios "$(firmware_bin)"
        -display none
        -monitor none
    )
}

run_qemu()
{
    require_command "${QEMU}"
    require_command timeout

    if [[ ${NO_BUILD} -eq 0 ]]; then
        build
    fi

    [[ -f "$(firmware_bin)" ]] || die "firmware not found: $(firmware_bin)"

    local log_dir
    log_dir="$(new_log_dir run)"
    mkdir -p "${log_dir}"

    local serial_log="${log_dir}/serial.log"
    local qemu_log="${log_dir}/qemu.log"
    local command_log="${log_dir}/command.txt"

    local -a args
    base_qemu_args args

    if [[ ${CONSOLE} -eq 1 ]]; then
        args+=(-serial stdio)
    else
        args+=(-serial "file:${serial_log}")
    fi

    args+=("${QEMU_EXTRA[@]}")

    {
        printf '%q ' "${QEMU}" "${args[@]}"
        printf '\n'
    } >"${command_log}"

    info "QEMU: ${MACHINE}, CPU=${CPU}, SMP=${SMP}, RAM=${MEMORY}"
    info "logs: ${log_dir}"

    local status

    set +e

    if [[ ${CONSOLE} -eq 1 ]]; then
        if [[ "${TIMEOUT_SECONDS}" == "0" ]]; then
            "${QEMU}" "${args[@]}" \
                2> >(tee "${qemu_log}" >&2) \
                | tee "${serial_log}"
            status=${PIPESTATUS[0]}
        else
            timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
                "${QEMU}" "${args[@]}" \
                2> >(tee "${qemu_log}" >&2) \
                | tee "${serial_log}"
            status=${PIPESTATUS[0]}
        fi
    else
        if [[ "${TIMEOUT_SECONDS}" == "0" ]]; then
            "${QEMU}" "${args[@]}" >"${qemu_log}" 2>&1
            status=$?
        else
            timeout --kill-after=1s "${TIMEOUT_SECONDS}s" \
                "${QEMU}" "${args[@]}" >"${qemu_log}" 2>&1
            status=$?
        fi
    fi

    set -e

    if [[ ${status} -eq 124 ]]; then
        info "QEMU timeout after ${TIMEOUT_SECONDS}s (expected for parked firmware)"
        return 0
    fi

    if [[ ${status} -ne 0 && ${status} -ne 130 ]]; then
        warn "QEMU exited with status ${status}"
        return "${status}"
    fi

    return 0
}

select_gdb()
{
    if have riscv64-unknown-elf-gdb; then
        printf '%s\n' riscv64-unknown-elf-gdb
        return
    fi

    if have gdb-multiarch; then
        printf '%s\n' gdb-multiarch
        return
    fi

    die "no RISC-V-capable GDB found; run bash scripts/setup-dev-env.sh"
}

debug_qemu()
{
    require_command "${QEMU}"

    if [[ ${NO_BUILD} -eq 0 ]]; then
        build
    fi

    [[ -f "$(firmware_elf)" ]] || die "ELF not found: $(firmware_elf)"
    [[ -f "$(firmware_bin)" ]] || die "firmware not found: $(firmware_bin)"

    local log_dir
    log_dir="$(new_log_dir debug)"
    mkdir -p "${log_dir}"

    local serial_log="${log_dir}/serial.log"
    local qemu_log="${log_dir}/qemu.log"
    local command_log="${log_dir}/command.txt"

    local -a args
    base_qemu_args args
    args+=(
        -serial "file:${serial_log}"
        -S
        -gdb "tcp::${GDB_PORT}"
    )
    args+=("${QEMU_EXTRA[@]}")

    {
        printf '%q ' "${QEMU}" "${args[@]}"
        printf '\n'
    } >"${command_log}"

    info "starting QEMU halted"
    info "GDB port: ${GDB_PORT}"
    info "logs: ${log_dir}"

    "${QEMU}" "${args[@]}" >"${qemu_log}" 2>&1 &
    local qemu_pid=$!

    cleanup()
    {
        if kill -0 "${qemu_pid}" 2>/dev/null; then
            kill "${qemu_pid}" 2>/dev/null || true
            wait "${qemu_pid}" 2>/dev/null || true
        fi
    }

    trap cleanup EXIT INT TERM

    # QEMU opens its GDB socket very early. Avoid probing it with a second
    # protocol client; a short wait plus the process-liveness check is enough.
    sleep 0.25
    kill -0 "${qemu_pid}" 2>/dev/null || die "QEMU exited before GDB attach"

    local gdb
    gdb="$(select_gdb)"

    local -a gdb_args=(
        -q
        "$(firmware_elf)"
        -ex "set architecture riscv:rv64"
        -ex "set pagination off"
        -ex "set remotetimeout 5"
        -ex "target remote :${GDB_PORT}"
    )

    if [[ -n "${BREAK_SYMBOL}" ]]; then
        gdb_args+=(
            -ex "tbreak ${BREAK_SYMBOL}"
            -ex "continue"
        )
    fi

    if [[ ${SERVER_ONLY} -eq 1 ]]; then
        echo
        info "QEMU is waiting for GDB."
        printf 'Attach with:\n  %q ' "${gdb}"
        printf '%q ' "${gdb_args[@]}"
        printf '\n'
        echo
        info "Press Ctrl+C here to stop QEMU."

        wait "${qemu_pid}"
        return
    fi

    "${gdb}" "${gdb_args[@]}"
}

print_env()
{
    local cmd

    for cmd in \
        git \
        cmake \
        ninja \
        python3 \
        "${QEMU}" \
        "${CC}" \
        "${CXX}" \
        "${OBJCOPY}" \
        "${OBJDUMP}" \
        "${READELF}"
    do
        if have "${cmd}"; then
            printf '%-32s %s\n' "${cmd}" "$(command -v "${cmd}")"
        else
            printf '%-32s %s\n' "${cmd}" "MISSING"
        fi
    done

    if have riscv64-unknown-elf-gdb; then
        printf '%-32s %s\n' riscv64-unknown-elf-gdb "$(command -v riscv64-unknown-elf-gdb)"
    elif have gdb-multiarch; then
        printf '%-32s %s\n' gdb-multiarch "$(command -v gdb-multiarch)"
    else
        printf '%-32s %s\n' GDB "MISSING"
    fi

    echo
    printf 'ROOT_DIR=%s\n' "${ROOT_DIR}"
    printf 'BUILD_DIR=%s\n' "${BUILD_DIR}"
}

clean()
{
    if [[ ! -d "${BUILD_DIR}" ]]; then
        info "nothing to clean: ${BUILD_DIR}"
        return
    fi

    case "${BUILD_DIR}" in
        "${ROOT_DIR}"/build/*)
            rm -rf "${BUILD_DIR}"
            info "removed ${BUILD_DIR}"
            ;;
        *)
            die "refusing to remove build directory outside ${ROOT_DIR}/build: ${BUILD_DIR}"
            ;;
    esac
}

parse_options()
{
    while [[ $# -gt 0 ]]; do
        case "$1" in
            --build-dir)
                [[ $# -ge 2 ]] || die "--build-dir requires a value"
                BUILD_DIR="$2"
                shift 2
                ;;
            --build-type)
                [[ $# -ge 2 ]] || die "--build-type requires a value"
                BUILD_TYPE="$2"
                shift 2
                ;;
            --smp)
                [[ $# -ge 2 ]] || die "--smp requires a value"
                SMP="$2"
                shift 2
                ;;
            --memory)
                [[ $# -ge 2 ]] || die "--memory requires a value"
                MEMORY="$2"
                shift 2
                ;;
            --machine)
                [[ $# -ge 2 ]] || die "--machine requires a value"
                MACHINE="$2"
                shift 2
                ;;
            --cpu)
                [[ $# -ge 2 ]] || die "--cpu requires a value"
                CPU="$2"
                shift 2
                ;;
            --timeout)
                [[ $# -ge 2 ]] || die "--timeout requires a value"
                TIMEOUT_SECONDS="$2"
                shift 2
                ;;
            --gdb-port)
                [[ $# -ge 2 ]] || die "--gdb-port requires a value"
                GDB_PORT="$2"
                shift 2
                ;;
            --no-build)
                NO_BUILD=1
                shift
                ;;
            --reconfigure)
                RECONFIGURE=1
                shift
                ;;
            --no-console)
                CONSOLE=0
                shift
                ;;
            --server-only)
                SERVER_ONLY=1
                shift
                ;;
            --break)
                [[ $# -ge 2 ]] || die "--break requires a symbol"
                BREAK_SYMBOL="$2"
                shift 2
                ;;
            --)
                shift
                QEMU_EXTRA=("$@")
                return
                ;;
            -h|--help)
                usage
                exit 0
                ;;
            *)
                die "unknown option: $1"
                ;;
        esac
    done
}

main()
{
    local command="${1:-help}"

    if [[ $# -gt 0 ]]; then
        shift
    fi

    parse_options "$@"

    case "${command}" in
        env)
            print_env
            ;;
        configure)
            configure
            ;;
        build)
            build
            ;;
        run)
            run_qemu
            ;;
        debug)
            debug_qemu
            ;;
        clean)
            clean
            ;;
        help|-h|--help)
            usage
            ;;
        *)
            die "unknown command: ${command}"
            ;;
    esac
}

main "$@"
