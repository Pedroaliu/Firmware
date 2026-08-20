# Jixia M00-08.03.01 — Accepted Non-Blocking Endpoint/Message IPC ABI

- **Date:** 2026-08-20
- **Status:** ACCEPTED for increment M00-08.03.01 (implementation + local acceptance PASS;
  integration via PR, CI evidence recorded at merge time).
- **Supersedes (for this increment):** the CANDIDATE leans in
  `docs/research/JIXIA_M00_08_03_IPC_ARCHITECTURE_CANDIDATE.md`.
- **Out of scope here:** every blocking/reply/token mechanism of that candidate.

## 0. Relationship to the research candidate

This document freezes the **non-blocking subset** of the M00-08.03 IPC architecture
candidate. It implements the candidate's current leans where they only concern this
subset, and leaves every other `[UNRESOLVED-n]` item open:

| Candidate item | Lean adopted here | Status |
|---|---|---|
| UNRESOLVED-1 (single owner vs any-task recv) | destroy is owner-only; **any** task holding a valid handle may `ipc_send`/`ipc_try_recv` | adopted for this increment |
| UNRESOLVED-2 (queue-full policy: block vs `-EAGAIN`) | **`-EAGAIN`, never block** (§3.1 lean) | adopted for this increment |
| UNRESOLVED-3 (capability table vs global handles) | **global typed handles**, identity checked only where the operation demands it (destroy) | adopted for this increment |
| UNRESOLVED-7 (4 vs 6 payload words) | **4 × 64-bit words** (§2.1 lean) | adopted for this increment |
| UNRESOLVED-4/5/6/8..13 (ReplyToken scope, blocking paths, task-exit, cancel, wake policy) | not touched — no such mechanism exists in this increment | still open |

`-EWOULDBLOCK` and `-EAGAIN` are the same errno value (11); the empty-queue
return of `ipc_try_recv` is therefore `-EAGAIN`, matching the candidate §3.3
semantics under the generic errno numbering.

## 1. Syscall ABI (frozen numbers, single definition site)

`microkernel/arch/riscv/task_syscall_abi.h` defines exactly once:

```c
#define JIXIA_TASK_SYSCALL_ENDPOINT_CREATE  6
#define JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY 7
#define JIXIA_TASK_SYSCALL_SEND             8
#define JIXIA_TASK_SYSCALL_CALL             9   /* reserved, -ENOSYS */
#define JIXIA_TASK_SYSCALL_RECV            10   /* reserved, -ENOSYS */
#define JIXIA_TASK_SYSCALL_TRY_RECV        11
#define JIXIA_TASK_SYSCALL_REPLY           12   /* reserved, -ENOSYS */
#define JIXIA_TASK_SYSCALL_COUNT           13
```

Register conventions (RISC-V U-mode ECALL, `a7` = number, arguments in `a0..a4`):

| Syscall | In | Out (success) | Errors |
|---|---|---|---|
| `endpoint_create` | — | `a0` = typed endpoint handle (u64, nonzero) | `-ENOSPC` (28) table full |
| `endpoint_destroy` | `a0` = handle | `a0` = 0 | `-EACCES` (13) non-owner; `-EINVAL` (22) stale/malformed |
| `ipc_send` | `a0` = handle, `a1..a4` = payload words 0..3 | `a0` = 0 | `-EAGAIN` (11) FIFO full; `-EINVAL` stale/malformed |
| `ipc_try_recv` | `a0` = handle | `a0` = 0, `a1..a4` = payload words, `a5` = sender TaskId | `-EAGAIN` FIFO empty; `-EINVAL` stale/malformed |
| `ipc_call`/`ipc_recv`/`ipc_reply` (9/10/12) | — | — | `-ENOSYS` (38), reserved for blocking increments |

The caller's TaskId is captured by the kernel at the ECALL; it is never a
user-supplied argument.

## 2. Kernel data structures (all static, no dynamic allocation)

```text
EndpointManager (singleton, boot-hart-first construction)
    Spinlock      table_lock            // guards only slot allocation bitmap
    bool[16]      slot_allocated
    Endpoint[16]  endpoints             // kMaxEndpoints = 16

Endpoint
    Spinlock      lock                  // one lock per endpoint
    bool          active                // ACTIVE | DEAD
    uint32_t      generation            // handle-validation epoch, never 0 when live
    TaskId        owner                 // creator; only owner may destroy
    size_t        head, count           // ring FIFO cursor/depth usage
    Message[16]   queue                 // kEndpointQueueDepth = 16

Message (pure value, copied at both linearization points)
    TaskId        sender
    uint64_t      words[4]
```

### EndpointHandle layout

