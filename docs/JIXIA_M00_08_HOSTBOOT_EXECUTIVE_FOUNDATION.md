# Jixia M00-08 Hostboot-Shaped Executive Foundation

**Status:** accepted in `main` at `e930a24`; GNU RV64 build and QEMU lifecycle acceptance passed

**Date:** 2026-08-18

## 1. Purpose

This checkpoint replaces the earlier one-task privilege probe with an executive skeleton whose
ownership and lifecycle follow Hostboot as closely as the current Jixia foundation permits.

Hostboot remains the behavioral and boot-lifecycle reference. The Jixia implementation is an
independent RISC-V expression: it does not translate Hostboot source line by line, and it keeps the
existing Jixia M-mode-bare/U-mode-Sv39 protection model.

Reference baseline:

```text
OpenPOWER Hostboot release-fw1120
commit 22e3c409ab8b439d4c8eb31b644acb498032a487

src/kernel/start.S
src/kernel/kernel.C
src/kernel/cpumgr.C
src/kernel/scheduler.C
src/kernel/taskmgr.C
src/kernel/syscall.C
src/include/kernel/cpu.H
src/include/kernel/scheduler.H
src/include/kernel/task.H
src/include/kernel/taskmgr.H
```

Hostboot is Apache-2.0 licensed. Architecture and observable behavior may be studied and
reimplemented, but literal copied source or comments would require preservation of the applicable
license and attribution. This checkpoint uses project-native names, data structures, comments, and
RISC-V mechanisms while recording Hostboot as its design provenance.

## 2. Bootstrap order

The boot-hart path now has the following explicit order:

```text
clear HartLocal.current_task
    -> C++ bootstrap phase
    -> boot handoff phase (dummy)
    -> PageManager
    -> HeapManager (fixed-pool backend only)
    -> VmmManager
    -> TimeManager
    -> Scheduler
    -> TaskManager
    -> CpuManager + per-hart idle tasks
    -> platform status phase (dummy)
    -> debug registry phase (dummy)
    -> initial U-mode task
    -> release secondary harts
    -> common task dispatcher
```

This preserves the dependency that memory management exists before CPU/task allocation and that all
per-hart scheduler and idle-task objects exist before secondary harts enter the dispatcher.

## 3. Hart release protocol

Jixia uses two release gates because its accepted M00-05 path already requires every hart to prove
its private stack, HartLocal anchor, trap stack, and timer path.

1. `jixia_boot_release` protects BSS/global initialization.
2. `jixia_executive_release` protects TaskManager/Scheduler/CpuManager initialization.

The boot hart publishes the second gate with a release fence immediately before first dispatch.
Secondary harts consume it with an acquire fence, select their per-hart idle task, execute the dummy
deferred-work phase, and enter the same TrapFrame restore path as the boot hart.

This is the RISC-V/Jixia equivalent of Hostboot holding other hardware threads at its startup
spinlock until CPU objects and kernel executive state are ready.

## 4. Object ownership

| Responsibility | Hostboot shape | Jixia checkpoint |
| --- | --- | --- |
| Kernel bootstrap | `Singleton<Kernel>` | `Singleton<Kernel>` |
| Task lifecycle/tree | `Singleton<TaskManager>` | `Singleton<TaskManager>` |
| Global runnable policy | shared `Scheduler` singleton | shared `Scheduler` singleton |
| CPU-local state | `cpu_t` | `HartLocal` anchored by `mscratch` |
| Current task | `SPRG3` | `HartLocal.current_task` |
| Local affinity queue | `cpu_t.scheduler_extra` | `HartLocal.scheduler_extra` |
| Idle task | one per CPU thread | one per present hart |
| Delay-list hook | `cpu_t.delay_list` | reserved `HartLocal.delay_list` |
| Memory/VMM/time | singleton managers | singleton managers |

Global managers own policy and shared queues. `HartLocal` contains only state that differs per hart.
`TaskContext` belongs to a task and is not stored in HartLocal.

