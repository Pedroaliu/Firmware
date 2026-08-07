# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-07
- **Working mode:** solo development with ChatGPT research/review/implementation support
- **Progress branch:** `feature/console-foundation`
- **Stable integration branch:** `main`
- **Current work item:** `F00-01 Kernel print foundation`
- **Current status:** ACTIVE — implementation prepared; GNU RISC-V/QEMU acceptance pending
- **Canonical Console design:** `docs/JIXIA_CONSOLE_DESIGN.md`
- **Parked work:** `M00-04 Timer interrupt` on `milestone/m00-04-timer-interrupt` at `299aff177497399236a848724b56c2e040ce4db4`

## Status legend

| Status | Meaning |
|---|---|
| `DONE` | Definition of Done satisfied and evidence recorded |
| `ACTIVE` | the single current primary work item |
| `PAUSED` | implementation preserved but deliberately not current work |
| `NEXT` | ordered immediately after ACTIVE |
| `PLANNED` | accepted roadmap item, not started |
| `FROZEN` | deliberately blocked by an architectural prerequisite |
| `RESEARCH` | exploratory work without an implementation commitment |

## Milestone / feature ledger

| Work item | Status | Evidence | Notes |
|---|---|---|---|
| M00-00 Minimal RV64 boot, stack, BSS, UART | DONE | `c30c0405b388a0fba4c528856236ff02267f1a77` | QEMU virt reset entry and initial firmware output |
| M00-01 Minimal fatal M-mode trap | DONE | `ce661a8c1f1798861cab2ef766749cae38bcdc69` | `mtvec`, `mcause`, `mepc`, and `mtval`; non-resumable |
| Architecture v0.1 | DONE | `1fe8fbcf16a477fc921e7b5aac7be066d13c65b2` | original ArchFW architecture record |
| LPAR/CECSIM co-design direction | DONE | `9f5422939fa5501811135170a1c8633ef18842f3` | long-term firmware-native partition and simulator direction |
| Jixia naming and persistent context | DONE | `6c6769adb8f1aa9c6e1b6f4afb9d3800b5d22433` | Jixia identity and project memory established |
| Semantic paths and `jixia::*` namespaces | DONE | `df8bc2d6bd32d1b13b659e5e33629d24c1488bc2` | semantic source paths and C++ namespace boundary |
| Solo development roadmap | DONE | `1d9e1cfb9be782b5e7ad44d21b41600f021fd597` | single-threaded project execution and feature gates |
| Freestanding C++ compatibility fix | DONE | `7d8a66f4dbac12e6196d0fbbf3a28932647bbd0e` | GNU bare-metal build and QEMU revalidated |
| M00-02 Complete RV64 TrapFrame | DONE | `bash scripts/test-trap-frame.sh` on 2026-08-07 | complete integer context and `TRAP_FRAME_TEST: PASS` |
| M00-03 Recoverable trap and `mret` | DONE | `bash scripts/test-recoverable-trap.sh` on 2026-08-07 | 32-bit `EBREAK` and 16-bit `C.EBREAK` both resume through common restore + `mret` |
| F00-01 Kernel print foundation | ACTIVE | branch `feature/console-foundation`; QEMU acceptance pending | `printk`, shared formatter, 36 KiB append-only kernel log, temporary raw-UART mirror |
| Future usr Console Service | PLANNED | design recorded in `docs/JIXIA_CONSOLE_DESIGN.md` | queue/daemon, logical channels, UART/screen/SOL/Jingjie backends deferred until services exist |
| M00-04 Timer interrupt | PAUSED | branch `milestone/m00-04-timer-interrupt` at `299aff177497399236a848724b56c2e040ce4db4` | timer implementation preserved separately; resume after F00-01 acceptance |
| M00-05 Per-hart state and stacks | NEXT | pending | required before multicore work and kernel-print concurrency policy |
| M00-06 Privilege transition foundation | NEXT | pending | remains in firmware-first phase |
| M00-07 Early physical allocator | PLANNED | pending | supports later service/memory work |
| M00-08 Structured event and trace ABI | PLANNED | pending | shared later with Jingjie |
| M00-09 Automated QEMU test harness | PLANNED | pending | broader machine-checkable regression tests |

## DONE: M00-02 Complete RV64 TrapFrame

### Objective

Create a precise, shared RV64 trap-context representation that can later support recoverable exceptions, interrupts, service isolation, context switching, vCPU state, debug, and RAS evidence.

### Acceptance evidence

