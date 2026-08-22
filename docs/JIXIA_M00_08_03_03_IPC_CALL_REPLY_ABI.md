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
- **Revision 3 (2026-08-22):** third review round applied — `end_task`
  restricted to the current task (the non-current-task teardown lifecycle race
  is closed structurally, §7), the frozen `Task.state_info` blocked-state
  contract (§3.1), the acquire-release atomic `reply_obligation_count` contract
  with bounded counts and checked increment/decrement (§7), destroy-first
  staleness wording corrected (endpoint epoch mismatch **or** endpoint
  inactive/retired → `-EINVAL`, §5), the strengthened C31 hermetic probe
  contract, and two more acceptance cases C32/C33 (§13).
- **Revision 4 (2026-08-22):** fourth review round applied — the
  `reply_obligation_count` mutations are frozen as **conditional checked
  atomic RMWs** (a compare-and-swap loop or equivalent conditional RMW that
  commits `old → old+1` only when `old < 256` and `old → old-1` only when
  `old > 0`; overflow/underflow/double decrement fail closed **before** an
  invalid value is written — not a fetch-add/fetch-sub-then-inspect contract,
  §7.3, C32); the §7.2 residue preflight wording is frozen with a per-field
  access discipline (`reply_obligation_count` is the true cross-endpoint-lock
  atomic; `message_wait.queued`/`call_wait_token` are membership/lifecycle
  fields protected structurally by the current-task-only rule and observed
  after wake via the endpoint → runqueue publication chain — no mixed
  plain/atomic undefined contract); and the delivery-path entry contract
  fixes the kernel-side receiver identity: `try_recv(Task& receiver,
  uint64_t handle, Message* message_out)` (and the `recv` equivalent) so
  bind → obligation++ → expose-token happen inside the endpoint critical
  section on both delivery paths — syscall-layer post-unlock binding is
  forbidden (§4, §12).
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
9. **`end_task()` ends only the current task (frozen).** At entry,
   `TaskManager::end_task()` must verify the target is the calling hart's
   `current_task`; `ending_current == false` fails closed **immediately** —
   before any Task state mutation, before switching `current_task`, before any
   tracker mutation, and before `release_task_locked()` frees the Task slot.
   For the current task, a residue preflight over `message_wait.queued`,
   `call_wait_token`, and `reply_obligation_count` also fails closed before
   the first teardown step, with a frozen **per-field access discipline**
   (§7.2): only `reply_obligation_count` is a true atomic — it is the one
   field mutated concurrently across different endpoint locks (conditional
   checked RMW, §7.3); `message_wait.queued` and `call_wait_token` are
   membership/lifecycle fields, mutated only inside the owning endpoint's
   critical section, safe to read here because the current-task-only rule
   structurally excludes a concurrent waiter-pop/call delivery for the legal
   current task, and observed after wake through the endpoint → runqueue
   publication ordering. `.03.03` promises no retry; `release_task_locked()`
   stays as the second line of defense (§7). The preflight is a residue check
   for the *current* task only — it is **not** a concurrent
   non-current-task teardown guard (that race is closed structurally by the
   current-task-only rule; §7). Non-current-task kill, asynchronous crash
   cleanup, and recoverable exit are all M00-08.03.04.
10. **`reply_obligation_count` never gates scheduling.** A server holding live
    tokens must remain fully schedulable — yield, preemption, requeue, and the
    eventual `ipc_reply` all keep working; the count is checked only at task
    end/release.
11. **Generation ceilings retire, never wrap** — for endpoint slots (existing
    .03.01 behavior: destroy at the ceiling permanently retires the slot; the
    epoch does not advance) and for transaction slots (a consumed/invalidated
    slot at `0xFFFFFF` goes `RETIRED` for the remainder of the endpoint epoch).
