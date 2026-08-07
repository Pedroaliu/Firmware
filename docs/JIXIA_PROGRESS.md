# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-07
- **Working mode:** solo development with ChatGPT research/review/implementation support
- **Stable integration branch:** `main`
- **Integration branch:** `integration/console-foundation`
- **Next development branch:** `milestone/m00-05-smp-foundation`
- **Current milestone:** `M00-05 Per-hart state, stacks, and SMP foundation`
- **Current status:** ACTIVE — first task is integrated single-hart regression, then multi-hart bring-up

## Status legend

| Status | Meaning |
|---|---|
| `DONE` | implementation accepted and evidence retained |
| `ACTIVE` | the single current primary milestone |
| `NEXT` | ordered immediately after ACTIVE |
| `PLANNED` | accepted roadmap item, not started |
| `FROZEN` | blocked by an architectural prerequisite |
| `RESEARCH` | exploratory work without an implementation commitment |

## Milestone / feature ledger

| Work item | Status | Evidence | Notes |
|---|---|---|---|
| M00-00 Minimal RV64 boot, stack, BSS, UART | DONE | `c30c0405b388a0fba4c528856236ff02267f1a77` | QEMU virt reset entry and initial firmware output |
| M00-01 Minimal fatal M-mode trap | DONE | `ce661a8c1f1798861cab2ef766749cae38bcdc69` | `mtvec`, `mcause`, `mepc`, `mtval` fatal path |
| M00-02 Complete RV64 TrapFrame | DONE | `bash scripts/test-trap-frame.sh`; `TRAP_FRAME_TEST: PASS` | complete integer context, shared assembly/C++ ABI, common save/restore path |
| M00-03 Recoverable trap and `mret` | DONE | `bash scripts/test-recoverable-trap.sh`; `RECOVERABLE_TRAP_TEST: PASS` | 32-bit `EBREAK` and 16-bit `C.EBREAK` resume through common restore + `mret` |
| M00-04 Machine timer interrupt | DONE | branch `milestone/m00-04-timer-interrupt`; integrated to `main` by PR #6; `scripts/test-timer-interrupt.sh` | first recoverable asynchronous M-mode interrupt; timer is serviced/rearmed without advancing saved `mepc` |
| F00-01 Kernel print foundation | DONE | branch `feature/console-foundation`; `scripts/test-kernel-print.sh`; user-confirmed QEMU acceptance on 2026-08-07 | shared formatter, 36 KiB append-only kernel log, temporary UART mirror, `printk` |
| Console/timer integration | ACTIVE | branch `integration/console-foundation` | preserve both timer and console paths; integrated acceptance requires all four foundation PASS markers |
| M00-05 Per-hart state, stacks, SMP foundation | ACTIVE | pending | next code milestone after integration baseline is accepted |
| M00-06 Privilege transition foundation | NEXT | pending | firmware-first privilege model |
| M00-07 Early physical allocator | PLANNED | pending | supports later service/memory work |
| M00-08 Structured event and trace ABI | PLANNED | `docs/JIXIA_TRACE_OBSERVABILITY_VISION.md` | shared later with Jingjie |
| M00-09 Automated QEMU test harness | PLANNED | pending | consolidate machine-checkable regression tests |

The temporary `Console/timer integration` row is repository-maintenance work, not a new architectural milestone. Remove or mark it DONE once the integration branch is merged.

## Accepted foundation

### M00-02 — TrapFrame

The software trap ABI contains x0-x31 plus `mstatus`, `mepc`, `mcause`, and `mtval`. Assembly and C++ share one checked layout. The known-register test produced:

```text
TRAP_FRAME_TEST: PASS
```

Design record: `docs/JIXIA_M00_02_TRAP_FRAME.md`.

### M00-03 — recoverable synchronous traps

Recovery is whitelist-based. The handler verifies `EBREAK` or `C.EBREAK` at saved `mepc`, advances by the decoded 4/2-byte length, and returns through the common restore + `mret` path.

Recorded markers:

```text
standard   : resumed after 32-bit EBREAK
compressed : resumed after 16-bit C.EBREAK
RECOVERABLE_TRAP_TEST: PASS
TRAP_FRAME_TEST: PASS
```

### M00-04 — machine timer interrupt

The timer path introduced the first asynchronous recoverable trap:

```text
mcause.interrupt = 1
mcause.code      = 7
```

Key invariant: asynchronous timer handling does **not** advance saved `mepc`. The timer condition is serviced/rearmed before return through the same TrapFrame restore path.

Source/test artifacts are retained on `main` after PR #6:

```text
microkernel/core/timer.{h,cpp}
microkernel/core/timer_interrupt_test.cpp
platform/qemu_virt/timer.{h,cpp}
scripts/test-timer-interrupt.sh
```

