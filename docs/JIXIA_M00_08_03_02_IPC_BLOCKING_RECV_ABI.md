# Jixia M00-08.03.02 — Frozen Blocking Receive IPC ABI

- **Date:** 2026-08-21
- **Status:** FROZEN FOR IMPLEMENTATION — implementation + local acceptance complete on
  `agent/m00-08-03-02-ipc-blocking-recv`; **PR pending** (not merged; integration
  evidence is recorded in the ledgers only after merge).
- **Builds on:** `docs/JIXIA_M00_08_03_01_IPC_NONBLOCKING_ABI.md` (accepted).
- **Out of scope here:** `ipc_call`/`ipc_reply`, ReplyToken/Transact records, timeouts,
  cancellation, capability tables, service registry/name routing, and full external
  kill / task-exit IPC cleanup. All remain open for later M00-08.03 increments.

## 1. Syscall ABI change (single definition site)

`microkernel/arch/riscv/task_syscall_abi.h` keeps exactly 13 numbers, unchanged:

```c
#define JIXIA_TASK_SYSCALL_ENDPOINT_CREATE  6
#define JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY 7
#define JIXIA_TASK_SYSCALL_SEND             8
#define JIXIA_TASK_SYSCALL_CALL             9   /* reserved, -ENOSYS */
#define JIXIA_TASK_SYSCALL_RECV            10   /* ipc_recv: implemented here */
#define JIXIA_TASK_SYSCALL_TRY_RECV        11   /* unchanged, non-blocking */
#define JIXIA_TASK_SYSCALL_REPLY           12   /* reserved, -ENOSYS */
#define JIXIA_TASK_SYSCALL_COUNT           13
```

Number 10 changes from `reserved` to the blocking `ipc_recv`. Numbers 9 and 12 keep
returning `-ENOSYS` (38). No number is renumbered and the count stays 13.

| Syscall | In | Out | Errors |
|---|---|---|---|
| `ipc_recv` (10) | `a0` = handle | success: `a0` = 0, `a1..a4` = payload words 0..3, `a5` = sender TaskId | `-EINVAL` (22) malformed/stale handle (returns immediately, never blocks); `-EIDRM` (43) endpoint destroyed while this receiver was blocked |

Semantics frozen here:

- Any task holding a valid global handle may `ipc_recv` (same policy as send/try_recv;
  destroy stays owner-only).
- Multiple receivers queue FIFO per endpoint; a send consumes exactly one waiter.
- No timeout and no cancel: the only wake sources are a matching `send` or endpoint
  destruction.
- On the `-EIDRM` wake, only `a0` is defined; `a1..a5` are unspecified.
- A blocked receiver's registers are written by the kernel before the task is made
  runnable again; the receiver resumes at the instruction after its `ecall`.

## 2. Kernel data structures (all static, no dynamic allocation)

```text
Task (microkernel/core/task.h)
    MessageWaitNode message_wait    // NEW: {previous, next, queued}
                                   // independent of runqueue previous/next and of
                                   // DelayNode; one task lives in at most one
                                   // endpoint waiting FIFO

Endpoint (ipc_manager.cpp, private)
    Task* waiting_head              // NEW: FIFO of blocked receivers
    Task* waiting_tail              // NEW
    size_t waiting_count            // NEW
```

## 3. Protocols (linearization and lock order)

Lock order, one direction only: `table_lock_ -> endpoint lock -> {runqueue lock,
delay-queue lock}`. No TaskManager lock is ever taken inside an endpoint lock. Never
two endpoint locks at once. Production `printk` remains unchanged and lock-free at
record level; concurrent UART text is therefore not a multi-hart correctness oracle.
Verification uses the independent structured trace instead of adding a console lock
whose interrupt/preemption recursion contract has not been designed.

### 3.1 Atomic blocking (recv, inside `endpoint.lock`)

```text
enqueue waiter -> state = blocked_message -> state_info = handle -> set_next_runnable()
```

The endpoint lock is released only after this hart's `current_task` has been replaced,
so a waking send on another hart can never observe a half-blocked receiver. After
`recv()` returns `blocked` to the syscall handler, the old caller may already run on
another hart — the handler must not touch the old caller's `Task`/`Context` again; it
works only on values captured before the call and on the returned value struct.

A pending message (send-before-recv) is popped into the caller's return registers
without blocking; `waiting_count` and `count` are never both nonzero.

### 3.2 Wakeup (send, inside `endpoint.lock`)

- Pop exactly one receiver (FIFO head).
- Write the receiver's saved return registers (`a0 = 0`, `a1..a4` payload, `a5` sender).
- Clear waiting membership and `state_info`.
- `scheduler.add_task(receiver)` inside the endpoint lock; an `add_task` failure is a
  broken kernel invariant — the kernel fails closed (parks) rather than returning
  `-EAGAIN`.
- No direct handoff and no forced sender yield: the sender keeps running.

A send consumes exactly one waiter or produces exactly one pending message — never both.