12. **`Task.state_info` blocked-state contract frozen (§3.1):** a recv-blocked
    task carries `state = blocked_message` with `state_info = Endpoint*`; a
    call-blocked caller carries `state = blocked_message` with
    `state_info = Transaction*` and `call_wait_token` = the ReplyToken.
    `call_wait_token` is the scheduling/release membership guard;
    `state_info` is kernel diagnostics and wait-object location only. Both
    delivery paths (direct delivery and queued call) install the identical
    caller blocked state, and every failure path leaves no `state_info`,
    token, or transaction residue.
13. **`reply_obligation_count` atomic contract frozen (§7):** initialized to 0
    at task creation; theoretical maximum 256 (16 endpoints × 16 transaction
    slots); increments/decrements are lock-free **conditional checked atomic
    RMWs** — a compare-and-swap loop or equivalent conditional atomic RMW
    that commits `old → old+1` only when `old < 256` and `old → old-1` only
    when `old > 0`; a successful commit carries acquire-release semantics and
    the end/release observation is an atomic acquire load; overflow,
    underflow, and double decrement fail closed **before** an invalid value
    is written (the check is part of the atomic transition itself, not a
    fetch-add/fetch-sub-then-inspect step). The count gates task end/release
    only — never scheduling, preemption, yield, or reply.

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
| `ipc_reply` (12) | `a0` = ReplyToken, `a1..a4` = reply words 0..3 | `a0` = 0 | `-EINVAL` (22) malformed token **or** stale token (endpoint epoch mismatch / endpoint inactive or retired / retired transaction slot / already consumed = duplicate); `-EACCES` (13) well-formed live token bound to a different server TaskId (probe only — does not consume) |
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
| FREE → DELIVERED_WAITING_REPLY | `ipc_call`: waiter present, slot free | **frozen order:** pop waiter → bind `server_task`/`server_tid` (= the waiter) → conditional checked atomic `server_task->reply_obligation_count++` (commits only `old < 256`, §7.3) → write the waiter's `a0..a6` (`a0=0`, `a1..a4` request, `a5` caller TaskId, `a6` token) → install the caller's frozen blocked state (transaction state, `blocked_message`, `call_wait_token`, `state_info` = `&Transaction` — §3.1) → `add_task(waiter)` → **never touch the waiter again** → switch the caller's hart `current_task` → release the endpoint lock (`.03.02` protocol) |
| REQUEST_PENDING → DELIVERED_WAITING_REPLY | recv/try_recv pops the request | bind `server_task` (= the popping, currently running task) / `server_tid` → conditional checked atomic `reply_obligation_count++` (commits only `old < 256`, §7.3) **before** the token is visible in the popper's return registers (same critical section; the popper is the running task, so no READY publication is involved) |
| DELIVERED_WAITING_REPLY → ANSWERED → FREE | `ipc_reply` consume | **frozen wake order (§3.1):** reply words written into the caller's saved registers (`a0=0`, `a1..a4` reply, `a5` replier TaskId) → `call_wait_token` cleared → `state_info` cleared → caller READY via `add_task` → **never touch the caller again**; slot `generation++` — or slot → `RETIRED` when the generation was `0xFFFFFF`; server's `reply_obligation_count--` via the stored `server_task` pointer (conditional checked decrement — commits only `old > 0`, §7.3; final decrement of this critical section — no dereference after) |
| REQUEST_PENDING / DELIVERED_WAITING_REPLY → DEAD → FREE | endpoint destroy | **frozen wake order (§3.1):** caller's `a0 = -EIDRM` written exactly once → `call_wait_token` cleared → `state_info` cleared → caller READY via `add_task` → no further caller access; token invalidated; only delivered transactions have a server binding — their `server_task->reply_obligation_count--` is the final touch of that pointer in this critical section; slot `generation++` (or → `RETIRED` at the ceiling) |

Invariants (kernel-enforced, fail closed):

- every transaction in `REQUEST_PENDING` or `DELIVERED_WAITING_REPLY` has
  exactly one blocked caller: `TaskState::blocked_message` with
  `Task.call_wait_token` == the token's raw value and
  `Task.state_info` == `&Transaction` (§3.1), and that Task is in no runqueue
  and no endpoint waiting FIFO;
