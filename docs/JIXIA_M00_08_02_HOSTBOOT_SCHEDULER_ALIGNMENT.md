# Jixia M00-08.02 Hostboot Scheduler Alignment

**Status:** implemented with local acceptance evidence (2026-08-19): deterministic single-hart RV64/QEMU acceptance PASS ×3 with quantitative preemption and deadline evidence; full local M00-02..M00-08.02 regression chain PASS. NOT formally DONE until the GitHub Actions run ID is recorded at integration.

**Date:** 2026-08-18

## 1. Scope and reference

This increment narrows one specific gap between the accepted M00-08.01 task lifecycle and the
Hostboot kernel: a runnable U-mode task must not depend on voluntary ECALLs for another task to run.

Pinned reference:

```text
OpenPOWER Hostboot release-fw1120
commit 22e3c409ab8b439d4c8eb31b644acb498032a487

src/kernel/kernel.C
src/kernel/start.S
src/kernel/cpumgr.C
src/kernel/scheduler.C
src/kernel/taskmgr.C
src/kernel/syscall.C
src/kernel/timemgr.C
src/include/kernel/cpu.H
src/include/kernel/task.H
src/include/kernel/scheduler.H
src/include/kernel/taskmgr.H
src/include/kernel/timemgr.H
src/include/util/singleton.H
```

Hostboot is Apache-2.0 licensed. Jixia preserves the source provenance and observable executive
logic but uses project-native code, RISC-V state, Sv39, CLINT `mtime`, and the Jixia M/U boundary.
No POWER assembly or literal Hostboot implementation text is copied into Jixia.

## 2. Singleton and ownership answer

Jixia is not a single-task singleton design. It has one executive object per firmware image for
global policy and one local execution object per hart:

| Object | Cardinality | Responsibility |
| --- | --- | --- |
| `Kernel` | one | ordered bootstrap and first dispatch |
| `TaskManager` | one | task allocation, TIDs, tracker tree, wait/detach/end |
| `Scheduler` | one | shared global ready queue and per-hart local queues |
| `TimeManager` | one | timeslice policy and all per-hart delay queues |
| `CpuManager` | one | present-hart executive construction |
| `VmmManager` | one for bootstrap | shared bootstrap service VSpace |
| `HartLocal` | one per present hart | current task, local queue, delay queue, idle task, counters |
| idle task | one per present hart | runnable fallback for that hart |
| `Task` / `TaskTracker` | one per task | execution context and lifecycle identity |

The singleton implementation deliberately relies on boot-hart-first construction plus the
executive release gate. The build uses `-fno-threadsafe-statics`; secondary harts may access a
singleton only after the boot hart publishes completion with release/acquire ordering.

This follows Hostboot's shape: managers and the global scheduler are shared; CPU-local pointers and
queues are not.

## 3. M00-08.01 scheduler boundary

M00-08.01 already implemented the following Hostboot-shaped mechanisms:

```text
TaskContext save/restore
Task + TaskTracker split
parent/child tracker tree
create / yield / end / wait / detach
global FIFO ready queue
per-hart affinity queue
per-hart idle task
U ECALL -> M kernel -> selected U context
```

Its dedicated QEMU acceptance proves the lifecycle on one hart. It does not prove timer
preemption, sleep/wakeup, service IPC, separate service address spaces, or multi-hart task
migration.

## 4. Preemptive dispatch translation

Hostboot's decrementer handler performs this policy:

```text
release expired delayed tasks
run CPU periodic work
if periodic work did not replace the current task:
    return current task to scheduler
    select next runnable task
```

Jixia translates the same policy to RISC-V machine timer interrupts:

```text
U task receives machine timer interrupt
    -> trap.S saves a complete TrapFrame on trusted per-hart M trap stack
    -> timer source is acknowledged and mtimecmp is disarmed
    -> save TrapFrame into current TaskContext
    -> release expired tasks from current hart's delay queue
    -> requeue current non-idle task
    -> select local-affinity, global, or idle task
    -> restore selected TaskContext and satp
    -> program next absolute mtimecmp deadline
    -> common trap restore
    -> mret to selected U task
```

The handler does not enable `mstatus.MIE` while it owns the non-nestable trusted trap stack.
It enables only the MTIE source. `mret` restores interrupt enable state from the selected U
context's `mstatus.MPIE`.

Every real timer-driven switch increments `HartLocal.scheduler_preemption_count`.

Timer arming is executive mechanism, not probe workload: first dispatch, every rescheduling ECALL
exit, and every timer-driven dispatch re-arm the absolute `mtimecmp` deadline in all M00-08 builds.
Only the acceptance workload and its markers remain behind the M00-08.02 probe gate, matching the
M00-08.01 convention (mechanism unconditional, workload gated). M00-08.01 acceptance therefore
already runs with machine-timer preemption armed.