One `uint64_t` U-mode value: **low 32 bits = table index, high 32 bits =
generation**. `EndpointHandle{index, generation}.raw()` is the ABI encoding.

- Generation 0 is the never-allocated BSS state and never appears in a live
  handle; a fresh slot is born with generation 1.
- `endpoint_destroy` advances the epoch (linearization point of the DEAD flip).
  A recreated slot keeps the post-destroy epoch, so a recycled slot's first new
  handle is exactly `old_handle + (1 << 32)` — same index, one epoch later.
- Every handle from a destroyed epoch fails closed with `-EINVAL` on send,
  try_recv, and destroy.

## 3. Semantics

### endpoint_create
- Allocates the lowest free slot, initializes owner/FIFO, publishes a typed
  handle. Generation is never 0.
- Table full → `-ENOSPC`.

### endpoint_destroy (owner-only)
- Non-owner caller → `-EACCES`. Stale/malformed handle → `-EINVAL`.
- Effects under the endpoint lock: FIFO cleared, `active = false`,
  `generation += 1`, slot released for reallocation.
- No wake/purge of blocked parties is needed: this increment defines no
  blocking state.

### ipc_send
- Validates index, generation, active; copies sender TaskId + 4 words into the
  FIFO tail. FIFO full → `-EAGAIN`.
- Never blocks, never schedules, never allocates.

### ipc_try_recv
- Any task holding a valid handle may call. Pops the FIFO head: 4 words +
  sender TaskId to return registers. Empty → `-EAGAIN`.
- Never blocks, never schedules.

## 4. Lock model and linearization points

Lock order (the only nesting allowed):

```text
create/destroy:  table_lock -> endpoint_lock      (never reversed)
send/try_recv:   endpoint_lock only
```

- Never two endpoint locks at once.
- No TaskManager or TimeManager lock is taken inside an endpoint lock; the IPC
  layer calls neither manager (the caller's TaskId is an immutable field).
- No `scheduler::add_task`, no runqueue operation, no TimeManager call exists
  in this layer — by construction, not convention.
- Linearization points: send = enqueue under ep lock; try_recv = pop under ep
  lock; destroy = DEAD flip + epoch advance under ep lock; create = slot
  publish under table + ep locks.

## 5. Explicitly out of scope (verified absent in this increment)

Blocking `ipc_recv`; `ipc_call`/`ipc_reply` (return `-ENOSYS`); ReplyToken /
Transact records; `blocked_message` state transitions; any wakeup/runqueue
operation; timeouts/cancellation; task-exit IPC cleanup; capability tables;
service registry/name routing; IPI/doorbell; dynamic memory.

## 6. Acceptance mapping

`scripts/test-m00-08-03-01-ipc-nonblocking.sh` (QEMU RV64, `--smp 1`),
deterministic across repeated runs, required-marker + ordered-marker checks:

| Case | Evidence (kernel markers) |
|---|---|
| C01 send-before-recv | `M00_08_IPC_C01_A_SENT` (before any recv attempt) → `M00_08_IPC_C01_B_GOT` (payload + sender verified) → `M00_08_IPC_C01_SEND_BEFORE_RECV` (consumer assertion) |
| C03 FIFO order | `M00_08_IPC_C03_GOT_1/2/3` in strict log order + `M00_08_IPC_C03_FIFO` |
| C14 malformed/stale | `M00_08_IPC_C14_MALFORMED` (out-of-range index, zero handle, garbage across send/try_recv/destroy) + `M00_08_IPC_C14_STALE` (post-destroy handle, after `M00_08_IPC_ENDPOINT_DESTROY`) |
| C15 destroy/recreate isolation | `M00_08_IPC_ENDPOINT_DESTROY` → `M00_08_IPC_C14_STALE` → `M00_08_IPC_C15_RECYCLED_GENERATION` (same slot, +1 epoch asserted in U-mode as `h3 == h2 + (1<<32)`) → `M00_08_IPC_C15_ISOLATION` (new epoch delivers) |
| C16 queue-full | `M00_08_IPC_C16_FULL` (17th send `-EAGAIN` at depth 16) → `M00_08_IPC_C16_POP_OLDEST` → `M00_08_IPC_C16_RECOVER` |
| Extra: owner policy | `M00_08_IPC_DESTROY_NONOWNER` (`-EACCES`) |
| Extra: capacity | `M00_08_IPC_ENDPOINT_ENOSPC` (17th endpoint) |
| Extra: reserved ABI | `M00_08_IPC_RESERVED_ENOSYS` (`ipc_call` → `-ENOSYS`) |
| Summary | `M00_08_IPC_NONBLOCKING` (init task returns `0x0803CAFE` after full scenario) |

M00-08.01 and M00-08.02 regressions remain required and green in CI.