- the reply payload is written into the caller's saved registers, then
  `call_wait_token` and `state_info` are cleared, all **strictly before** the
  caller is published READY (the frozen wake order of §3.1, inside the reply
  critical section);
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

### 3.1 Blocked-state contract (`state_info` / `call_wait_token`) — frozen

`Task.state_info` is an existing field (it already names the object a blocked
task waits on, mirroring Hostboot); `.03.03` freezes exactly how the two
message-blocking waits use it. No new `TaskState` value exists (§0.3):

```text
recv-blocked (blocked in ipc_recv):
    state      = blocked_message
    state_info = Endpoint*

call-blocked (blocked in ipc_call):
    state           = blocked_message
    state_info      = Transaction*
    call_wait_token = ReplyToken (raw token value)
```

Role separation (frozen):

- **`call_wait_token` is the scheduling/release membership guard.** The
  scheduler and `TaskManager` refuse membership changes and task end/release
  on `message_wait.queued` and `call_wait_token != 0` — and on nothing else
  (§6.3).
- **`state_info` is kernel diagnostics and wait-object location information
  only.** It identifies which `Endpoint` or `Transaction` a blocked task is
  waiting on (crash dumps, invariant checks, debugging); it is never read by
  the scheduler, never used as a membership guard, and never dereferenced to
  make a scheduling or task-lifetime decision.

Wake order when a call is answered by `ipc_reply` or invalidated by endpoint
destroy — the sequence inside the endpoint critical section must contain, in
this order:

```text
write the caller's return registers
→ clear call_wait_token
→ clear state_info
→ add_task(caller)
→ never access the caller again
```

Consistency and residue rules (frozen):

- the **direct-delivery** path (waiter present, §3) and the **queued-call**
  path (request popped by recv/try_recv, §3) must install the **identical**
  caller blocked state — `blocked_message` + `state_info = Transaction*` +
  `call_wait_token` = token — so no kernel code ever needs to distinguish the
  two paths when locating or waking a blocked caller;
- no failure path may leave residue: if `ipc_call` fails, or a wake is
  aborted, the caller's `state_info`, `call_wait_token`, and the transaction
  record must all be back to their pre-call values (no dangling
  `state_info`, no stale token, no half-allocated transaction) — this
  composes with the residue-free allocation ordering of §4;
- on every successful wake (reply or destroy), both fields are cleared before
  READY publication per the frozen wake order above.

## 4. Relationship to the existing queues

`ipc_call` reuses the send paths wholesale — no second queue exists:

- **Waiting receiver present:** send's fast path plus the frozen ordering
  additions of §3 — pop exactly one waiter, bind it as `server_task`/`server_tid`,
  conditional checked obligation++ (§7.3), write its return registers
  (`a5` = caller TaskId, `a6` = token), install the caller's frozen blocked
  state (§3.1 — identical to the queued-call path's), then `add_task` inside
  the lock and never touch the waiter again. No handoff, no forced yield.
- **No waiter:** the request is enqueued in the **same depth-16 FIFO** as plain
  sends. `Message` gains a `uint64_t reply_token` field (`0` = plain). Plain
  sends and calls interleave in strict arrival order under the endpoint lock.
- **Both `recv` and `try_recv`** pop call requests; neither can filter. `a6`
  tells the server what it received; `a5` still identifies the caller.
- **Delivery-path entry contract (frozen, §12):** both delivery paths —
  `ipc_call`'s direct delivery (waiter present) and the queued-call pop by
  `recv`/`try_recv` — share **one** bind → obligation++ → expose-token
  contract executed inside the endpoint critical section. The kernel-side
  delivery entry must therefore carry the receiving Task explicitly:
  `try_recv(Task& receiver, uint64_t handle, Message* message_out)` and the
  strictly equivalent `recv` entry (the current main-internal
  `try_recv(uint64_t handle, Message* message_out)` shape is insufficient —
  it hides the receiver from the pop path). Binding the server at the
  syscall layer after the endpoint lock is released is forbidden: by the
  time the token is visible in the receiver's registers (`a6`), the
  transaction must already be bound (`server_task`/`server_tid`) and the
  obligation already incremented.
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
   `call_wait_token`, clear `state_info` (the frozen wake order of §3.1),
   publish READY;
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
| destroy first | endpoint epoch mismatch, or endpoint inactive/retired (ceiling) → `-EINVAL` (unified stale verdict) | live transactions → callers woken `-EIDRM` | caller gets `-EIDRM`; the reply fails stale |

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
| `ipc_try_recv` pop | unchanged .03.01 (+ conditional checked obligation increment for call requests, same endpoint critical section — §4 delivery-path entry contract) | endpoint |
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
  `TaskState::blocked_message` and `state_info` = `Transaction*` per §3.1;
  not a new state value). **`state_info` is never a membership guard** — it
  is diagnostics/wait-object location only (§3.1) — and **`reply_obligation_count`
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