## 5. Sleep and idle deadline policy

Each present hart owns a sorted, spinlock-protected delay queue. A sleeping task carries an
intrusive `DelayNode`, matching the role of Hostboot's task delay node without reusing ready-queue
links.

```text
sleep(seconds, nanoseconds)
    -> validate and convert duration against QEMU-virt timebase
    -> compute absolute wake deadline
    -> mark task BLOCK_SLEEP
    -> insert into current hart delay queue
    -> dispatch another task

timer interrupt
    -> remove every deadline <= now
    -> add each released task to Scheduler
```

When idle is selected, the next timer deadline is the earlier of:

```text
normal idle slice end
nearest sleeping-task deadline
```

The final `mtimecmp` value is programmed as an absolute deadline. A tight idle-yield loop must not
recompute `now + old_remaining` and gradually move a sleeper's deadline forward.

Acceptance proves this quantitatively: the workload prints its requested and measured tick counts,
and the kernel publishes its own arming constants (`M00_08_SCHED_SLICES`). The runner asserts
`elapsed >= requested` and `elapsed < task_timeslice_ticks`, deriving the bound from the printed
constants rather than a magic number. Both polling regressions fail deterministically, because the
timer is re-armed at the scheduling point: a fixed task-slice timer wakes the sleeper roughly one
task slice (100 000 ticks) after sleep entry, and a fixed idle-slice timer roughly one idle slice
(1 000 000 ticks). Deadline-aware arming wakes it just above the 20 000-tick request, about five
times below the bound.

The QEMU-virt backend currently records a 10 MHz timebase constant. Reading the platform
`timebase-frequency` property from the boot handoff is still required before this becomes a generic
platform contract.

## 6. Shared bootstrap VSpace invariant

M00-08.01 mapped a task stack when the task was created. That is safe in the one-hart acceptance,
but unsafe as a general shared-root SMP rule: another hart may already be using the same page table,
and Jixia has no TLB-shootdown protocol yet.

M00-08.02 therefore pre-maps every bounded fixed-pool task stack before secondary harts pass the
executive release gate:

```text
VmmManager creates bootstrap root
    -> TaskManager maps all fixed task-stack pages
    -> CpuManager creates all per-hart idle tasks
    -> initial task created
    -> release secondary harts
```

Later `task_create` reuses an existing mapping and does not change the live shared root.

This removes one immediate SMP hazard; it does not provide per-service isolation. All bootstrap
tasks still share one root and can address every mapped bootstrap stack.

## 7. Acceptance workload

The M00-08.02 U-mode workload creates:

1. a CPU-bound child that never yields or performs a syscall and spins on a shared one-shot word;
2. a witness child queued behind it that releases that word;
3. a sleeper child that blocks for two milliseconds.

The required evidence is:

```text
CPU-bound child selected first
    -> mtime preempts it
    -> witness executes and releases the handshake
    -> interrupted CPU-bound context resumes and exits
    -> timer preemption counter advances

sleeper calls sleep
    -> task becomes BLOCK_SLEEP
    -> idle path runs
    -> nearest deadline triggers timer
    -> sleeper becomes READY and resumes after ECALL
```

Runner:

```bash
bash scripts/test-m00-08-02-preemptive-scheduler.sh
```

Required quantitative evidence, asserted by the runner rather than only printed:

```text
M00_08_PREEMPTION_COUNT: >= 1
M00_08_SCHED_SLICES: arming constants (bound source)
M00_08_SLEEP_WAKE_EVIDENCE: elapsed >= requested && elapsed < task_timeslice_ticks
```

Observed locally on 2026-08-19 across three deterministic runs: elapsed 20129/20106/20160 of
20000 requested ticks (task slice 100000), preemption count 1. The pre-release stack pre-mapping
invariant (section 6) is proven constructively — the fixed-pool mapping loop precedes the executive
release gate in the same static boot sequence — so no runtime page-table instrumentation is added
for it.

The runner deliberately uses `-smp 1` so the handshake is a deterministic preemption proof rather
than a second hart running the witness concurrently. The CI workflow now runs both M00-08.01 and
M00-08.02 after all earlier regressions; multi-hart scheduler acceptance remains a separate item.

## 8. Can Jixia run a user service now?

It can create and execute a real RISC-V U-mode task from statically resident `.user_text`:

```text
raw entry + argument + explicit AddressSpace
    -> Task / stack / TaskContext
    -> ready queue
    -> mret to U
    -> preempt / block / wake / exit / wait
```

That is sufficient for a resident acceptance task or an early pinned provider prototype.

It is not yet a production `usr service` launch path. A service requires at least:

```text
resident component registry / build-generated catalog
component identity rather than arbitrary raw entry pointer
mapped code + rodata + data + stack contract
per-service VSpace and protection policy
safe copy_to_user / copy_from_user
message queue IPC with blocking and wakeup
service startup result and crash reporting
resource teardown
```

