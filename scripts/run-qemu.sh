#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

readonly BUILD_DIR="${1:-build}"
readonly FIRMWARE="${ROOT_DIR}/${BUILD_DIR}/archfw.bin"

if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware image not found: ${FIRMWARE}" >&2
    echo "Build ArchFW before running QEMU." >&2
    exit 1
fi

exec qemu-system-riscv64 \
    -machine virt \
    -cpu rv64 \
    -m 128M \
    -smp 1 \
    -bios "${FIRMWARE}" \
    -display none \
    -serial stdio \
    -monitor none