### 7.1 Blocked-state residue fields

- `Task.call_wait_token` (u64, 0 = none): set under the endpoint lock when the
  caller blocks in `ipc_call`; cleared by reply or destroy under the same
  endpoint lock, together with `state_info`, **before** READY publication (the
  frozen wake order of §3.1).
- `Task.state_info` follows the frozen blocked-state contract of §3.1:
  `Endpoint*` while recv-blocked, `Transaction*` while call-blocked, cleared
  on every wake before READY, never a scheduling or lifetime guard.

### 7.2 `end_task()` — current task only (frozen)

`TaskManager::end_task()` ends **only the current task**. Two entry checks,
both fail closed **before any teardown step**:

1. **Current-task check (first, immediate):** if the target is not the calling
   hart's `current_task` (`ending_current == false`), fail closed at entry —
   before modifying Task state, before switching `current_task`, before
   mutating or detaching the tracker, and before `release_task_locked()`
   could free the Task slot. `.03.03` provides **no** non-current-task exit
   path: killing another task, asynchronous crash cleanup of another task,
   and recoverable/retryable exit are all M00-08.03.04 (§7.4).
2. **Residue preflight (current task):** check the three residue conditions —
   `message_wait.queued`, `call_wait_token`, and `reply_obligation_count`; if
   any is nonzero, fail closed before the first teardown step. The three
   fields are deliberately **not** one uniform "three atomic acquire loads"
   contract; the per-field access discipline is frozen:
   - `reply_obligation_count` is a **true atomic**: it is the one field
     mutated concurrently by harts holding *different* endpoint locks
     (§7.3), so its mutations are conditional checked atomic RMWs and its
     preflight/`release_task_locked` reads are atomic acquire loads.
   - `message_wait.queued` and `call_wait_token` are **membership/lifecycle
     fields**: they are mutated only inside the owning endpoint's critical
     section, never across endpoint locks. For the *legal current task* no
     concurrent waiter-pop or call delivery can be mutating them — a task
     being delivered to is blocked and descheduled, hence not the running
     `current_task` of any hart (the same structural argument as entry
     check 1) — and their post-wake values are ordered by the endpoint →
     runqueue publication chain (delivery critical section → `add_task`
     release → schedule acquire) before the woken task can run `end_task`.
     They are read here under that structural + ordering guarantee, each
     with exactly one access discipline kernel-wide (a field is either an
     atomic or it is not; **no mixed plain/atomic access contract exists**).
   This placement is mandatory: the existing `end_task()` sequence is
   `state = ended` → switch this hart's `current_task` → mutate / detach the
   tracker → `release_task_locked()`, so a check performed only at release
   would fire after the task is already half-dismantled. Fail-closed here is
   the standard kernel invariant discipline (the hart parks); `.03.03`
   **does not promise a retryable exit** — a task exiting while it still
   waits on calls or holds unconsumed tokens is a programming error under
   this ABI, and the kernel refuses to tear it down.

