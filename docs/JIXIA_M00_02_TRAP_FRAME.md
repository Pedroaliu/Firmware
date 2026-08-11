# M00-02 — RV64 TrapFrame Design and Test Notes

## Objective

M00-02 defines one precise machine-mode trap-context ABI shared by RISC-V assembly and freestanding C++. The representation is intended to become the foundation for recoverable traps, interrupts, context switching, debug state, RAS evidence, and later vCPU state.

This milestone is limited to the RV64 integer register context. Floating-point, vector, debug, hypervisor, and lower-privilege state are not part of this frame.

## Frame layout

The frame is allocated on the interrupted stack and is 16-byte aligned when the interrupted stack satisfies the RV64 ABI alignment rule.

```text
offset 0x000..0x0f8  x[0]..x[31]
offset 0x100         mstatus
offset 0x108         mepc
offset 0x110         mcause
offset 0x118         mtval
size   0x120         288 bytes
align  0x010         16 bytes
```

`x[n]` always corresponds directly to architectural register `xn`:

- `x[0]` is explicitly written as zero;
- `x[2]` contains the interrupted value of `sp`, not the frame base;
- all other integer registers contain their values at trap entry.

The numeric offsets and the C++ `TrapFrame` definition live in `microkernel/arch/riscv/trap_frame.h`. Compile-time assertions check the size, alignment, CSR offsets, and all 32 GPR offsets.

## Entry invariants

The current entry path follows this order:

1. subtract `JIXIA_TRAP_FRAME_SIZE` from `sp`;
2. save interrupted `t0` before using it as scratch state;
3. reconstruct interrupted `sp` as `frame_sp + frame_size`;
4. save all remaining integer registers;
5. read and save `mstatus`, `mepc`, `mcause`, and `mtval`;
6. pass the frame base in `a0` to `jixia_trap_dispatch`.

The early `t0` save is mandatory. Using `t0` before preserving it would destroy part of the interrupted context.

## Restore invariants

The restore path:

1. restores `mstatus` and `mepc`;
2. restores every integer register except `sp`;
3. performs no further frame access after loading interrupted `sp`;
4. executes `mret`.

`mcause` and `mtval` are evidence describing the trap and are not restored for return.

The restore implementation is present in M00-02, but the normal C++ dispatcher remains non-returning. M00-03 will introduce an explicitly recoverable dispatch result and exercise the `mret` path.

## Known-register test

The test stimulus is implemented in `trap_frame_test.S`.

Before EBREAK it:

- snapshots live `sp`, `gp`, and `tp` values;
- loads distinct 64-bit patterns into `ra`, `t0`–`t6`, `s0`–`s11`, and `a0`–`a7`;
- executes an explicit 32-bit EBREAK.

The C++ validator checks:

- frame alignment;
- `x0 == 0`;
- exact saved `sp`, `gp`, and `tp` snapshots;
- every fixed GPR pattern;
- exception rather than interrupt;
- `mcause` code 3;
- `mtval == 0`.

It emits exactly one machine-readable result marker:

```text
TRAP_FRAME_TEST: PASS
```

or:

```text
TRAP_FRAME_TEST: FAIL
```

Run it with:

```bash
bash scripts/test-trap-frame.sh build
```

For the CMake preset build directory:

```bash
bash scripts/test-trap-frame.sh build/clion-debug
```

The script rebuilds the firmware, runs QEMU for a bounded interval, captures UART output in `trap-frame-test.log`, and fails unless the PASS marker is present and no FAIL marker is present.

## Current limitations and policy

M00-02 intentionally retains these limitations:

- one boot hart only;
- interrupted stack is reused as the trap stack;
- no stack overflow or guard-page detection;
- no nested-trap recovery policy;
- machine interrupts remain disabled during the test;
- no F/D/V extension state;
- no PMP, S-mode, HS-mode, VS-mode, or virtualization state;
- dispatcher is fatal/non-returning;
- restore and `mret` are structurally present but not yet exercised.

Until per-hart state and guarded trap stacks exist, any trap taken while the fatal/test handler is running is considered unrecoverable.

## Completion evidence still required

M00-02 is not DONE until the GNU RISC-V build and QEMU test are run on the development workstation and the exact command, PASS output, commit, and known limitations are recorded in `docs/JIXIA_PROGRESS.md`.