### F00-01 — Kernel Print

Accepted dependency split:

```text
printk
   |
shared freestanding formatter
   |
36 KiB append-only KernelLogBuffer
   |
   `---- temporary raw-UART mirror
```

The future runtime Console Service remains separate from this minimum kernel diagnostic path. See `docs/JIXIA_CONSOLE_DESIGN.md`.

The kernel-print test validates exact formatting bytes and preserves the trap regressions. During repository integration it also checks the M00-04 timer marker:

```text
KERNEL_PRINT_TEST: PASS
RECOVERABLE_TRAP_TEST: PASS
MACHINE_TIMER_TEST: PASS
TRAP_FRAME_TEST: PASS
Kernel print test: PASS
```

## NOW — M00-05 Per-hart state, stacks, and SMP foundation

### Objective

Move Mozi from a single boot hart to a correct multi-hart foundation without hard-coding socket topology into the microkernel.

### First gate: integrated baseline

Before changing SMP state, run on the integration branch:

```bash
bash scripts/test-kernel-print.sh
bash scripts/test-timer-interrupt.sh
```

Both scripts must observe the integrated foundation without a fatal trap.

### Work breakdown

```text
[ ] define maximum/boot-time hart representation without assuming socket numbering
[ ] define per-hart boot/trap stack ownership and alignment
[ ] make secondary harts use private stacks before entering C++
[ ] make exactly one boot hart perform global BSS/global initialization
[ ] define boot-hart -> secondary-hart release/rendezvous protocol
[ ] state RISC-V/C++ memory-order and fence requirements for release/observe
[ ] add HartLocal/per-hart runtime state
[ ] bring up multiple QEMU harts and prove unique per-hart state
[ ] define printk policy before allowing concurrent writers
[ ] retain M00-02/M00-03/M00-04/F00-01 regressions
[ ] separate hart identity from PlatformGraph socket/core/NUMA topology
[ ] add machine-checkable SMP acceptance evidence
```

### Initial invariants

- a hart must never execute C/C++ on another hart's stack;
- secondary harts must not race the boot hart's BSS/global initialization;
- publication of global initialization completion must have an explicit memory-order contract;
- hart ID is an architectural identifier, not a formula for socket/core/NUMA identity;
- physical topology belongs to PlatformGraph;
- avoid global shared mutable state where per-hart ownership is sufficient;
- do not add an ad-hoc spinlock simply to make `printk` appear SMP-safe; establish the required ownership/synchronization contract first.

Concurrency rules: `docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`.

## NEXT queue

1. `M00-06 Privilege transition foundation`
2. `M00-07 Early physical allocator`
3. `M00-08 Structured event and trace ABI`

Only one architectural milestone is ACTIVE at a time.

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

## Branch/integration rule

Completed milestones and accepted foundational features should be merged into `main` promptly. New major work starts from the latest integrated baseline.

```text
main
  |
  +-- milestone/feature branch
          |
          +-- implementation
          +-- test evidence
          +-- design record
          |
          `-- merge back to main
```

Do not build a long chain of completed milestone branches while leaving `main` stale.

## Progress history

### 2026-08-07 — repository baseline consolidation

- M00-02, M00-03, and M00-04 were integrated into `main` through PR #6 (`d33111c2beb1e360bb057747f8b1c7dda34dc773`).
- Console/Kernel Print is being integrated on `integration/console-foundation` rather than blindly merging the diverged feature branch.
- Conflict resolution preserves both timer and console code paths.
- Kernel, recoverable-trap, machine-timer, and TrapFrame test output is routed through the accepted kernel print path.
- The next architectural milestone is M00-05 SMP/per-hart foundation.

### 2026-08-07 — F00-01 Kernel Print accepted

- Status: DONE
- Original branch: `feature/console-foundation`
- Acceptance: user confirmed the kernel-print QEMU test passed on the development workstation.
- Design decisions: kernel log is authoritative; UART is a bring-up mirror; future service console is a separate failure/runtime domain.

### 2026-08-07 — M00-03 Recoverable trap completed

- Test: `bash scripts/test-recoverable-trap.sh`
- Result: standard and compressed breakpoints resumed, `RECOVERABLE_TRAP_TEST: PASS`, `TRAP_FRAME_TEST: PASS`.

### 2026-08-07 — M00-02 TrapFrame completed

- Test: `bash scripts/test-trap-frame.sh`
- Result: `TRAP_FRAME_TEST: PASS`.

### 2026-08-05 — project execution process established

- One primary architectural milestone at a time.
- Persistent records: `PROJECT_CONTEXT.md`, `docs/JIXIA_SOLO_ROADMAP.md`, and this ledger.