**Why current-task-only is a structural closure, not a convenience.** The
three preflight checks are a residue check for the current task; they are
**not** sufficient to guard concurrent teardown of a *non-current* task, and
this document does not claim otherwise. The closed window (recorded so no
future round reintroduces it):

```text
server blocked in an endpoint's receiver waiting FIFO
H1 (holds the endpoint lock, delivery in progress):
    waiter_pop(server) — message_wait.queued becomes 0
H2 (holds no endpoint lock): non-current end_task(server) loads
    message_wait.queued    == 0
    call_wait_token        == 0
    reply_obligation_count == 0
H2: proceeds — releases the Task slot
H1: continues its critical section — binds server_task / obligation++ /
    writes the waiter's return registers
=> use-after-free on the released Task
```

H2's three loads are unsynchronized with H1's in-flight delivery critical
section (H2 holds no lock that orders it against the endpoint lock), so
"all three read zero" cannot mean "no delivery is in flight". The race is
therefore closed structurally, by entry check 1: a task that is being
delivered to (a waiting-FIFO member being popped, or a call-blocked caller
being answered) is, by construction, blocked and descheduled — it is not the
running `current_task` of any hart, so it cannot legally be the target of
`end_task`. Conversely, once a woken server is actually scheduled and calls
`end_task`, the delivery critical section has long completed, and the residue
is visible to the preflight through the publication chain: obligation
increment (conditional checked acquire-release RMW, §7.3) → READY
publication (`add_task`, runqueue lock release) → schedule (runqueue lock
acquire) → `end_task` preflight (atomic acquire load of the count; the
membership fields ride the same chain per the per-field access discipline
above). For the current task the preflight is therefore exact, not racy.

- `release_task_locked()` keeps refusing on the same three conditions as the
  **second line of defense**, catching any path that could bypass the
  `end_task()` entry checks.
- Race analysis (frozen, current task only): the preflight load may observe a
  nonzero count that a concurrent destroy is about to clear → refusal, which
  is always the safe answer. The reverse — observing 0 while a delivery to
  this task is in flight — cannot happen for the current task: the increment
  precedes the token becoming visible to the server (and the server's READY
  publication) in the same delivery critical section (§3), and the server
  cannot run `end_task` before it is scheduled. No re-attempt is guaranteed
  or required.

### 7.3 `reply_obligation_count` — atomic contract (frozen)

`Task.reply_obligation_count` is incremented when a task's recv/try_recv
delivers a call request (the task now holds a live ReplyToken) and decremented
when that token dies — successful reply consume, or destroy invalidation.
Mutations happen under an endpoint lock, but one server may hold tokens from
several endpoints, so two harts holding *different* endpoint locks may mutate
the same server's count concurrently. The frozen contract:

- **Initialization:** `0` at task creation — the task-create path initializes
  the field, and a recycled Task slot always restarts at 0.
- **Bound:** the theoretical maximum is **256** — 16 endpoints × 16
  transaction slots per endpoint; no valid kernel state exceeds it.
- **Mutation primitive:** a lock-free **conditional checked atomic RMW** — a
  compare-and-swap loop (or an equivalent single conditional atomic RMW
  primitive) in which the bound check is part of the atomic transition
  itself. This is deliberately **not** a fetch-add/fetch-sub-then-inspect
  contract: an unconditional RMW that has already modified the value and only
  afterwards examines the returned old value can transiently write an
  invalid count (and, on failure, requires an unrecoverable repair store);
  the conditional RMW instead never writes unless the transition is legal.
  Never a plain unprotected integer, never a non-atomic
  load-modify-store pair.
- **Success ordering:** a committed increment or decrement carries
  **acquire-release semantics** — it publishes this critical section's
  effects (transaction state, register writes, bindings) and acquires all
  prior mutations by other endpoint locks on the same count.
