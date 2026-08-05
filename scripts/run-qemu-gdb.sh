#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

readonly BUILD_DIR="${1:-build}"
readonly GDB_PORT="${2:-1234}"
readonly FIRMWARE="${ROOT_DIR}/${BUILD_DIR}/jixia.bin"

if [[ ! -f "${FIRMWARE}" ]]; then
    echo "Firmware image not found: ${FIRMWARE}" >&2
    echo "Build Jixia firmware before starting the QEMU GDB server." >&2
    exit 1
fi

echo "Jixia QEMU GDB server"
echo "  firmware : ${FIRMWARE}"
echo "  endpoint : 127.0.0.1:${GDB_PORT}"
echo "  state    : CPU halted; waiting for GDB"

exec qemu-system-riscv64 \
    -machine virt \
    -cpu rv64 \
    -m 128M \
    -smp 1 \
    -bios "${FIRMWARE}" \
    -display none \
    -serial stdio \
    -monitor none \
    -S \
    -gdb "tcp::${GDB_PORT}"
