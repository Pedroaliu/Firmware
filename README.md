# ArchFW M00: QEMU UART Bootstrap

This branch contains the first executable ArchFW milestone: a tiny
RISC-V M-mode microkernel bootstrap for QEMU `virt`.

## What M00 currently does

1. starts at physical address `0x80000000`;
2. keeps hart 0 and parks secondary harts;
3. establishes `gp` and a 64 KiB boot stack;
4. clears `.bss`;
5. installs a minimal M-mode trap vector;
6. initializes QEMU's ns16550 UART at `0x10000000`;
7. prints the boot banner and enters `wfi` idle.

This is intentionally not yet a complete seL4-like kernel. There are no
TCBs, scheduler, capabilities, endpoints or VSpaces in this commit. The
purpose is to freeze and verify the machine-entry contract before adding
kernel objects.

## Build dependencies

Preferred LLVM toolchain:

```text
clang 17+
ld.lld
llvm-objcopy
qemu-system-riscv64
```

## Build and run

```sh
make
make run
```

Equivalent explicit QEMU command:

```sh
qemu-system-riscv64 \
  -machine virt \
  -m 128M \
  -smp 1 \
  -nographic \
  -bios build/qemu-virt/archfw.bin
```

Exit QEMU with `Ctrl-a x`.

Expected output begins with:

```text
ArchFW microkernel M00
======================
[archfw] phase    : KERNEL_BOOTSTRAP
[archfw] platform : qemu-riscv64-virt
...
[archfw] status   : M00 UART bootstrap complete
```

## Source layout

```text
arch/riscv64/       reset entry and trap entry
drivers/uart/       ns16550 polling console
include/            kernel and platform interfaces
kernel/             first C entry and trap panic
linker/              QEMU virt memory layout
scripts/             run helpers
docs/                milestone design notes
```

## Next implementation steps

M00.1 adds `BootInfo`, a fixed early allocator and typed `MemoryRegion`
records. M00.2 adds the first `Thread`, idle thread and event-based kernel
entry. Capability spaces and Endpoint/Notification objects follow only
after those invariants are covered by host-side tests.

The full architecture study remains in branch `archfw-architecture-v0.1`.