- **End/release checks:** atomic acquire loads (the §7.2 preflight;
  `release_task_locked`).
- **Checked increment:** the conditional RMW commits `old → old + 1` only
  when `old < 256`; observing `old ≥ 256` (overflow) fails closed **without
  writing** — the count never leaves its valid range, even transiently.
- **Checked decrement:** the conditional RMW commits `old → old - 1` only
  when `old > 0`; observing `old == 0` (underflow — a decrement without a
  matching increment, i.e. a double decrement) fails closed **without
  writing**.
- **Fail-closed set:** overflow, underflow, and repeated/duplicate decrement
  are all kernel invariant violations → fail closed **before any invalid
  value is written** (C32 exercises exactly this semantics under
  cross-endpoint contention).
- **Scheduling neutrality (load-bearing, §6.3):** the count gates task
  end/release exclusively. It must never prevent a server from being
  scheduled, preempted, requeued, yielding, or executing `ipc_reply` — a
  token-holding server that could not run could never reply, and the
  obligation would never drain.
- `reply_obligation_count > 0` is itself the lifetime guarantee for every
  stored `server_task` pointer: while a transaction is live, the §7.2 entry
  checks refuse the server's exit, so the Task object cannot be released;
  each critical section that performs a final decrement (reply consume,
  destroy) never dereferences the pointer afterwards (§2.2).

### 7.4 Deferred summary

**Deliberately deferred to M00-08.03.04:** non-current-task kill,
asynchronous crash cleanup of another task, recoverable/retryable exit,
revoking outstanding tokens on server exit, waking clients when their server
dies, client cancellation, timeouts, and any non-fail-closed cleanup path.
M00-08.03.03 ships only the constraints above.

## 8. Explicitly out of scope

Timeouts and cancellation; capability tables; service registry / name routing;
provider / page-fault IPC; dynamic allocation; task kill / asynchronous crash
cleanup / recoverable exit — in particular every **non-current-task** exit
path (`TaskManager::end_task()` ends only the current task, §7.2; the full
task-exit IPC cleanup is M00-08.03.04); any benchmark or verification
infrastructure change (`verification/`, host torture, trace checker, and the
nightly benchmark are untouched by this increment).

## 9. Error codes (frozen, asm-generic numbering)