Therefore the correct current statement is:

> Jixia can schedule preprocessed U-mode task code; it cannot yet `task_exec` a named firmware
> service with a protected address space and IPC contract.

## 9. Remaining Hostboot-kernel gaps

### Source-area matrix

| Hostboot area | Jixia state after M00-08.02 | Remaining alignment |
| --- | --- | --- |
| `kernel.C` | ordered manager bootstrap and first U dispatch | real boot handoff, platform status, debug registry, deferred work |
| `start.S` | per-hart boot, trusted trap stack, complete integer frame, `mret` | production vector policy, nested/priority rules, FP/vector state, shutdown/wakeup paths |
| `cpumgr.C` | bounded FDT population, per-hart objects, release gate | master migration, online/offline, periodic work, remote wake/doorbells |
| `scheduler.C` | shared FIFO, per-hart affinity queue, idle fallback, timer rotation | accepted SMP migration, load/wake policy, affinity API, contention stress |
| `taskmgr.C` | Task/Tracker tree, create/end/wait/detach, U context | dynamic stacks, safe wait outputs, TLS/FP/vector cleanup, full crash status/reaping |
| `timemgr.C` | per-hart sorted delays, sleep/wakeup, nearest idle deadline | handoff-derived timebase, short-delay policy, remote wake broadcast, timer wrap tests |
| `syscall.C` | task lifecycle plus sleep and invalid-call task crash | typed dispatch table, pointer validation/copy, IPC, futex, VM/device calls, ABI versioning |
| `doorbell.C` / `ipc.C` | absent | CLINT/ACLINT software-interrupt IPI, remote runnable notification, action queue |
| `msghandler.C` / `intmsghandler.C` | absent | message queues, send/receive/respond, interrupt-to-service delivery |
| `futexmgr.C` | absent | wait/wake/requeue and teardown semantics after safe user memory exists |
| `vmmmgr.C` / segments / `block.C` | one shared bootstrap Sv39 root | per-service VSpace, regions/backing objects, provider-blocked faults, ASIDs/shootdown |
| `stacksegment.C` | fixed pre-mapped 4 KiB stacks | guard pages, growth policy, dynamic allocation and deterministic reclamation |
| `pagemgr.C` / `heapmgr.C` | contained-page allocator and fixed executive pools | production heap, freeing/coalescing, ownership accounting, post-DDR lifecycle |
| `exception.C` / `machchk.C` / `terminate.C` | whitelisted traps plus fatal park | U-fault task termination, structured FFDC, recoverability policy, platform shutdown |
| `deferred.C` / `workitem.C` | bootstrap placeholder | ordered deferred queue and periodic kernel work |
| `console.C` / `simpletrace.C` / `idebug.C` | `printk` and bounded kernel log | SMP-safe publication, trace ABI, pointer registry, task/queue dumps |
| `devicesegment.C` | absent | capability-checked device mapping and unmapping |
| `bltohbdatamgr.C` | QEMU `a0/a1` handoff placeholder | typed, validated bootloader/platform communication data |

The missing rows should not all move into M-mode. Hostboot is the lifecycle reference, while Jixia
keeps only protection, scheduling, memory, IPC, and machine-authority mechanisms in the trusted
kernel. Component lookup, istep policy, providers, and most diagnostics belong in disposable
U-mode services.

### Required before the first real InitService

1. typed message queue and synchronous send/receive/respond blocking paths;
2. user-memory translation and copy helpers for syscall pointers;
3. resident Root Component Registry and deterministic component metadata;
4. per-service VSpaces, stack guard pages, and narrow PMP policy;
5. crash-to-task termination for U faults with parent-visible status;
6. task/stack/page reclamation beyond the fixed bootstrap pool.

### Required before SMP scheduling is accepted

1. multi-hart M00-08 workload and deterministic evidence;
2. IPI/doorbell wakeup when a task is queued for an idle remote hart;
3. TLB shootdown/ASID rules for any live address-space mutation;
4. affinity pin/unpin and migration semantics;
5. synchronized console/trace publication from multiple harts;
6. lock-order and race stress for task end, wake, wait, and migration.

### Later Hostboot executive parity

- dynamic heap and stack segments;
- futex wait/wake/requeue;
- deferred work and periodic kernel actions;
- message-handler-backed VMM faults;
- FP/vector lazy context and TLS cleanup;
- CPU online/offline, master migration, shutdown, and wakeup paths;
- debug pointer registry, task tree dump, structured crash FFDC;
- production VMM segments, permissions, device maps, and mainstore extension.

These are ordered dependencies, not a reason to copy all of `src/kernel` into the trusted core.
Jixia should retain Hostboot's lifecycle logic while preserving the smaller M-mode mechanism and
moving firmware policy into U-mode services.
