# Jixia M00-05 SMP Foundation

## Status

- **Milestone:** M00-05 — Per-hart state, stacks, and SMP foundation
- **Status:** DONE
- **Accepted:** 2026-08-11
- **Primary acceptance command:** `bash scripts/test-m00-05-population.sh`
- **Platform:** QEMU `virt`, RV64

M00-05 moves Mozi from a single-boot-hart foundation to a correct small-SMP execution model without treating architectural hart IDs as software topology.

The milestone deliberately stops before scheduler, general work dispatch, concurrent console output, NUMA topology, or userspace. Its purpose is to establish the per-hart ownership and synchronization rules that later privilege, scheduler, interrupt, and service work can safely depend on.

---

## 1. Accepted invariants

### 1.1 Private stack before C/C++

Every participating hart owns a private early/kernel stack before entering C or C++.

A secondary hart must never execute normal firmware code on another hart's stack.

### 1.2 One boot hart owns global initialization

Only architectural hart 0 performs `.bss` and global initialization.

Secondary harts may claim a private stack before global initialization completes, but they may not enter normal C/C++ until the boot hart publishes completion.

### 1.3 Boot synchronization storage is not in `.bss`

The early rendezvous variables live in image-backed `.data.boot_sync` because secondary harts need them while the boot hart is still clearing `.bss`.

Placing the release flag or slot allocator in `.bss` would create a bootstrapping race with the BSS clear itself.

### 1.4 HartId is not topology

Jixia distinguishes:

```text
HartId
    architectural hardware identity

HartIndex
    dense software runtime slot
```

No microkernel code may infer socket/core/cluster/NUMA identity from arithmetic on `HartId`.

Physical topology belongs to PlatformGraph.

### 1.5 Atomicity and ordering are separate requirements

Secondary slot allocation uses an atomic fetch-and-add because uniqueness requires atomicity.

Global-state publication uses explicit release/acquire ordering because visibility requires memory ordering.

These are different correctness problems and are not interchangeable.

### 1.6 HartLocal publication is explicit

Each `HartLocal` slot has one owner/writer. The owner initializes all fields and then publishes `state = online` with release semantics.

Readers observe `state` with acquire semantics before consuming the rest of the slot.

`volatile` is not used as a cross-hart synchronization primitive.

### 1.7 Per-hart state is preferred to shared mutable state

M00-05 partitions timer state per hart rather than protecting one global timer counter with a lock.

This establishes the preferred concurrency pattern for later work:

```text
ownership / partitioning first
shared synchronization only when the abstraction truly requires sharing
```

### 1.8 Console remains single-writer

During M00-05 only the boot hart writes normal `printk` output.

Secondary harts participate in bounded SMP tests and publish results through explicit shared state. They do not become arbitrary console writers.

---

## 2. Boot sequence

The accepted early SMP flow is:

```text
all harts enter _start
        |
        +-- disable interrupts
        +-- initialize gp
        |
        +-- hart 0 -> dense slot 0
        |
        `-- secondary
              -> atomic claim dense slot 1..N-1
              -> reject/park on capacity overflow

all supported harts
        -> select private stack from dense slot
        -> ABI-align sp

hart 0
        -> clear .bss
        -> release-publish boot completion

secondary harts
        -> poll boot completion
        -> acquire fence

all released harts
        -> install per-hart mtvec
        -> pass HartId + DTB + HartIndex to C++
        -> initialize HartLocal
        -> bind HartLocal pointer in mscratch
        -> release-publish HartLocal online state
