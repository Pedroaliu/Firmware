# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-12
- **Working mode:** solo development with ChatGPT research/review/implementation support
- **Stable integration branch:** `main`
- **Current progress branch:** `milestone/m00-06-03-supervisor-ecall-return`
- **Current milestone:** `M00-06 Privilege transition foundation`
- **Current status:** ACTIVE — M00-06.02 M->S transition accepted; next prove the S->M ECALL round trip and saved lower-privilege context
- **Previous milestone:** `M00-05 Per-hart state, stacks, and SMP foundation` — DONE and integrated into `main`

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
| M00-04 Machine timer interrupt | DONE | PR #6; `scripts/test-timer-interrupt.sh` | first recoverable asynchronous M-mode interrupt |
| F00-01 Kernel print foundation | DONE | `scripts/test-kernel-print.sh` | formatter, append-only KernelLogBuffer, temporary UART mirror, `printk` |
| Console/timer integration | DONE | PR #8; `043d7c71eba8ed067ccda5421a11e408f69bd1a0` | timer and console foundations coexist without regression |
| M00-05 Per-hart state, stacks, SMP foundation | DONE | `bash scripts/test-m00-05-population.sh`; user-confirmed 2026-08-11; `docs/JIXIA_M00_05_SMP_FOUNDATION.md` | private stacks, HartLocal/mscratch, explicit publication, FDT population, per-hart timers, 1/2/4-hart acceptance, controlled 5-hart rejection |
| M00-06 Privilege transition foundation | ACTIVE | `docs/JIXIA_M00_06_PRIVILEGE_TRANSITION.md`; CI run `31567837192` | M00-06.01 trusted trap entry DONE; M00-06.02 M->S supervisor entry DONE; M00-06.03 ECALL round trip next |
| M00-07 Early physical allocator | NEXT | pending | supports later service/memory work |
| M00-08 Structured event and trace ABI | PLANNED | `docs/JIXIA_TRACE_OBSERVABILITY_VISION.md` | shared later with Jingjie |
| M00-09 Automated QEMU test harness | PLANNED | `scripts/jixia.sh`; milestone scripts remain authoritative | consolidate machine-checkable regression tests later |

---

## Accepted foundation through M00-04/F00-01

### M00-02 — TrapFrame

The software trap ABI contains x0-x31 plus `mstatus`, `mepc`, `mcause`, and `mtval`. Assembly and C++ share one checked layout.

Recorded result:

```text
TRAP_FRAME_TEST: PASS
```

Design record: `docs/JIXIA_M00_02_TRAP_FRAME.md`.

### M00-03 — recoverable synchronous traps

Recovery is whitelist-based. The handler verifies `EBREAK` or `C.EBREAK`, advances saved `mepc` by the decoded instruction length, and returns through the common restore + `mret` path.

Recorded result:

```text
RECOVERABLE_TRAP_TEST: PASS
TRAP_FRAME_TEST: PASS
```

### M00-04 — machine timer interrupt

The timer path introduced the first asynchronous recoverable trap. Its core invariant is that asynchronous timer handling does **not** artificially advance saved `mepc`.

### F00-01 — Kernel Print

Accepted minimum diagnostic path:

```text
printk
   |
shared freestanding formatter
   |
KernelLogBuffer
   |
   `---- temporary raw-UART mirror