### 3.3 Destroy (table_lock -> endpoint_lock)

- The DEAD flip plus epoch advance remains the linearization point.
- The pending FIFO is cleared.
- Every blocked receiver is woken FIFO-order with `a0 = -EIDRM` and published READY via
  `scheduler.add_task`, each exactly once.
- Later operations on the old handle fail with `-EINVAL` (unchanged from M00-08.03.01).

## 4. Invariants (kernel-enforced, fail closed)

- `waiting_count > 0 && count > 0` never holds; checked at every entry.
- A receiver is in at most one waiting FIFO; `RunQueue::insert`,
  `Scheduler::add_task`, `TaskManager::set_current_task`, and
  `TaskManager::release_task_locked` all refuse (fail closed) a task whose
  `message_wait.queued` is set — a blocked receiver can never be scheduled or silently
  freed (no silent UAF from task release).
- A waiter is woken at most once (send or destroy), because it is unlinked from the FIFO
  before it is made runnable.
- Full external kill / task-exit IPC cleanup is explicitly not implemented in this
  increment; `release_task_locked` refuses such tasks instead of freeing them.

## 5. Acceptance mapping

`scripts/test-m00-08-03-02-ipc-blocking-recv.sh` builds the verification-only
`jixia-verify.bin` image (`JIXIA_M00_08_03_02_PROBE=ON`,
`JIXIA_VERIFICATION=ON`) and boots it twice. The normal build remains
`jixia.bin`: it contains no trace storage, jitter hooks, checker logic or
test-only wake counters, and the production IPC API does not expose verification
outputs. This follows Hostboot's useful `hbicore.bin` / `hbicore_test.bin`
separation while retaining one production implementation under test.

- `--smp 1` deterministic run — required-marker + ordered-marker checks:

| Case | Evidence |
|---|---|
| Generation ceiling / retirement probe (now snapshotting waiting-FIFO state too) | `M00_08_IPC_GENERATION_CEILING` -> `M00_08_IPC_SLOT_RETIREMENT` |
| C02 recv-before-send | `M00_08_IPC_C02_WOKE_DELIVERED` (U-mode receiver verified full payload + sender); structured events prove wait-enqueue -> send-wake -> result-publish -> READY |
| C05 two-receiver FIFO pairing | `M00_08_IPC_C05_M1_TO_R1` -> `M00_08_IPC_C05_M2_TO_R2` (each receiver verified its own payload), then `M00_08_IPC_C05_THIRD_PENDING` (a third send pends — no phantom waiter double-wake) |
| C12 destroy of a blocked receiver | structured destroy-wake/result/READY history, plus `M00_08_IPC_C12_STALE_HANDLE` (old handle send/try_recv `-EINVAL`) -> `M00_08_IPC_C12_EIDRM` (receiver resumed with `-EIDRM`) |
| C13a timer preemption while blocked | `M00_08_IPC_C13A_PREEMPT_WHILE_BLOCKED` (mtime preempted a CPU-bound task while a receiver stayed blocked) -> `M00_08_IPC_C13A_RESUMED` (receiver resumed only after the send) |
| C19 drained endpoint + reserved ABI | `M00_08_IPC_C19_DRAINED_EAGAIN`, `M00_08_IPC_CALL_REPLY_ENOSYS` |
| Summary | `M00_08_IPC_BLOCKING_RECV` (init returns `0x0804CAFE` after the full scenario) |

- `--smp 2` stress run — 64-round single-endpoint sender/receiver litmus with
  `-EAGAIN` retry (lost-wakeup / double-wakeup stress: every round's value is
  checked in order, so a lost wake hangs and a double wake shifts values):

| Requirement | Evidence |
|---|---|
| Both harts participate in block/wake traffic | overflow-free per-hart trace; checker requires both expected harts |
| A block on one hart woken from the other hart | checker correlates waiter TaskId and requires at least one differing-hart wake |
| Every blocked receiver woken exactly once | checker reconstructs each endpoint waiter FIFO and requires one wake, one result publication and one later READY publication per waiter, with none left pending |
| C19 workload correctness under any interleaving | `M00_08_IPC_C19_SENDER_DONE`, `M00_08_IPC_C19_RECEIVER_DONE`, `M00_08_IPC_C19_DRAINED_EAGAIN` |

The trace checker is a separate hosted tool; the firmware records facts but never
prints a self-certified concurrency conclusion. Marker lines remain useful for
single-hart workload assertions, not as the SMP history oracle.

M00-08.01, M00-08.02, and M00-08.03.01 regressions remain required and green in CI
(the .03.01 script proves C01/C03/C14/C14b/C15/C16 unchanged).

## 6. Explicitly out of scope (verified absent)

`ipc_call`/`ipc_reply` (return `-ENOSYS`); ReplyToken/Transact; timeouts; cancellation;
capability tables; registry/name routing; IPI/doorbell; dynamic memory; full task-exit
IPC cleanup (a task that ends while still queued in a waiting FIFO is a fail-closed
kernel invariant break, not a cleanup path).