## 5. Task and tracker semantics

`Task` contains execution state, explicit address-space identity, scheduling state, stack ownership,
affinity state, detached state, and queue links.

`TaskTracker` survives a joinable task's execution object and records the parent/child tree, exit
status, return value, pending wait, and original entry point.

The implemented lifecycle is:

```text
create
    -> allocate Task + TaskTracker + stack
    -> initialize U context and satp identity
    -> link tracker below current parent (or kernel root)
    -> enqueue READY task

yield
    -> return current non-idle task to runnable queue
    -> choose local-affinity queue, global queue, or idle task

end
    -> choose another runnable task before freeing the current Task
    -> retain tracker for a joinable child
    -> wake a matching blocked parent
    -> immediately reap a detached clean task

wait(tid)
    -> immediately reap an already completed child, or
    -> mark parent BLOCK_JOIN and dispatch another task

wait(-1)
    -> prefer any completed child independent of sibling order
    -> block only when children exist but all are still running

detach
    -> mark the current task non-joinable
    -> clean exit removes its tracker
    -> crash fails closed
```

When a tracker is removed, live children are reparented to its parent. A clean completed child that
becomes kernel-parented is reaped because no task can join it; a crashed orphan fails closed.

## 6. RISC-V dispatch ABI

The Power task-save/`rfid` path is replaced by the accepted Jixia RISC-V mechanism:

```text
TaskContext
    -> x[0..31], mstatus, mepc, satp
    -> restore into trusted per-hart TrapFrame
    -> write task satp
    -> sfence.vma + fence.i
    -> common jixia_trap_restore
    -> mret to U-mode
```

U-mode task code is isolated in the page-aligned `.user_text` section. The shared bootstrap Sv39
root maps that section U-RX and fixed task stacks U-RW. The context and APIs carry an explicit
address-space identity even though this checkpoint temporarily shares one root.

U-mode ECALL enters M-mode directly. The syscall handler saves the caller context, applies the task
operation, selects a task when required, restores the selected context, and returns through the
common trap path.

## 7. Acceptance workload

The resident U-mode acceptance task performs both join paths:

```text
child A: create -> yield -> child exits -> wait(-1) reaps completed child
child B: create -> parent waits immediately -> BLOCK_JOIN -> child exit wakes parent
init:    detach -> return -> common task-end stub -> idle task
```

The dedicated runner is:

```bash
bash scripts/test-m00-08-01-task-lifecycle.sh
```

The implementation must not be reported as accepted until the GNU RISC-V build and QEMU runner
observe every required marker without a fatal trap.

## 8. Explicitly incomplete work

The following interfaces or phases are intentionally incomplete and must not be mistaken for
Hostboot-equivalent functionality:

- dynamic heap allocation; task objects, trackers, and stacks use bounded fixed pools;
- timer-driven preemption; cooperative scheduling points exist, while TimeManager currently records
  only normal/idle policy values;
- sleep/delay queue behavior behind `HartLocal.delay_list`;
- message queues, futexes, deferred work, and provider IPC;
- user output pointers for `wait`; non-null pointers are rejected until `copy_to_user` exists;
- per-service VSpaces and ASIDs; the first tasks share one explicit bootstrap root;
- fine-grained PMP policy; the first-dispatch PMP grant remains a broad probe-only grant;
- bootloader communication data, platform scratch/status publication, and debug pointer registry;
- dynamic stack mapping/reclamation in PageManager;
- production crash reporting and termination policy;
- Linux/OpenSBI handoff and post-boot SBI runtime behavior.

These gaps are staged work, not reasons to change the Hostboot-shaped ownership and lifecycle now in
place.

M00-08.02 begins closing the timer-preemption and delay-queue items without rewriting the accepted
M00-08.01 task/tracker model. See `docs/JIXIA_M00_08_02_HOSTBOOT_SCHEDULER_ALIGNMENT.md`.