```

The future runtime Console Service remains a separate failure/runtime domain.

---

## DONE — M00-05 Per-hart state, stacks, and SMP foundation

### Objective achieved

Mozi now has a correct small-SMP foundation that separates architectural hart identity from dense software runtime state and does not infer physical topology from hart-number arithmetic.

### Accepted mechanisms

```text
[x] define maximum capacity without treating it as actual population
[x] define dense HartIndex separately from architectural HartId
[x] provide a private per-hart stack before C/C++ entry
[x] make exactly one boot hart perform BSS/global initialization
[x] keep pre-BSS synchronization variables outside .bss
[x] use atomic slot allocation for uniqueness
[x] use explicit release/acquire ordering for publication
[x] add HartLocal and bind it through per-hart mscratch
[x] preserve a single-writer printk policy during M00-05
[x] discover actual CPU population from a bounded FDT parser
[x] reject invalid/zero/over-capacity population without waiting forever
[x] move timer compare/state to per-hart ownership
[x] prove every present hart can take its own timer interrupt
[x] retain M00-02/M00-03/M00-04/F00-01 regressions
[x] add machine-checkable 1/2/4-hart and over-capacity acceptance
[x] record concurrency and SMP design invariants
```

### Core invariants

- a hart never enters normal C/C++ on another hart's stack;
- secondaries do not touch normal BSS/global state before the boot hart publishes completion;
- slot-allocation atomicity and memory-publication ordering are treated as different problems;
- `HartId` is architectural identity, while `HartIndex` is a dense software slot;
- physical socket/core/cluster/NUMA topology belongs to PlatformGraph;
- per-hart ownership is preferred over shared mutable state and locks;
- `volatile` is not treated as cross-hart synchronization;
- only the boot hart is a normal `printk` writer in this milestone;
- `mscratch -> HartLocal` becomes the per-hart kernel-state anchor for later privilege work.

### Acceptance evidence

Primary command:

```bash
bash scripts/test-m00-05-population.sh
```

User-confirmed on 2026-08-11.

Supported matrix:

```text
-smp 1: PASS
-smp 2: PASS
-smp 4: PASS
```

Controlled over-capacity case:

```text
-smp 5
unsupported CPU count 5 (capacity 4)
SMP_POPULATION_TEST: FAIL
CONTROLLED_OVER_CAPACITY: PASS
```

Supported cases preserve:

```text
SMP_FOUNDATION_TEST: PASS
SMP_POPULATION_TEST: PASS
SMP_TIMER_TEST: PASS
KERNEL_PRINT_TEST: PASS
RECOVERABLE_TRAP_TEST: PASS
MACHINE_TIMER_TEST: PASS
TRAP_FRAME_TEST: PASS
```

Design record:

- `docs/JIXIA_M00_05_SMP_FOUNDATION.md`
- `docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`

### Known limitations

M00-05 intentionally does not add:

- scheduler or arbitrary secondary-hart work dispatch;
- user/supervisor execution;
- lower-privilege trap-stack switching;
- concurrent multi-writer `printk`;
- physical socket/core/NUMA topology;
- dynamic hart hotplug;
- a general PlatformGraph/FDT implementation;
- structured Event/Trace ABI.

The host ThreadSanitizer experiment is deferred because the current Deepin GCC TSan runtime fails before the model executes with an unexpected-memory-mapping error; the available Clang setup lacks the required C++ standard-library configuration. This is recorded as a host-tooling limitation, not a target-runtime correctness failure.

Structured trace/event evidence is not yet applicable because the shared event ABI is explicitly scheduled for M00-08. M00-05 uses machine-checkable serial markers plus the acceptance harness.

---

## NOW — M00-06 Privilege transition foundation

### Objective

Prove a small, controlled privilege transition before introducing paging, allocator, scheduler, or service isolation.

Target state machine:

```text
M-mode Mozi
    -> configure trusted runtime M trap stack
    -> configure MPP = S, mepc, PMP/bootstrap permissions
    -> mret
    -> S-mode payload with satp = 0 and its own stack
    -> ecall / controlled exception
    -> M-mode trap on trusted per-hart trap stack
    -> prove previous privilege and saved state
    -> controlled return or termination
```

### Accepted through M00-06.02

M00-06.01 established the lower-privilege trust boundary. M00-06.02 then made runtime trap-stack ownership uniform and proved the first one-way M->S transition.

Accepted runtime stack ownership:

```text
per hart

normal M stack
    ordinary M-mode firmware execution

trusted M trap stack
    all runtime M-level traps: M->M, S->M, U->M

S probe stack
    first supervisor payload
