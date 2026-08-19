#!/usr/bin/env bash
#
# Jixia developer environment bootstrap.
#
# Default behavior:
#   - inspect the local machine
#   - install only missing dependencies on Debian/Ubuntu/Deepin/UOS
#   - verify the toolchain after installation
#
# Usage:
#   bash scripts/setup-dev-env.sh
#   bash scripts/setup-dev-env.sh --check
#   bash scripts/setup-dev-env.sh --yes
#   bash scripts/setup-dev-env.sh --without-gdb
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

MODE="install"
ASSUME_YES=0
WITH_GDB=1

usage()
{
    cat <<'EOF'
Jixia developer environment setup

Usage:
  setup-dev-env.sh [options]

Options:
  --check          Check only; never install packages.
  -y, --yes        Install missing packages without prompting.
  --without-gdb    Do not require/install a debugger.
  -h, --help       Show this help.

Supported automatic installation:
  Debian / Ubuntu / Deepin / UOS (APT)

Required tools:
  git
  cmake >= 3.20
  ninja
  python3
  clang-format
  timeout
  qemu-system-riscv64
  riscv64-unknown-elf-gcc / g++
  riscv64-unknown-elf binutils

Debugger:
  gdb-multiarch (or riscv64-unknown-elf-gdb if already installed)

APT safeguards:
  JIXIA_APT_UPDATE_TIMEOUT_SECONDS  Overall apt-get update limit (default: 300)
  JIXIA_APT_INSTALL_TIMEOUT_SECONDS Overall apt-get install limit (default: 900)
EOF
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            MODE="check"
            ;;
        -y|--yes)
            ASSUME_YES=1
            ;;
        --without-gdb)
            WITH_GDB=0
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "ERROR: unknown option: $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

info()
{
    printf '[Jixia setup] %s\n' "$*"
}

warn()
{
    printf '[Jixia setup][WARN] %s\n' "$*" >&2
}

die()
{
    printf '[Jixia setup][ERROR] %s\n' "$*" >&2
    exit 1
}

have()
{
    command -v "$1" >/dev/null 2>&1
}

version_ge()
{
    # version_ge ACTUAL REQUIRED
    [[ "$(printf '%s\n%s\n' "$2" "$1" | sort -V | head -n1)" == "$2" ]]
}

read_os_release()
{
    if [[ ! -r /etc/os-release ]]; then
        die "cannot read /etc/os-release"
    fi

    # shellcheck disable=SC1091
    . /etc/os-release

    DISTRO_ID="${ID:-unknown}"
    DISTRO_LIKE="${ID_LIKE:-}"
    DISTRO_NAME="${PRETTY_NAME:-${DISTRO_ID}}"
}

declare -a MISSING_COMMANDS=()
declare -a MISSING_PACKAGES=()

add_missing()
{
    local command_name="$1"
    local package_name="$2"

    if ! have "${command_name}"; then
        MISSING_COMMANDS+=("${command_name}")
        MISSING_PACKAGES+=("${package_name}")
    fi
}

dedupe_packages()
{
    local -A seen=()
    local -a unique=()
    local package

    for package in "${MISSING_PACKAGES[@]}"; do
        if [[ -z "${seen[${package}]+x}" ]]; then
            unique+=("${package}")
            seen["${package}"]=1
        fi
    done

    MISSING_PACKAGES=("${unique[@]}")
}

check_tools()
{
    MISSING_COMMANDS=()
    MISSING_PACKAGES=()

    add_missing git git
    add_missing cmake cmake
    add_missing ninja ninja-build
    add_missing python3 python3
    add_missing clang-format clang-format
    add_missing timeout coreutils

    # Debian-family qemu-system-riscv64 is normally provided by
    # qemu-system-misc on the distributions targeted by this bootstrap.
    add_missing qemu-system-riscv64 qemu-system-misc

    # Bare-metal RISC-V compiler and binary utilities.
    add_missing riscv64-unknown-elf-gcc gcc-riscv64-unknown-elf
    add_missing riscv64-unknown-elf-g++ gcc-riscv64-unknown-elf
    add_missing riscv64-unknown-elf-objcopy binutils-riscv64-unknown-elf
    add_missing riscv64-unknown-elf-objdump binutils-riscv64-unknown-elf
    add_missing riscv64-unknown-elf-readelf binutils-riscv64-unknown-elf

    if [[ ${WITH_GDB} -eq 1 ]]; then
        if ! have gdb-multiarch && ! have riscv64-unknown-elf-gdb; then
            MISSING_COMMANDS+=("gdb-multiarch")
            MISSING_PACKAGES+=("gdb-multiarch")
        fi
    fi

    dedupe_packages
}