- `TrapFrame` contains x0-x31, `mstatus`, `mepc`, `mcause`, and `mtval`.
- RV64 frame size, alignment, and assembly/C++ offsets are compile-time checked.
- Trap entry saves the complete integer context and trap restore reconstructs it before `mret`.
- The known-register test validates fixed GPR patterns plus live `sp`, `gp`, and `tp` snapshots.
- `bash scripts/test-trap-frame.sh` produced `TRAP_FRAME_TEST: PASS` on 2026-08-07.
- Design record: `docs/JIXIA_M00_02_TRAP_FRAME.md`.

## DONE: M00-03 Recoverable trap and `mret`

### Objective

Turn the fatal trap path into a deliberately recoverable path for explicitly recognized software breakpoint instructions while preserving fail-closed behavior for unsupported trap causes.

### Design decisions

- Trap recovery is whitelist-based: unsupported interrupts/exceptions remain fatal.
- `TrapFrame` is the authoritative software recovery state.
- `mcause` code alone is insufficient; the interrupt bit and code are interpreted together.
- Cause code 3 is not assumed to mean an executable EBREAK instruction; the handler verifies the instruction at `mepc`.
- Instruction length is decoded from the instruction stream; standard EBREAK advances by 4 and C.EBREAK by 2.

### Acceptance evidence

`bash scripts/test-recoverable-trap.sh` on 2026-08-07 produced:

```text
standard   : resumed after 32-bit EBREAK
compressed : resumed after 16-bit C.EBREAK
RECOVERABLE_TRAP_TEST: PASS
TRAP_FRAME_TEST: PASS
Recoverable trap test: PASS
```

## NOW: F00-01 Kernel print foundation

### Why the scope changed

The first Console prototype put router, multiple sinks, stream syntax, memory ring, UART backend and future service concepts directly under `microkernel/console`.

After studying the supplied Hostboot kernel and `usr/console` implementations, the accepted boundary is narrower:

```text
NOW: microkernel
    printk
    shared formatter
    append-only KernelLogBuffer
    temporary raw-UART mirror

LATER: usr/service runtime
    display/displayf/console::out
    queue + daemon
    logical DEFAULT/DEBUG channels
    UART/screen/SOL/Jingjie routing
```

The full decision record and future TODOs are in `docs/JIXIA_CONSOLE_DESIGN.md`. Future Console work should resume from that file rather than repeating the source study.

### Hostboot src.zip library review

Reviewed supplied `src.zip`:

- use the **design idea** from `lib/sprintf.C`: one formatter writing through a generic character receiver;
- defer `lib/stdio.C` until Jixia actually needs `sprintf/snprintf`;
- defer string/ctype/assert support until a real minimal libc/panic contract is needed;
- do not import Hostboot `stdlib.C`, sync/syscall/TLS, or C++ runtime files because they depend on Hostboot heap/VMM/task/runtime semantics;
- unrelated math/random/crc/splaytree code is not part of Kernel Print.

### Current implementation

```text
lib/format.{h,cpp}
    freestanding formatter and generic Writer

microkernel/console/kernel_console.{h,cpp}
    36 KiB fixed append-only log
    truncation flag
    temporary raw-UART mirror

microkernel/console/printk.{h,cpp}
    Mozi printf-style kernel frontend

microkernel/core/kernel_print_test.cpp
    exact format + in-memory byte validation

scripts/test-kernel-print.sh
    QEMU acceptance + M00-03/M00-02 regression
```

Initial formatter supports:

```text
%% %c %s
%d %i %u %o
%x %X %b %B %p
hh h l ll z t
# 0 - + space
field width
```

No floating point, precision, locale, hosted stdio or full ISO-C printf claim.

### Current invariants

- kernel print is independent of any future Console Service;
- formatter has no UART knowledge;
- normal microkernel/test code uses `printk`, not direct `uart_puts`;
- kernel memory log is authoritative;
- UART is only a bring-up mirror;
- raw `uart_putc` remains below the formatted path;
- buffer is append-only, not a ring;
- no heap/task/IPC/iostream/exceptions/RTTI/runtime-static-constructor dependency;
- initial implementation is single-hart/non-locking;
- timer code is not part of this branch.

### Acceptance command

```bash
bash scripts/test-kernel-print.sh
```

Required markers:

```text
[Jixia][Test][KernelPrint]
probe      : s=ok d=-42 u=42 x=00001a2b p=0x0000000000001234 %
buffer     : append-only kernel log retained exact probe
capacity   : 36864 bytes
KERNEL_PRINT_TEST: PASS
RECOVERABLE_TRAP_TEST: PASS
TRAP_FRAME_TEST: PASS
Kernel print test: PASS
```