```

Runtime trap entry now saves interrupted x2/sp as a value, rejects unsupported nested traps, switches to the per-hart trusted trap stack, constructs the TrapFrame there, dispatches, restores the interrupted x2, and returns with `mret`.

The first S transition uses:

```text
satp      = 0
medeleg   = 0
mideleg   = 0
MIE/mie   = 0
PMP entry = permissive RWX NAPOT, unlocked, probe only
```

The temporary PMP entry is permission for the transition experiment, not a service-isolation design.

Machine-checkable result:

```text
M00_06_02_TRANSITION_ARMED: PASS
M00_06_02_SUPERVISOR_ENTRY: PASS
M00-06.02 supervisor transition: PASS
```

CI evidence: GitHub Actions run `31567837192`.

### ACTIVE submilestone — M00-06.03 S->M ECALL round trip

Next proof:

```text
S payload
    -> load known S-mode register markers
    -> ecall
    -> M trap entry on trusted per-hart trap stack
    -> validate mcause == 9
    -> validate mstatus.MPP == S
    -> validate saved S x2/gp/a0/a7 and mepc
    -> return to S or a controlled M continuation
```

The M00-06 design and acceptance contract is recorded in `docs/JIXIA_M00_06_PRIVILEGE_TRANSITION.md`.

### Explicit non-goals for M00-06

```text
Sv39/page tables
physical allocator
scheduler/tasks
U-mode applications
PMP service isolation
ArchHV / HS / VS
full syscall ABI
nested M-level trap support
```

---

## NEXT queue

1. finish `M00-06.03 S->M ECALL round trip`
2. finish `M00-06.04 hostile lower-privilege stack and M00-06 closure`
3. `M00-07 Early physical allocator`
4. `M00-08 Structured event and trace ABI`
5. `M00-09 Automated QEMU test harness`

Only one architectural milestone is ACTIVE at a time.

---

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

---

## Branch/integration rule

Development branches may contain fine-grained implementation/debug commits. Accepted submilestones are squashed into `main` as semantic checkpoints. The next submilestone branch then advances from that clean checkpoint.

```text
main
  |
  +-- milestone/Mxx.xx branch
          |
          +-- implementation
          +-- tests / CI
          +-- design record
          |
          `-- squash accepted checkpoint into main
```

Do not build a long chain of completed milestone branches while leaving `main` stale.

---

## Progress history

### 2026-08-12 — M00-06.02 accepted

- Added a dedicated trusted runtime M trap stack for every hart.
- Unified runtime M-level trap storage: M-origin and lower-origin traps both construct TrapFrames on the per-hart trap stack.
- Preserved the interrupted x2/sp value before switching stack domains.
- Added fail-closed detection for unsupported nested/double M-level traps.
- TrapFrame regression now verifies that the frame resides on trusted trap-stack storage.
- Added the first controlled one-way M->S transition with `satp=0`, no delegation, and interrupts disabled.
- The first CI attempt exposed an S-mode instruction-access fault; the trusted M trap path safely captured it.
- Added a permissive, unlocked PMP probe entry so S-mode can execute/use its stack/UART without claiming isolation.
- GitHub Actions run `31567837192` passed all old regressions plus `M00-06.02 supervisor transition`.
- Next submilestone: M00-06.03 S->M ECALL round trip.

### 2026-08-11 — M00-06 activated

- M00-05 closure records were integrated into `main`.
- New branch: `milestone/m00-06-privilege-transition`.
- The initial M00-06 experiment is constrained to M->S->M with `satp = 0`.
- Trusted per-hart trap-stack entry is the primary correctness/security problem.

### 2026-08-11 — M00-05 accepted and integrated

- User confirmed the complete population/SMP timer regression matrix passed on the development workstation.
- Supported QEMU populations: 1, 2, and 4 harts.
- Five harts exceed current capacity and are rejected through the controlled failure path.
- M00-05 design/invariant record added as `docs/JIXIA_M00_05_SMP_FOUNDATION.md`.
- Host TSan remains deferred tooling work and does not block milestone acceptance.

### 2026-08-07 — repository baseline consolidated

- M00-02, M00-03, and M00-04 integrated through PR #6.
- Console/timer conflict-resolved integration merged through PR #8.
- CMake, firmware entry, trap dispatch, diagnostics, linker rules, and regression scripts preserve both timer and console foundations.

### 2026-08-05 — project execution process established

- One primary architectural milestone at a time.
- Persistent records: `PROJECT_CONTEXT.md`, `docs/JIXIA_SOLO_ROADMAP.md`, and this ledger.