| Condition | Value | Operations |
|---|---|---|
| malformed endpoint handle / malformed ReplyToken (tag or field range) | `-EINVAL` (22) | call, reply, recv, try_recv, send, destroy |
| stale endpoint handle / stale or duplicate ReplyToken (epoch mismatch, endpoint inactive/retired, retired slot, consumed) | `-EINVAL` (22) | call, reply |
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
| blocked caller Task | scheduler, via the transaction record | call block | reply / destroy wake (frozen wake order §3.1: registers → clear token → clear `state_info` → READY) | woken exactly once with `-EIDRM` |
| user ReplyToken | receiving server | delivery (`a6`) | a single `ipc_reply` | invalid — endpoint epoch mismatch, or endpoint inactive/retired (ceiling); obligation decremented |
| `reply_obligation_count` | server task | delivery increment (initialized 0 at task creation; bounded 256; conditional checked acquire-release RMW, §7.3) | consume / destroy decrement (conditional: commits only `old > 0`) | decremented for delivered transactions |
| `call_wait_token` | caller task | call block | reply / destroy (before READY) | cleared before READY |
| `Task.state_info` | kernel (blocked-state diagnostics, §3.1) | call / recv block (`Transaction*` / `Endpoint*`) | wake: cleared before READY | cleared before READY |

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
| `microkernel/core/ipc_manager.h/.cpp` | `Transaction` + per-endpoint `table[16]`; token encode/decode; `call`/`reply`; destroy extension; `Message.reply_token`; **widen the internal recv/try_recv entries to carry the receiving Task explicitly** — `try_recv(Task& receiver, uint64_t handle, Message* message_out)` and the strictly equivalent `recv` entry (superseding main's `try_recv(uint64_t handle, Message* message_out)`), so bind + conditional checked obligation++ complete inside the endpoint critical section before the token is exposed in `receiver`'s registers — the shared bind → obligation++ → expose-token contract of both delivery paths (§4) |
| `microkernel/core/task.h` | `call_wait_token`; atomic `reply_obligation_count` (conditional checked RMW — CAS loop, §7.3); `message_wait.queued`/`call_wait_token` stay membership fields with their frozen single access discipline (§7.2); no new field for `state_info` — the existing field follows the §3.1 contract |
| `microkernel/core/task_manager.cpp` | `end_task()` current-task-only entry refusal (`ending_current == false` → fail closed before any teardown) + residue preflight (per-field access discipline, §7.2) + `release_task_locked` refusals; task-create initialization (`reply_obligation_count = 0`, `call_wait_token = 0`, `state_info = nullptr`) |
| `microkernel/core/scheduler.cpp` | membership guards extended to the call-wait marker |
| `microkernel/core/task_syscall.cpp` | handlers for 9/12; pass the calling task (the receiver) into the widened `recv`/`try_recv` kernel entries — **never bind the server at the syscall layer after the endpoint lock is released** (§4); recv/try_recv `a6` write |
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
| C32 cross-endpoint obligation atomicity | one server obtains one live ReplyToken from endpoint A and one from endpoint B (`OBLIGATION_EXACTLY_2`); two harts concurrently execute `reply(A)` and `destroy(B)`; final `reply_obligation_count` is exactly 0 (`OBLIGATION_EXACTLY_0`); both callers resume exactly once; no lost update, no underflow, no double wake — the mutations must be conditional checked RMWs (§7.3: each commit is conditional — increment only `old < 256`, decrement only `old > 0`; an invalid value is never written even transiently, and the final exactly-0 count proves no lost update or underflow occurred under cross-endpoint contention) |
| C33 task-end lifecycle guard | a server holding a live token triggers task END: refusal fires **before any teardown** — Task state, `current_task`, tracker, and Task slot are all unmodified/unreused afterwards (asserted by post-refusal state comparison); the probe exercises the same entry judgment as production `end_task()` (see the C33 probe discipline below) |
| obligations | `SERVER_OBLIGATION_ZERO` at scenario end |
| summary | `M00_08_IPC_CALL_REPLY` |

The .03.01/.03.02 scripts remain required regressions; no existing marker may
change.

**C31 hermetic probe contract (frozen).** The transaction-generation-ceiling
probe is a white-box, strictly probe-gated kernel hook (mirroring the .03.01
ceiling probe). It must be hermetic — the snapshot/restore coverage includes
at least:

- endpoint `active`, `retired`, `generation`, `owner`;
- message FIFO head/count and every complete `Message`, including its
  `reply_token`;
- waiting-FIFO head/tail/count and node membership;
- the full transaction table: `state`, `generation`, `caller`,
  `server_task`, `server_tid`, `request[4]`;
- the probe-involved Task state: `state`, `state_info`, `call_wait_token`,
  `reply_obligation_count`, and return registers;
- allocator/table slot status.

Probe discipline (frozen): both success and failure paths restore the full
snapshot before returning; `Spinlock` objects are never copied, assigned, or
reset (the snapshot holds shadow copies of lock-guarded data taken while the
lock is held); probe buffers are static — the probe must not consume boot
stack space; the default no-probe build neither executes nor carries the test
workload; benchmark and verification remain separate suites, and production
keeps only the strictly probe-gated hook.

**C33 probe discipline (frozen).** The real `end_task()` refusal parks the
hart (fail-closed discipline), so the acceptance probe must either use a
dedicated hart for the refusing END, or route through a probe-gated shared
preflight helper that production `end_task()` itself calls for the same
entry judgment. Testing a stand-in function unrelated to production
`end_task()` is not acceptable — C33 must demonstrate that the production
entry path refuses, not that a look-alike does.
