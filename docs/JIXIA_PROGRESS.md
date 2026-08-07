# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-07
- **Working mode:** solo development with ChatGPT research/review/implementation support
- **Progress branch:** `feature/console-foundation`
- **Stable integration branch:** `main`
- **Current work item:** `F00-01 Console foundation`
- **Current status:** ACTIVE — establish a standalone firmware Console architecture and implementation
- **Parked work:** `M00-04 Timer interrupt` on `milestone/m00-04-timer-interrupt` at `299aff177497399236a848724b56c2e040ce4db4`

## Status legend

| Status | Meaning |
|---|---|
| `DONE` | Definition of Done satisfied and evidence recorded |
| `ACTIVE` | the single current primary work item |
| `PAUSED` | implementation is preserved but deliberately not current work |
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
| F00-01 Console foundation | ACTIVE | branch `feature/console-foundation`; QEMU acceptance pending | standalone output architecture: stream, router, memory sink, UART sink, emergency route |
| M00-04 Timer interrupt | PAUSED | branch `milestone/m00-04-timer-interrupt` at `299aff177497399236a848724b56c2e040ce4db4` | timer implementation preserved separately; resume after Console integration |
| M00-05 Per-hart state and stacks | NEXT | pending | required before multicore work |
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

## NOW: F00-01 Console foundation

### Why this is separate from M00-04

Console and machine-timer interrupt are independent platform features. Mixing them made the M00-04 branch contain both an asynchronous-trap mechanism and a large cross-cutting output architecture, making review, regression attribution, and later integration unnecessarily confusing.

The split is now explicit:

```text
feature/console-foundation
    based on completed M00-03
    contains Console only

milestone/m00-04-timer-interrupt
    timer-only work preserved at 299aff177497399236a848724b56c2e040ce4db4
    paused until Console is accepted
```

A safety backup of the earlier mixed history is retained as `backup/m00-04-timer-console-mixed`.

### Objective

Replace direct UART use in normal firmware output with a small freestanding Console architecture that supports multiple output sinks without introducing the hosted C++ runtime.

### Architecture

```text
console::out / future printk / future log frontend
                    |
                    v
                 Formatter
                    |
                    v
               ConsoleRouter
              /      |       \
        MemorySink UartSink future sinks
                              |
                         screen / SOL / Jingjie
```

Console text and future structured RAS/event records may share transports, but they are not the same data model.

### Work breakdown

```text
[x] write standalone Console architecture design
[x] separate Console branch from timer branch
[x] retain raw polling UART below Console
[x] implement lightweight ConsoleSink descriptor
[x] implement fixed-capacity ConsoleRouter
[x] implement normal and emergency routing
[x] implement 36 KiB static memory ring sink
[x] implement lightweight integer/hex/pointer formatting
[x] implement console::out and console::emergency stream frontends
[x] implement QEMU polling-UART sink
[x] migrate normal firmware banner output to Console
[x] migrate M00-03 test output to Console
[x] migrate M00-02 TrapFrame diagnostics to Console
[x] add dedicated Console memory/UART acceptance test
[x] add `scripts/test-console.sh`
[ ] build with the user's GNU RISC-V bare-metal toolchain
[ ] run `bash scripts/test-console.sh`
[ ] rerun M00-03 recoverable-trap regression
[ ] rerun M00-02 TrapFrame regression
[ ] record acceptance evidence
```

### Initial invariants

- `microkernel/console` has no QEMU MMIO knowledge.
- UART is one sink, not the Console abstraction.
- The memory sink is first-class diagnostic storage.
- No heap allocation is used.
- No `std::iostream`, exceptions, RTTI, or runtime static constructors are required.
- Raw UART remains usable before Console initialization and in the lowest bring-up/fatal path.
- Emergency routing reaches only sinks marked panic-safe.
- Initial memory-ring implementation is single-writer/single-hart; multi-hart ownership is deferred until per-hart state exists.
- No timer implementation is present in or required by the Console branch.

### Acceptance command

```bash
bash scripts/test-console.sh
```

Required markers:

```text
console-memory-probe
memory     : ring sink retained exact probe
CONSOLE_TEST: PASS
RECOVERABLE_TRAP_TEST: PASS
TRAP_FRAME_TEST: PASS
Console test: PASS
```

Canonical design record: `docs/JIXIA_CONSOLE_DESIGN.md`.

## PAUSED: M00-04 Timer interrupt

### Preserved implementation

Timer work is not discarded. The clean timer-only branch is:

```text
milestone/m00-04-timer-interrupt
commit 299aff177497399236a848724b56c2e040ce4db4
```

It already contains the first candidate implementation and dedicated QEMU acceptance script, but it has not yet been accepted on the user's QEMU/toolchain.

### Objective when resumed

Add the first recoverable asynchronous trap: a machine timer interrupt that is deliberately armed, recognized as interrupt code 7, serviced/rearmed, and returned through the existing common TrapFrame restore and `mret` path.

### Key invariants when resumed

- `mcause` must be **interrupt=true, code=7**.
- Timer interrupt handling must not apply the synchronous EBREAK `mepc += instruction_length` rule.
- The pending timer condition must be cleared/rearmed before return.
- `trap.S` remains the common entry/restore path unless evidence proves otherwise.
- M00-04 remains single-hart; per-hart timer/state belongs to M00-05.

After Console is accepted, M00-04 should be rebased/reapplied on top of the accepted Console foundation and then independently validated.

## NEXT queue

1. finish and accept `F00-01 Console foundation`;
2. resume and accept `M00-04 Timer interrupt`;
3. `M00-05 Per-hart state and stacks`;
4. `M00-06 Privilege transition foundation`.

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

They are released only through the gates in `docs/JIXIA_SOLO_ROADMAP.md`.

## Progress update protocol

When a work item is completed, record:

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

Then update this ledger and `PROJECT_CONTEXT.md`.

## Progress history

### 2026-08-07 — Console separated from timer work

- Status: ACTIVE feature split.
- Decision: Console is an independent cross-cutting platform feature; it is not part of M00-04 Timer interrupt.
- Active branch: `feature/console-foundation`, based on completed M00-03.
- Timer branch: `milestone/m00-04-timer-interrupt`, preserved at `299aff177497399236a848724b56c2e040ce4db4`.
- Safety backup: `backup/m00-04-timer-console-mixed` retains the earlier mixed branch history.
- Reason: keep feature boundaries, regression evidence, and later review clean.
- Next evidence: GNU RISC-V build + `bash scripts/test-console.sh`.

### 2026-08-07 — M00-03 Recoverable trap and `mret` completed

- Status: DONE
- Branch: `milestone/m00-03-recoverable-trap`
- Test command: `bash scripts/test-recoverable-trap.sh`
- Test result: standard 32-bit EBREAK resumed, 16-bit C.EBREAK resumed, `RECOVERABLE_TRAP_TEST: PASS`, `TRAP_FRAME_TEST: PASS`, and final `Recoverable trap test: PASS`.
- What was learned: trap recovery is software policy over saved architectural state; synchronous breakpoint recovery must distinguish instruction encoding/length from the trap cause itself.
- Known limitations: no accepted asynchronous interrupt handling yet, no per-hart trap stack/state, no nested-trap policy.

### 2026-08-07 — M00-02 Complete RV64 TrapFrame completed

- Status: DONE
- Branch: `milestone/m00-02-trap-frame`
- Test command: `bash scripts/test-trap-frame.sh`
- Test result: `TRAP_FRAME_TEST: PASS`
- What was learned: the trap frame is a software ABI between trap assembly and higher-level policy; hardware knows architectural registers/CSRs, not the C++ `TrapFrame` type.

### 2026-08-05 — freestanding C++ compatibility and QEMU revalidation

- Status: DONE
- Build result: passed on the user's Ubuntu workstation with `riscv64-unknown-elf-gcc/g++`.
- QEMU result: Jixia entered the microkernel and observed the expected breakpoint exception state.

### 2026-08-05 — solo development process established

- Status: DONE
- Roadmap commit: `1d9e1cfb9be782b5e7ad44d21b41600f021fd597`
- Decision: one active primary work item at a time; no parallel major subsystem development.
