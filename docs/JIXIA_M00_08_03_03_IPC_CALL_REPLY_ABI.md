# Jixia M00-08.03.03 — Call/Reply + ReplyToken ABI (Candidate)

- **Date:** 2026-08-21
- **Status:** CANDIDATE / FROZEN FOR REVIEW. This is a design-only increment: no
  production IPC code exists or is written under this status. The document must be
  reviewed and re-marked `FROZEN FOR IMPLEMENTATION` (separate docs commit) before
  any implementation increment starts, and `ACCEPTED` only after merge with CI
  evidence recorded in the ledgers.
- **Revision 2 (2026-08-21):** second review round applied — transaction
  `server_task` pointer binding, the frozen direct-delivery publication order,
  `end_task` preflight fail-closed semantics, the obligation/scheduling
  separation, explicit endpoint and transaction ceiling retirement (no wrap),
  and four added acceptance cases (§13).
- **Builds on:** `docs/JIXIA_M00_08_03_01_IPC_NONBLOCKING_ABI.md` (accepted) and
  `docs/JIXIA_M00_08_03_02_IPC_BLOCKING_RECV_ABI.md` (accepted: squash
  `ba27c4c1a520ae817a1980c764c89581518a50fd`, PR #30, CI run `32460452557`).
- **Research candidate mapping:** closes the ReplyToken scope and the blocking
  call path of `docs/research/JIXIA_M00_08_03_IPC_ARCHITECTURE_CANDIDATE.md`
  (UNRESOLVED-4 and the call half of the UNRESOLVED-5/8 family); task-exit IPC
  cleanup (the UNRESOLVED-6 family) is deliberately split to **M00-08.03.04**;
  timeouts, cancel, capability tables, and registry routing remain open.

## 0. Review corrections locked into this candidate

1. **Sender identity is never displaced.** `recv`/`try_recv` keep `a5` = sender
   TaskId and add `a6` = ReplyToken (0 for plain `ipc_send` messages); there is
   no separate boolean discriminator register.
2. **ReplyToken is a full 64-bit self-locating encoding** (endpoint index +
   endpoint epoch + transaction slot + transaction generation + tag), so
   `ipc_reply` takes exactly one endpoint lock. No global transaction lock
   exists: the transaction table is 16 static slots per endpoint, guarded by
   that endpoint's existing lock.
3. **No new `TaskState` value.** A call-waiting caller stays `blocked_message`
   (`'M'`) with an explicit `Task.call_wait_token` marker; membership stays
   runqueue XOR recv-waiter XOR call-transaction-wait.
4. **Task-exit uses `Task.reply_obligation_count`** (atomic counter, §7);
   release is refused while it is nonzero. The full server-exit / client-cancel
   cleanup stays in M00-08.03.04.
5. **`ipc_call` failure paths are residue-free:** FIFO full → no transaction
   allocated; transaction table full → no queue modification; all checks and
   allocations happen in one endpoint critical section.
6. **No permanent ABI assumption about TaskId magnitudes.** Value-type
   discrimination is per-syscall plus the frozen EndpointHandle bit-63-clear /
   ReplyToken bit-63-set rule only.
7. **The transaction stores both `server_task` (Task\*) and `server_tid`.**
   Endpoint destroy must decrement the server's obligation inside the endpoint
   lock, and the lock order forbids a TaskId→Task lookup there (that would need
   the TaskManager lock). The stored pointer is decremented atomically; after
   the final decrement of a critical section the pointer is never dereferenced
   again. `reply_obligation_count > 0` itself guarantees the pointed-to Task
   cannot have been released (§7).
8. **Direct-delivery publication order is frozen (§3):** binding and the
   obligation increment strictly precede READY publication (`add_task`); the
   woken receiver is never touched after `add_task`.
9. **`end_task()` fails closed at entry** via an atomic preflight, before any
   state, scheduler, or tracker mutation; `.03.03` promises no retry;
   `release_task_locked()` stays as the second line of defense (§7).
10. **`reply_obligation_count` never gates scheduling.** A server holding live
    tokens must remain fully schedulable — yield, preemption, requeue, and the
    eventual `ipc_reply` all keep working; the count is checked only at task
    end/release.
11. **Generation ceilings retire, never wrap** — for endpoint slots (existing
    .03.01 behavior: destroy at the ceiling permanently retires the slot; the
    epoch does not advance) and for transaction slots (a consumed/invalidated
    slot at `0xFFFFFF` goes `RETIRED` for the remainder of the endpoint epoch).

## 1. Syscall ABI (numbers and count unchanged)

`microkernel/arch/riscv/task_syscall_abi.h` keeps exactly 13 numbers; 9 and 12
activate, nothing is renumbered:

```c
#define JIXIA_TASK_SYSCALL_ENDPOINT_CREATE  6   /* unchanged */
#define JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY 7   /* unchanged */
#define JIXIA_TASK_SYSCALL_SEND             8   /* unchanged */
#define JIXIA_TASK_SYSCALL_CALL             9   /* ipc_call: specified here */
#define JIXIA_TASK_SYSCALL_RECV            10   /* unchanged op; a6 out added */
#define JIXIA_TASK_SYSCALL_TRY_RECV        11   /* unchanged op; a6 out added */
#define JIXIA_TASK_SYSCALL_REPLY           12   /* ipc_reply: specified here */
#define JIXIA_TASK_SYSCALL_COUNT           13
```

Register conventions (U-mode ECALL, `a7` = number, arguments in `a0..a4`; task
identity is never a user-supplied argument — the kernel captures the caller's
TaskId at the ECALL):

| Syscall | In | Out (success) | Errors |
|---|---|---|---|
| `ipc_call` (9) | `a0` = endpoint handle, `a1..a4` = request words 0..3 | `a0` = 0, `a1..a4` = reply words 0..3, `a5` = replier TaskId | `-EINVAL` (22) malformed/stale endpoint handle — immediate, never blocks; `-EAGAIN` (11) message FIFO full **or** transaction table exhausted — immediate, never blocks (no blocked-sender state is introduced); `-EIDRM` (43) endpoint destroyed while this caller was blocked |
| `ipc_reply` (12) | `a0` = ReplyToken, `a1..a4` = reply words 0..3 | `a0` = 0 | `-EINVAL` (22) malformed token **or** stale token (endpoint epoch mismatch / retired slot / already consumed = duplicate); `-EACCES` (13) well-formed live token bound to a different server TaskId (probe only — does not consume) |
| `ipc_recv` (10) | `a0` = handle | `a0` = 0, `a1..a4` = payload words, `a5` = sender TaskId, **`a6` = ReplyToken (0 = plain send, nonzero = call request)** | unchanged from .03.02 (`-EINVAL`, `-EIDRM`) |
| `ipc_try_recv` (11) | `a0` = handle | same shape as recv including `a6` | unchanged (`-EAGAIN` empty, `-EINVAL`) |

Frozen semantics:

- `ipc_call` blocks exactly like `ipc_recv` (the .03.02 atomic blocking
  protocol, §6.3) until a matching `ipc_reply` or endpoint destroy; no timeout,
  no cancel.
- On the `-EIDRM` wake only `a0` is defined; `a1..a6` are unspecified (matches
  the recv rule).
- `a5` always carries a real task identity: for a receiver it is the request's
  caller TaskId, for a woken caller it is the replier TaskId. `a6` is the only
  new register contract and is purely additive (§11).
- `ipc_reply` never blocks: it either consumes the token and wakes the caller,
  or fails immediately with the frozen error.
- A live transaction's caller is blocked, so the currently running replier can
  never be the caller: self-reply is structurally impossible (asserted
  kernel-side).

## 2. ReplyToken

### 2.1 Encoding (frozen)

```text
bit 63      tag = 1                       EndpointHandle bit 63 is frozen 0,
                                          so the two handle types self-
                                          discriminate on this bit alone
bits 62..39 transaction generation, 24 b   1..0xFFFFFF; 0 never published
bits 38..35 transaction slot index, 4 b    0..15 (per-endpoint static table)
bits 34..4  endpoint generation, 31 b      1..0x7FFFFFFF (= kMaxGeneration cap)
bits 3..0   endpoint index, 4 b            0..15
```

`ipc_reply` decodes the endpoint fields and takes **exactly one** endpoint
lock — the encoding must locate a unique endpoint without scanning (scanning
would break the never-two-endpoint-locks rule).

### 2.2 Binding and single use

The kernel-side transaction record (static, inside the endpoint) stores:

- endpoint index and the endpoint generation **at bind time**;
- transaction slot index and generation;
- `caller` Task pointer — kernel-private, never part of the user token;
- `server_task` Task pointer **and** `server_tid` TaskId — the identity of the
  task whose recv/try_recv delivered the request, captured by the kernel at
  delivery. The pointer exists so endpoint destroy can perform the final
  obligation decrement under the endpoint lock without any TaskId→Task lookup
  (which would require the TaskManager lock — forbidden inside an endpoint
  lock). Safety: while the transaction is live, the server's
  `reply_obligation_count > 0`, so `end_task` refuses and the Task object
  cannot be released; once a critical section performs the final decrement it
  never dereferences `server_task` again;
- the four request words (needed until delivery; the reply words are never
  stored — at reply time they go straight into the caller's saved registers).

A user ReplyToken carries no raw `Task*`, no `Transact*`, and no kernel pointer
of any kind — it is a pure typed value. The token is single-use: the first
successful `ipc_reply` consumes it; every later use is stale.

### 2.3 ABA, wrap, and staleness

- **Endpoint ABA:** destroy + recreate advances the endpoint generation; any
  old token's embedded epoch mismatches → `-EINVAL` (identical discipline to
  stale endpoint handles in .03.01). **Ceiling exception:** destroying an
  endpoint already at `kMaxGeneration` does **not** advance the epoch — the
  existing .03.01 behavior permanently retires the slot (no recreation, epoch
  frozen). Old tokens of a retired slot stay invalid not by epoch mismatch but
  because the slot can never become `active` again; reply liveness explicitly
  requires `active && !retired` (§2.4).
- **Transaction ABA:** every consume or invalidation advances the slot's
  transaction generation before the slot is reusable; a duplicate or old token
  mismatches → `-EINVAL`. After a slot is reused, an old token from a previous
  generation of that same slot is still rejected (its transaction generation
  no longer matches) while the new token succeeds.
- **Generation wrap — transaction slots:** the generation caps at `0xFFFFFF`;
  a consumed or invalidated slot at the cap transitions to an explicit
  `RETIRED` state (no increment, never reallocated) for the remainder of the
  current endpoint epoch — mirroring endpoint slot retirement. A fresh
  endpoint epoch (destroy + recreate of a non-ceiling slot) reinitializes the
  whole transaction table (all slots `FREE`, slot generations restart at 1):
  every old token is already invalid by epoch mismatch.
- **Duplicate reply** therefore equals a stale token: the first reply consumed
  it. Of two racing replies exactly one wins (the one that linearizes first
  under the endpoint lock).

### 2.4 Check order (frozen; all steps under the endpoint lock)

1. well-formedness: tag set, all fields in range → else `-EINVAL`;
2. liveness: endpoint `active && !retired` **and** epoch match **and** slot not
   `RETIRED` **and** transaction generation match **and** state ==
   `DELIVERED_WAITING_REPLY` → else `-EINVAL` (stale — covers epoch mismatch,
   retired endpoint slot, retired transaction slot, and consumed tokens);
3. server binding: bound `server_tid` == caller TaskId → else `-EACCES`
   (the probe does not consume the token; a later correct reply can succeed);
4. consume: the single state transition of §3 under the lock → success.

Wrong-server is thus always distinguishable from stale by construction: a
wrong-server verdict requires a token that is still live.

## 3. Transaction lifecycle

Per endpoint: `Transaction table[16]`, each record
`{state, generation, caller Task*, server_task Task*, server_tid TaskId,
request[4]}` — static, guarded by the endpoint's existing lock. States: `FREE`,
`REQUEST_PENDING`, `DELIVERED_WAITING_REPLY`, `ANSWERED`, `DEAD`, `RETIRED`.

```text
              call (waiter present: direct delivery)
  FREE ──────────────────────────────────────────▶ DELIVERED_WAITING_REPLY
    │                                                      │
    │ call (no waiter, FIFO space)                          │ ipc_reply (consume)
    ▼                                                       ▼
  REQUEST_PENDING ── receiver pops request ──▶ DELIVERED_WAITING_REPLY → ANSWERED → FREE
    │                                            (slot generation++ each cycle; at the
    └──────────── destroy (any live state) ────── 0xFFFFFF ceiling a consumed or
                 caller woken -EIDRM exactly once, invalidated slot goes RETIRED
                 token invalidated                    instead of FREE — never reused)
                 → DEAD → FREE (slot generation++)
```

| Transition | Trigger (all under endpoint lock) | Side effects |
|---|---|---|
| FREE → REQUEST_PENDING | `ipc_call`: no waiter, FIFO space, slot free | request enqueued in the shared FIFO carrying its token; transaction allocated |
| FREE → DELIVERED_WAITING_REPLY | `ipc_call`: waiter present, slot free | **frozen order:** pop waiter → bind `server_task`/`server_tid` (= the waiter) → atomic `server_task->reply_obligation_count++` → write the waiter's `a0..a6` (`a0=0`, `a1..a4` request, `a5` caller TaskId, `a6` token) → complete the caller's block state (transaction state, `call_wait_token`, `blocked_message`) → `add_task(waiter)` → **never touch the waiter again** → switch the caller's hart `current_task` → release the endpoint lock (`.03.02` protocol) |
| REQUEST_PENDING → DELIVERED_WAITING_REPLY | recv/try_recv pops the request | bind `server_task` (= the popping, currently running task) / `server_tid` → atomic `reply_obligation_count++` **before** the token is visible in the popper's return registers (same critical section; the popper is the running task, so no READY publication is involved) |
| DELIVERED_WAITING_REPLY → ANSWERED → FREE | `ipc_reply` consume | reply words written into the caller's saved registers (`a0=0`, `a1..a4` reply, `a5` replier TaskId) **strictly before** READY, `call_wait_token` cleared, caller READY via `add_task`, no further caller access; slot `generation++` — or slot → `RETIRED` when the generation was `0xFFFFFF`; server's `reply_obligation_count--` via the stored `server_task` pointer (final decrement of this critical section — no dereference after) |
| REQUEST_PENDING / DELIVERED_WAITING_REPLY → DEAD → FREE | endpoint destroy | caller woken with `a0 = -EIDRM` exactly once, `call_wait_token` cleared, caller READY; token invalidated; only delivered transactions have a server binding — their `server_task->reply_obligation_count--` is the final touch of that pointer in this critical section; slot `generation++` (or → `RETIRED` at the ceiling) |

Invariants (kernel-enforced, fail closed):

- every transaction in `REQUEST_PENDING` or `DELIVERED_WAITING_REPLY` has
  exactly one blocked caller: `TaskState::blocked_message` with
  `Task.call_wait_token` == the token's raw value, and that Task is in no
  runqueue and no endpoint waiting FIFO;
- the reply payload is written into the caller's saved registers **strictly
  before** the caller is published READY (both inside the reply critical
  section);
- a transaction is consumed at most once (single generation-advancing
  transition under the lock);
- a server's `reply_obligation_count` equals its count of live
  `DELIVERED_WAITING_REPLY` transactions (aggregate across endpoints);
- the obligation increment **strictly precedes** both the token becoming
  visible to the server (its `a6` return register / READY publication) and the
  server being scheduled — a server can never observe or reply with a token
  its own obligation count does not yet cover;
- after any final obligation decrement, the stored `server_task` pointer is
  not dereferenced again in that critical section;
- a `RETIRED` transaction slot is never allocated; `RETIRED` clears only when
  a fresh endpoint epoch reinitializes the table.

## 4. Relationship to the existing queues

`ipc_call` reuses the send paths wholesale — no second queue exists:

- **Waiting receiver present:** send's fast path plus the frozen ordering
  additions of §3 — pop exactly one waiter, bind it as `server_task`/`server_tid`,
  atomic obligation++, write its return registers (`a5` = caller TaskId, `a6` =
  token), complete the caller's block state, then `add_task` inside the lock and
  never touch the waiter again. No handoff, no forced yield.
- **No waiter:** the request is enqueued in the **same depth-16 FIFO** as plain
  sends. `Message` gains a `uint64_t reply_token` field (`0` = plain). Plain
  sends and calls interleave in strict arrival order under the endpoint lock.
- **Both `recv` and `try_recv`** pop call requests; neither can filter. `a6`
  tells the server what it received; `a5` still identifies the caller.
- **Full endpoint:** `ipc_call` returns `-EAGAIN` immediately — a caller is
  never parked on a full queue (keeps the .03.01 UNRESOLVED-2 lean; no
  blocked-sender state exists).
- **Residue-free failure ordering (frozen):** within one endpoint critical
  section — (1) validate the handle; (2) check transaction slot availability;
  (3) check FIFO space / waiter presence; (4) only then allocate and
  enqueue/deliver. FIFO full → no transaction allocated. Transaction table
  full → no queue modification. A "transaction allocated but request not
  enqueued" half-state cannot occur.
- The .03.02 invariant is preserved: `waiting_count > 0 && count > 0` never
  holds (call's fast path pops a waiter exactly like send).

## 5. Destroy semantics

One critical section (`table_lock_` → endpoint lock), extending .03.02:

1. `active = false`; endpoint generation++ — **except at the generation
   ceiling**, where the existing .03.01 behavior permanently retires the slot
   (epoch frozen, no recreation) instead of advancing the epoch;
2. wake every waiting receiver with `-EIDRM` (existing .03.02);
3. **new:** transition every live transaction of this endpoint to DEAD — wake
   each blocked caller with `a0 = -EIDRM` **exactly once**, clear
   `call_wait_token`, publish READY;
4. all unconsumed tokens of this endpoint are stale — by epoch mismatch for a
   recreated slot, or because a retired slot can never become `active` again
   (old endpoint handles keep their existing `-EINVAL` behavior);
5. for each transaction that had been delivered, decrement its server's
   `reply_obligation_count` **via the stored `server_task` pointer** (no
   TaskManager lookup, which the lock order forbids here);
   `REQUEST_PENDING` transactions have no server binding and no decrement.
   Each decrement is the final touch of that pointer in this critical section —
   it is never dereferenced afterwards.

**reply vs destroy race — deterministic outcomes** (both need the same
endpoint lock; the first holder linearizes):

| Order | reply observes | destroy observes | Result |
|---|---|---|---|
| reply first | live token → consumed, caller READY with the reply | no live transaction in its scan | caller keeps the reply; no `-EIDRM` wake |
| destroy first | epoch mismatch → `-EINVAL` | live transactions → callers woken `-EIDRM` | caller gets `-EIDRM`; the reply fails stale |

Exactly one outcome per race: a caller can never receive both a reply and a
destroy wake.

## 6. Concurrency rules

### 6.1 Linearization points

| Operation | Linearization point | Lock |
|---|---|---|
| `ipc_call` direct delivery | waiter popped + bind + obligation++ + registers written + `add_task` (§3 frozen order) | endpoint |
| `ipc_call` enqueue | transaction allocated + request enqueued | endpoint |
| `ipc_reply` | consume transition (`DELIVERED_WAITING_REPLY → ANSWERED`) + caller registers written + `add_task` | endpoint |
| `ipc_recv` block / deliver | unchanged .03.02 | endpoint |
| `ipc_try_recv` pop | unchanged .03.01 (+ obligation increment for call requests) | endpoint |
| `ipc_send` | unchanged .03.01/.03.02 | endpoint |
| `endpoint_destroy` | DEAD flip + epoch advance + wakes | table → endpoint |
| `endpoint_create` | slot publish | table → endpoint |

### 6.2 Lock order (unchanged, one direction)

`table_lock_ → endpoint lock → {runqueue lock, delay-queue lock}`.

- never two endpoint locks at once — every IPC syscall references exactly one
  endpoint, and the token encoding guarantees this for `ipc_reply`;
- no TaskManager lock inside an endpoint lock;
- `printk` remains a leaf lock;
- **no dynamic allocation in any critical section** — transaction tables are
  static per-endpoint arrays (16 endpoints × 16 slots); the widened `Message`
  is still a static queue entry.

### 6.3 Prohibitions and membership

- After a task is published READY (`add_task` returned, still inside the
  endpoint lock), the waker must not touch that `Task` again — another hart
  may schedule it and later release it.
- A `Task` is in at most one of: the runqueue, one endpoint waiting FIFO, one
  live call transaction. The **scheduling membership guards**
  (`RunQueue::insert`/`remove`, `Scheduler::add_task`,
  `TaskManager::set_current_task`) refuse — fail closed — exactly the two
  membership markers: `message_wait.queued` (recv waiter) and
  `call_wait_token != 0` (call-waiting caller, with
  `TaskState::blocked_message`; not a new state value). **`reply_obligation_count`
  is never a scheduling guard:** a server holding live tokens must remain fully
  schedulable — it yields, is preempted, requeues, and eventually replies;
  treating the count as a membership condition would deadlock the server
  forever. `TaskManager::release_task_locked` refuses all three conditions
  (both membership markers plus the obligation count) as the second line of
  defense (§7).
- The .03.02 atomic blocking protocol is reused verbatim for `ipc_call`: block
  under the endpoint lock, release the lock only after this hart's
  `current_task` has been switched, and the syscall handler never touches the
  old caller after a `blocked` result.

## 7. Task-exit boundary (fail-closed only; full cleanup → M00-08.03.04)

- `Task.call_wait_token` (u64, 0 = none): set under the endpoint lock when the
  caller blocks in `ipc_call`; cleared by reply or destroy under the same
  endpoint lock **before** READY publication.
- `Task.reply_obligation_count`: incremented when a task's recv/try_recv
  delivers a call request (the task now holds a live ReplyToken); decremented
  when that token dies — successful reply consume, or destroy invalidation.
  **Synchronization:** mutations happen under an endpoint lock, but one server
  may hold tokens from several endpoints, so two harts holding *different*
  endpoint locks may mutate the same server's count concurrently — the field
  must be a lock-free atomic RMW counter (kernel `amoadd`-class primitive),
  never a plain unprotected integer. Release-side checks are atomic loads.
- `TaskManager::end_task()` performs an **atomic preflight at entry**: atomic
  loads of `message_wait.queued`, `call_wait_token`, and
  `reply_obligation_count`; if any is nonzero it fails closed **before** the
  first teardown step. This placement is mandatory: the existing `end_task()`
  sequence is `state = ended` → switch this hart's `current_task` → mutate /
  detach the tracker → `release_task_locked()`, so a check performed only at
  release would fire after the task is already half-dismantled. Fail-closed
  here is the standard kernel invariant discipline (the hart parks); `.03.03`
  **does not promise a retryable exit** — a task exiting while it still waits
  on calls or holds unconsumed tokens is a programming error under this ABI,
  and the kernel refuses to tear it down.
- `release_task_locked()` keeps refusing on the same three conditions as the
  **second line of defense**, catching any path that could bypass the
  `end_task()` preflight.
- `reply_obligation_count > 0` is itself the lifetime guarantee for every
  stored `server_task` pointer: while a transaction is live, the preflight
  refuses the server's exit, so the Task object cannot be released; each
  critical section that performs a final decrement (reply consume, destroy)
  never dereferences the pointer afterwards (§2.2).
- Race analysis (frozen): the preflight load may observe a nonzero count that
  a concurrent destroy is about to clear → refusal, which is always the safe
  answer. The reverse — observing 0 while a delivery is in flight — cannot
  happen: the increment precedes the token becoming visible to the server
  (and the server's READY publication) in the same delivery critical section
  (§3). No re-attempt is guaranteed or required.
- **Deliberately deferred to M00-08.03.04:** revoking outstanding tokens on
  server exit, waking clients when their server dies, client cancellation,
  timeouts, retryable/revocable exit, and any non-fail-closed cleanup path.
  M00-08.03.03 ships only the constraints above.

## 8. Explicitly out of scope

Timeouts and cancellation; capability tables; service registry / name routing;
provider / page-fault IPC; dynamic allocation; task kill / crash cleanup (the
full task-exit IPC cleanup is M00-08.03.04); any benchmark or verification
infrastructure change (`verification/`, host torture, trace checker, and the
nightly benchmark are untouched by this increment).

## 9. Error codes (frozen, asm-generic numbering)

| Condition | Value | Operations |
|---|---|---|
| malformed endpoint handle / malformed ReplyToken (tag or field range) | `-EINVAL` (22) | call, reply, recv, try_recv, send, destroy |
| stale endpoint handle / stale or duplicate ReplyToken (epoch mismatch, retired slot, consumed) | `-EINVAL` (22) | call, reply |
| wrong server (live token bound to another TaskId; probe only, no consume) | `-EACCES` (13) | reply |
| endpoint destroyed while blocked (receiver or caller) | `-EIDRM` (43) | recv, call |
| message FIFO full (send, call) / transaction table exhausted (call) / empty queue (try_recv) | `-EAGAIN` (11) | send, call, try_recv |
| endpoint table full | `-ENOSPC` (28) | create (existing) |
| destroy by non-owner | `-EACCES` (13) | destroy (existing) |

## 10. Resource ownership and destruction

| Resource | Owner | Created | Released / consumed | Fate on endpoint destroy |
|---|---|---|---|---|
| endpoint slot + epoch | creator task | `endpoint_create` | `endpoint_destroy` (owner-only) | epoch advances — ceiling: slot permanently retired; handles stale either way |
| FIFO message entry | endpoint | send / call enqueue | recv / try_recv pop | entries dropped with the epoch |
| transaction slot + token | kernel (endpoint-scoped) | call (one critical section) | reply consume or destroy invalidation; consumed/invalidated slot goes `RETIRED` at the `0xFFFFFF` generation cap | all live transactions DEAD; callers woken `-EIDRM`; tokens stale |
| transaction `server_task` pointer | kernel (transaction-scoped) | delivery bind (§3 frozen order) | reply consume / destroy: final atomic decrement, never dereferenced after | decremented for delivered transactions; pointer thereafter dead |
| blocked caller Task | scheduler, via the transaction record | call block | reply / destroy wake (registers written before READY) | woken exactly once with `-EIDRM` |
| user ReplyToken | receiving server | delivery (`a6`) | a single `ipc_reply` | invalid (epoch mismatch or retired slot); obligation decremented |
| `reply_obligation_count` | server task | delivery increment | consume / destroy decrement | decremented for delivered transactions |
| `call_wait_token` | caller task | call block | reply / destroy (before READY) | cleared before READY |

## 11. Compatibility with .03.01 / .03.02

- Numbers and `COUNT` are unchanged; `endpoint_create`/`destroy`/`send`/
  `recv`/`try_recv` register contracts in `a0..a5` are bit-identical.
- `a6` on recv/try_recv is purely additive: no .03.01/.03.02 workload reads a
  register beyond `a5` (it was unspecified before), so every existing
  acceptance scenario and marker stays valid unchanged — those scripts are the
  regression gate for the implementation increment.
- The shared FIFO keeps depth 16 and strict arrival order; plain send behavior
  is unchanged (`Message.reply_token == 0`).
- The lock order is unchanged in direction and adds no lock: the transaction
  table is guarded by the endpoint's existing lock; `reply_obligation_count`
  is a lock-free atomic, not a lock.
- All .03.02 invariants survive (§4); the **scheduling** membership refusal
  set grows by `call_wait_token` only — `reply_obligation_count` gates task
  end/release exclusively and never scheduling (§6.3), so token-holding
  servers stay fully schedulable.
- No TaskId-magnitude assumption is introduced: `a5` carries a TaskId because
  the syscall says so, not because of any value-range property; bit-63
  discrimination applies only between the two frozen 64-bit handle types.

## 12. Implementation file plan (for the future implementation increment)

Design-only status: nothing below is written in this increment.

| File | Change |
|---|---|
| `microkernel/arch/riscv/task_syscall_abi.h` | activate the 9/12 comments (numbers already frozen) |
| `microkernel/core/ipc_manager.h/.cpp` | `Transaction` + per-endpoint `table[16]`; token encode/decode; `call`/`reply`; destroy extension; `Message.reply_token` |
| `microkernel/core/task.h` | `call_wait_token`; atomic `reply_obligation_count` |
| `microkernel/core/task_manager.cpp` | `end_task()` entry preflight + `release_task_locked` refusals; task-create initialization |
| `microkernel/core/scheduler.cpp` | membership guards extended to the call-wait marker |
| `microkernel/core/task_syscall.cpp` | handlers for 9/12; recv/try_recv `a6` write |
| `microkernel/arch/riscv/user_task.S`, `user_task_test_values.h` | acceptance workload (probe-gated) |
| `CMakeLists.txt` | `JIXIA_M00_08_03_03_PROBE` option/definitions |
| `scripts/test-m00-08-03-03-ipc-call-reply.sh`, `.github/workflows/ci.yml` | acceptance script + CI wiring |
| docs | flip this document `CANDIDATE / FROZEN FOR REVIEW → FROZEN FOR IMPLEMENTATION` first, as a separate docs commit |

## 13. Acceptance mapping sketch (marker names provisional; frozen at implementation)

| Case | Provisional evidence |
|---|---|
| C20 call/reply round trip | `CALL_BLOCKED → REPLY_DELIVERED` (woken caller asserted `a0=0`, four reply words, `a5` replier TaskId) |
| C21 token single use | `DUPLICATE_REPLY_EINVAL` |
| C22 wrong server | `WRONG_SERVER_EACCES → CORRECT_REPLY_SUCCEEDS` (wrong task first: `-EACCES`; the bound server then replies successfully — proving the error probe never consumed the token) |
| C23 destroy of a blocked caller | `DESTROY_WOKE_CALLER_EIDRM → STALE_REPLY_EINVAL` |
| C24 transaction exhaustion | `TXN_FULL_EAGAIN` (queue unmodified — proven by a subsequent plain send/recv) |
| C25 FIFO-full call | `CALL_FULL_EAGAIN` (no transaction allocated — proven by the transaction table still accepting a call after a pop) |
| C26 recv discrimination | plain message `a6 == 0` and call request `a6 != 0`, with `a5` = sender/caller TaskId in both |
| C27 mixed send/call ordering | strict FIFO interleaving of plain sends and calls on one endpoint |
| C28 reply/destroy race (dual hart) | per race, exactly one of {caller keeps the reply, caller woken `-EIDRM` + reply `-EINVAL`} |
| C29 direct-delivery ordering | `CALL_BIND_SERVER → OBLIGATION_INC → WAKE_READY` marker order inside the delivery critical section: binding and the obligation increment strictly precede READY publication; the woken server's obligation already covers its token when it first runs |
| C30 transaction ABA | `OLD_TOKEN_EINVAL_NEW_TOKEN_OK` (slot reused by a later call: the previous generation's token → `-EINVAL`, the new token replies successfully) |
| C31 transaction generation ceiling | `TXN_SLOT_RETIRED` — hermetic probe (mirroring the .03.01 ceiling probe) drives one slot's generation to `0xFFFFFF`: consume → `RETIRED`, never reallocated; subsequent calls succeed on other slots |
| obligations | `SERVER_OBLIGATION_ZERO` at scenario end |
| summary | `M00_08_IPC_CALL_REPLY` |

The .03.01/.03.02 scripts remain required regressions; no existing marker may
change.