```

The `mscratch -> HartLocal` anchor is intentionally established now so later privilege-transition trap entry can locate trusted per-hart kernel state without relying on a lower-privilege stack.

---

## 3. Capacity versus population

M00-05 explicitly separates compile-time/runtime capacity from actual machine population.

Current capacity:

```text
HART_MAX_COUNT = 4
```

Actual QEMU CPU population is discovered from the supplied FDT.

The boot hart rejects:

```text
present_count == 0
present_count > capacity
invalid DTB
```

instead of waiting forever for non-existent harts.

Current FDT support is intentionally minimal and bounded. It counts CPU nodes for the QEMU-virt milestone; it is not yet the future PlatformGraph topology parser.

---

## 4. Per-hart timer proof

M00-05 converts timer state and QEMU timer-compare operations to per-hart ownership.

Software uses dense `HartIndex` for software-owned arrays and architectural `HartId` where the QEMU timer hardware interface is explicitly indexed by hart identity.

The SMP timer probe verifies that every present hart:

1. starts from its own timer count;
2. arms its own one-shot compare;
3. receives its own machine timer interrupt;
4. traverses its own trap path;
5. increments only its own `HartLocal` timer count;
6. publishes completion to the boot hart;
7. finishes with exactly one observed timer interrupt for that probe.

The boot hart is the only hart that prints the aggregate test result.

---

## 5. Acceptance matrix

User-confirmed acceptance on 2026-08-11:

```bash
bash scripts/test-m00-05-population.sh
```

Supported populations:

```text
-smp 1  PASS
-smp 2  PASS
-smp 4  PASS
```

Controlled over-capacity path:

```text
-smp 5
unsupported CPU count 5 (capacity 4)
SMP_POPULATION_TEST: FAIL
CONTROLLED_OVER_CAPACITY: PASS
```

Expected supported-case markers:

```text
SMP_FOUNDATION_TEST: PASS
SMP_POPULATION_TEST: PASS
SMP_TIMER_TEST: PASS
KERNEL_PRINT_TEST: PASS
RECOVERABLE_TRAP_TEST: PASS
MACHINE_TIMER_TEST: PASS
TRAP_FRAME_TEST: PASS
```

The current firmware intentionally parks, so the QEMU timeout used by the acceptance harness is an expected termination mechanism rather than a failure.

---

## 6. What M00-05 proves

M00-05 provides executable evidence for:

- private per-hart stacks before C/C++;
- dense software hart slots independent of architectural identity;
- boot-hart-only global initialization;
- explicit release/acquire boot rendezvous;
- per-hart `HartLocal` state;
- `mscratch` as the per-hart kernel-state anchor;
- actual-population discovery separate from maximum capacity;
- controlled over-capacity rejection;
- per-hart timer compare/state/trap behavior;
- release/acquire result publication in the SMP timer test;
- preservation of M00-02/M00-03/M00-04/F00-01 regressions.

---

## 7. Known limitations and deferred work

M00-05 intentionally does **not** provide:

- scheduler or arbitrary secondary-hart work dispatch;
- user or supervisor execution;
- trusted trap-stack switching from lower privilege;
- multi-writer `printk`;
- physical socket/core/NUMA topology;
- a general Device Tree / PlatformGraph implementation;
- dynamic hart hotplug;
- scalable idle/wakeup protocol;
- formal RVWMO litmus verification;
- a functioning host ThreadSanitizer gate on the current Deepin toolchain.

The host TSan experiment was deferred because the local GCC TSan runtime fails before the test model with an unexpected-memory-mapping error, while the available Clang installation lacks the required C++ standard-library setup. This tooling limitation is not treated as M00-05 target-runtime failure.

Structured Event/Trace ABI does not exist yet; machine-checkable acceptance currently relies on dedicated UART markers and test scripts. Structured event evidence becomes a project-wide contract in M00-08.

---

## 8. Handoff to M00-06

M00-06 can now rely on:

```text
private per-hart stack
HartLocal
mscratch -> HartLocal
complete TrapFrame
recoverable mret path
per-hart mtvec
```

The next architectural problem is privilege transition.

The important new invariant is that after entering S-mode, M-mode trap handling must not blindly trust the interrupted S-mode stack. M00-06 must define a trusted per-hart trap/kernel stack transition while preserving the interrupted lower-privilege `sp` in the saved context.

The first privilege experiment should isolate the privilege mechanism from virtual memory complexity:

```text
M-mode Mozi
    -> set mstatus.MPP = S
    -> set mepc
    -> mret
    -> S-mode payload with satp = 0
    -> ecall / controlled trap
    -> M-mode trusted trap path
```

Paging, allocator, service scheduling, and full memory isolation remain later milestones.