### Remaining F00-01 work

```text
[x] settle kernel-vs-usr Console boundary
[x] record future usr Console Service architecture/TODO
[x] review supplied Hostboot src.zip libraries
[x] implement shared formatter candidate
[x] implement printk candidate
[x] implement append-only KernelLogBuffer candidate
[x] preserve raw UART as lower-level primitive
[x] migrate normal kernel/test output to printk
[x] add exact KernelLogBuffer format test
[x] add dedicated QEMU acceptance script
[x] compile new Kernel Print sources with Clang RV64 bare-metal target and -Wall -Wextra -Werror
[ ] build on user's GNU riscv64-unknown-elf toolchain
[ ] run `bash scripts/test-kernel-print.sh`
[ ] record final QEMU acceptance evidence
```

## PAUSED: M00-04 Timer interrupt

Timer work is preserved at:

```text
milestone/m00-04-timer-interrupt
299aff177497399236a848724b56c2e040ce4db4
```

After F00-01 is accepted, resume M00-04 on top of the accepted Kernel Print foundation.

Key timer invariants remain:

- `mcause` = interrupt=true/code=7;
- asynchronous timer interrupt does not artificially advance `mepc`;
- timer pending condition is cleared/rearmed before return;
- common TrapFrame restore + `mret` remains the return path;
- single-hart until M00-05.

## NEXT queue

1. finish/accept `F00-01 Kernel print foundation`
2. resume and accept `M00-04 Timer interrupt`
3. `M00-05 Per-hart state and stacks`
4. `M00-06 Privilege transition foundation`
5. `M00-07 Early physical allocator`

Only one is ACTIVE at a time.

## Frozen implementation areas

```text
FROZEN  ArchHV and LPAR runtime
FROZEN  HS/VS execution and G-stage translation
FROZEN  virtual interrupt and virtual I/O
FROZEN  Service LPAR
FROZEN  confidential LPAR runtime
FROZEN  migration
FROZEN  simulator-dependent partition hardware experiments
```

## Progress update protocol

When a work item is completed, append:

```markdown
### YYYY-MM-DD — <work item> completed

- Status: DONE
- Branch/PR:
- Commit:
- Test command:
- Test result:
- What was learned:
- Design decisions:
- Known limitations:
- Documentation updated:
- Next ACTIVE work item:
```

Then update the ledger, NOW section, and `PROJECT_CONTEXT.md` when the current branch or architecture direction changes.

## Progress history

### 2026-08-07 — Console scope split into Kernel Print and future usr service

- Status: ACTIVE design decision
- Branch: `feature/console-foundation`
- Decision: current work is only the Mozi Kernel Print foundation.
- Source study: Hostboot kernel console, Hostboot `usr/console`, and supplied `src.zip` libraries reviewed.
- Key boundary: kernel `printk`/buffer remains independent of the future queue/daemon/device Console Service.
- Buffer policy: current kernel history is 36 KiB append-only rather than a runtime ring.
- Library policy: implement only a small Jixia formatter now; do not import Hostboot libc/runtime wholesale.
- Canonical design/TODO: `docs/JIXIA_CONSOLE_DESIGN.md`.
- M00-04 remains PAUSED until Kernel Print is accepted.

### 2026-08-07 — M00-03 Recoverable trap and `mret` completed

- Status: DONE
- Branch: `milestone/m00-03-recoverable-trap`
- Test command: `bash scripts/test-recoverable-trap.sh`
- Test result: standard 32-bit EBREAK resumed, 16-bit C.EBREAK resumed, `RECOVERABLE_TRAP_TEST: PASS`, `TRAP_FRAME_TEST: PASS`, final `Recoverable trap test: PASS`.
- Next work later advanced to M00-04, then M00-04 was paused while Console/Kernel Print was separated cleanly.

### 2026-08-07 — M00-02 Complete RV64 TrapFrame completed

- Status: DONE
- Test command: `bash scripts/test-trap-frame.sh`
- Test result: `TRAP_FRAME_TEST: PASS`

### 2026-08-05 — freestanding C++ compatibility and QEMU revalidation

- Status: DONE
- Build: passed on user's Ubuntu workstation with `riscv64-unknown-elf-gcc/g++`.
- QEMU: Jixia entered the microkernel and observed expected breakpoint state.

### 2026-08-05 — solo development process established

- Status: DONE
- Decision: one active milestone/work item at a time; no parallel major subsystem development.
