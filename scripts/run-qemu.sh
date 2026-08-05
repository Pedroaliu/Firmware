#!/usr/bin/env sh
set -eu

QEMU=${QEMU:-qemu-system-riscv64}
IMAGE=${1:-build/qemu-virt/archfw.bin}

exec "$QEMU" \
    -machine virt \
    -m 128M \
    -smp 1 \
    -nographic \
    -bios "$IMAGE"