check_versions()
{
    if have cmake; then
        local cmake_version
        cmake_version="$(cmake --version | awk 'NR==1 {print $3}')"

        if ! version_ge "${cmake_version}" "3.20"; then
            die "CMake ${cmake_version} is too old; Jixia requires >= 3.20"
        fi
    fi
}

print_detected_versions()
{
    echo
    info "tool versions"

    have cmake && cmake --version | head -n1 || true
    have ninja && ninja --version | sed 's/^/ninja /' || true
    have clang-format && clang-format --version | head -n1 || true
    have qemu-system-riscv64 && qemu-system-riscv64 --version | head -n1 || true
    have riscv64-unknown-elf-gcc && riscv64-unknown-elf-gcc --version | head -n1 || true
    have riscv64-unknown-elf-g++ && riscv64-unknown-elf-g++ --version | head -n1 || true

    if have gdb-multiarch; then
        gdb-multiarch --version | head -n1
    elif have riscv64-unknown-elf-gdb; then
        riscv64-unknown-elf-gdb --version | head -n1
    fi
}

positive_integer()
{
    [[ "$1" =~ ^[1-9][0-9]*$ ]]
}

install_apt_packages()
{
    have apt-get || die "APT is not available"

    if [[ ${#MISSING_PACKAGES[@]} -eq 0 ]]; then
        return
    fi

    echo
    info "missing packages:"
    printf '  %s\n' "${MISSING_PACKAGES[@]}"

    if [[ ${MODE} == "check" ]]; then
        return
    fi

    if [[ ${ASSUME_YES} -eq 0 ]]; then
        echo
        read -r -p "Install these packages with sudo apt-get? [Y/n] " answer
        case "${answer}" in
            ""|y|Y|yes|YES)
                ;;
            *)
                die "installation cancelled"
                ;;
        esac
    fi

    local -a sudo_cmd=()
    if [[ ${EUID} -ne 0 ]]; then
        have sudo || die "sudo is required for package installation"
        sudo_cmd=(sudo)
    fi

    local update_timeout="${JIXIA_APT_UPDATE_TIMEOUT_SECONDS:-300}"
    local install_timeout="${JIXIA_APT_INSTALL_TIMEOUT_SECONDS:-900}"
    if ! positive_integer "${update_timeout}"; then
        die "JIXIA_APT_UPDATE_TIMEOUT_SECONDS must be a positive integer"
    fi
    if ! positive_integer "${install_timeout}"; then
        die "JIXIA_APT_INSTALL_TIMEOUT_SECONDS must be a positive integer"
    fi

    local -a apt_options=(
        -o Acquire::Retries=3
        -o Acquire::http::Timeout=20
        -o Acquire::https::Timeout=20
        -o Acquire::ForceIPv4=true
        -o Dpkg::Use-Pty=0
    )

    info "running apt-get update (overall timeout: ${update_timeout}s)"
    "${sudo_cmd[@]}" timeout --signal=TERM --kill-after=10s "${update_timeout}s" \
        apt-get "${apt_options[@]}" update

    info "installing packages (overall timeout: ${install_timeout}s)"
    "${sudo_cmd[@]}" timeout --signal=TERM --kill-after=10s "${install_timeout}s" \
        apt-get "${apt_options[@]}" install -y --no-install-recommends \
        "${MISSING_PACKAGES[@]}"
}

main()
{
    read_os_release

    info "repository: ${ROOT_DIR}"
    info "host: ${DISTRO_NAME}"

    check_tools

    case "${DISTRO_ID}:${DISTRO_LIKE}" in
        deepin:*|debian:*|ubuntu:*|uos:*|*:debian*|*:ubuntu*)
            ;;
        *)
            if [[ ${#MISSING_COMMANDS[@]} -gt 0 ]]; then
                warn "automatic installation is not implemented for ${DISTRO_NAME}"
                warn "missing commands: ${MISSING_COMMANDS[*]}"
                exit 1
            fi
            ;;
    esac

    if [[ ${#MISSING_COMMANDS[@]} -eq 0 ]]; then
        check_versions
        info "all required tools are already present"
        print_detected_versions
        exit 0
    fi

    info "missing commands:"
    printf '  %s\n' "${MISSING_COMMANDS[@]}"

    if [[ ${MODE} == "check" ]]; then
        echo
        info "check-only mode; nothing was installed"
        exit 1
    fi

    install_apt_packages

    check_tools

    if [[ ${#MISSING_COMMANDS[@]} -gt 0 ]]; then
        die "setup incomplete; still missing: ${MISSING_COMMANDS[*]}"
    fi

    check_versions
    print_detected_versions

    echo
    info "environment is ready"
    info "next: bash scripts/jixia.sh build"
}

main "$@"
