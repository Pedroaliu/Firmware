# Jixia M00-08.03 IPC Architecture — CANDIDATE (not an accepted ABI)

- **Date:** 2026-08-19
- **Branch:** `agent/m00-08-03-research`
- **Status:** CANDIDATE DESIGN. Nothing here is frozen; every open decision is
  marked **[UNRESOLVED-n]**. This document precedes implementation and the
  acceptance plan (`JIXIA_M00_08_03_IPC_ACCEPTANCE_PLAN.md`) is derived from it.

## 0. Fixed constraints (from project architecture — not negotiable here)

1. Jixia M-mode microkernel owns the mechanism; U-mode services are clients.
2. No Hostboot raw-pointer ABI: M-mode never dereferences U virtual pointers.
3. Initial payload fits entirely in syscall argument/return registers.
4. No user-buffer/pointer IPC in M00-08.03; safe user-copy is M00-08.04+.
5. Typed handles/identities only; kernel pointers never exposed to U-mode.
6. Single-hart acceptance first, but no SMP races designed in.
7. No IPI/remote wake in this milestone; correctness must hold with polling
   visibility across harts.
8. Message/Endpoint lifetime and ownership are explicit.
9. Project-specific behavior stays out of generic IPC code (no VFS types, no
   provider semantics in the primitive layer).

## 1. Objects under evaluation

### 1.1 Endpoint

Kernel object; the only rendezvous point. One well-known bootstrap endpoint is
created by the kernel for milestone acceptance; services later receive endpoints
by explicit grant (registry is a later milestone).

```text
Endpoint {
    Spinlock          lock;            // guards all fields below
    EpState           state;           // ACTIVE | DEAD
    Fifo<MessageSlot> pending;         // undelivered messages (fixed capacity)
    Fifo<Task*>       waiting;         // receivers blocked in recv
    List<Transact>    outstanding;     // call transactions awaiting reply
    uint32_t          generation;      // handle-validation epoch
    TaskId            owner;           // creator/receiver identity [UNRESOLVED-1:
                                      // single owner vs any-task recv]
}
```

- **MessageSlot**: kernel-owned storage (not a user pointer) holding the
  register payload, the transaction id, and the sender identity.
- Capacity: fixed small (e.g. 16) **[UNRESOLVED-2: depth + full-policy —
  block sender, or -EAGAIN]**. Fixed capacity avoids dynamic allocation inside
  the lock (JIXIA_CONCURRENCY_CORRECTNESS_RULES §7: no allocation in critical
  sections) — statically pooled slots.
- **Evaluation verdict:** Endpoint = keep. It is the minimal rendezvous that
  supports both async send and sync call; Hostboot's MessageQueue shape with
  the pointer parts removed.

### 1.2 Message

Not a kernel object with identity of its own — a **value** copied into a
MessageSlot at send LP, then copied into the receiver's syscall return
registers at recv LP. Rationale: with register-only payloads there is nothing
to reference-count; ownership transfer is exactly “endpoint queue → receiver”.
(Hostboot needed message objects because payloads were unbounded and memory
  ownership had to travel; Jixia M00-08.03 does not.)

- **Evaluation verdict:** Message-as-value = keep for M00-08.03. A heap
  Message object reappears only with buffer transfer (M00-08.04+).

### 1.3 EndpointHandle

64-bit U-mode value: `{ uint32 index; uint32 generation }` into a fixed kernel
endpoint table (kMaxEndpoints, e.g. 16, mirroring kMaxTasks=16 scale).

- Validation: index < table size ∧ generation match ∧ state==ACTIVE ∧ (policy)
  holder is permitted **[UNRESOLVED-3: per-task capability table vs global
  handles in M00-08.03]**. Current proposal for M00-08.03: global handles,
  identity-checked only where the operation demands it (reply); capability
  tables deferred to the seL4-informed later milestone.
- **Evaluation verdict:** typed index+generation handle = keep; it converts
  Hostboot's forgeable pointer into a fail-closed (-EINVAL) lookup.

### 1.4 ReplyToken / Transaction identity

64-bit value `{ uint32 endpoint_index; uint32 serial}` (or 64-bit global serial
**[UNRESOLVED-4: scope]**) naming exactly one `Transact` record:

```text
Transact {
    uint64 serial;        // never reused within an endpoint epoch
    TaskId  client;       // blocked caller (or NONE for fire-and-forget view)
    TaskId  server;       // the receiver that owns the reply obligation
    MsgPayload request;   // retained for client-resume copy-in? [UNRESOLVED-5:
                          // not needed if client registers carry payload — see 3.4]
    TrState  state;       // PENDING_REPLY | ANSWERED | DEAD
}
```

- Single-use: consumed at reply LP; second use → `-EINVAL` (Hostboot `-EBADF`
  analog). Wrong-server use → `-EACCES` **[UNRESOLVED-6: -EACCES vs -EINVAL]**.
- **Evaluation verdict:** explicit single-use token = keep; it is the
  non-forgeable replacement of Hostboot's `msg_t*`-keyed `responses` list.

## 2. Message payload and syscall-register ABI candidate

### 2.1 Payload fields (exact)

Send/call request (a0–a5 at ecall, standard RISC-V convention used by the
existing task syscall layer):

```text
a7 = syscall number (IPC_SEND / IPC_CALL / IPC_RECV / IPC_TRY_RECV / IPC_REPLY / IPC_ENDPOINT_CREATE / IPC_ENDPOINT_DESTROY)
a0 = endpoint handle (u64 typed handle)
a1 = message word 0  (service-defined command/opcode)
a2 = message word 1  (service-defined arg)
a3 = message word 2  (service-defined arg)
a4 = message word 3  (service-defined arg)
a5 = flags/reserved (0 for M00-08.03)
```

So the in-flight message is **4×64-bit words + sender TaskId + transaction id**,
all kernel-side. Recv returns:

```text
a0 = status (0 ok / negative errno)
a1 = message word 0   a2 = word 1   a3 = word 2   a4 = word 3
a5 = sender TaskId
-- for call-originated deliveries the receiver additionally gets the
   ReplyToken in a dedicated return register (see 2.2)
```

Rationale: mirrors `msg_t::data[2]` compressed to the register budget that the
existing `task_syscall` layer already moves (6 args in, 1-2 out today). Four
words is enough for opcode+3 args service calls **[UNRESOLVED-7: 4 words vs
6 — cost of carrying two more through TrapFrame vs utility]**.

### 2.2 Syscall numbers (candidate, extends `task_syscall_abi.h` numbering)

```text
#define JIXIA_TASK_SYSCALL_ENDPOINT_CREATE  6
#define JIXIA_TASK_SYSCALL_ENDPOINT_DESTROY 7
#define JIXIA_TASK_SYSCALL_SEND             8
#define JIXIA_TASK_SYSCALL_CALL             9
#define JIXIA_TASK_SYSCALL_RECV            10
#define JIXIA_TASK_SYSCALL_TRY_RECV        11
#define JIXIA_TASK_SYSCALL_REPLY           12
```

Reply ABI: `a0 = ReplyToken, a1..a4 = reply words`; returns 0/-errno.
Call ABI: `a0 = endpoint, a1..a4 = request`; returns `a0 = status,
a1..a4 = reply words` after resume.

### 2.3 Task states

Reuse existing enum values (already Hostboot-aligned):

- `blocked_message 'M'` — blocked in `recv` (state_info = endpoint) or in
  `call` awaiting reply (state_info = transaction).
- No new states required **[UNRESOLVED-8: whether call-wait deserves a distinct
  state value for debuggability vs reusing 'M' with state_info discrimination]**.

## 3. Primitive semantics

### 3.1 `send(ep, w0..w3)` — asynchronous fire-and-forget

- Never blocks the sender (unless queue-full policy says block —
  **[UNRESOLVED-2]**; current proposal: **-EAGAIN, never block**, keeping the
  first increment free of sender-blocking states).
- LP: successful slot enqueue or waiter pop, under `ep.lock`.
- Errors: `-EINVAL` (bad handle/dead endpoint), `-EAGAIN` (queue full),
  `-EACCES` **[UNRESOLVED-9: whether send is restricted in M00-08.03]**.

### 3.2 `recv(ep)` — blocking receive

- FIFO pop if available; else block: insert into `ep.waiting`,
  `state=blocked_message`, `state_info=ep`, `set_next_runnable()` — all inside
  one `ep.lock` section (transition T2 of the concurrency model).
- Deliver: payload words + sender id to return registers; if the message
  carries a live transaction, also hand out its ReplyToken and record this task
  as the owing server (T4).
- Errors after wake: none in M00-08.03 (endpoint destroyed while blocked →
  wake with `-EDESTROYED`; see 3.7).

### 3.3 `try_recv(ep)` — non-blocking receive

- Identical pop path; empty → `-EWOULDBLOCK` immediately; never blocks, safe
  from poll loops.

### 3.4 `call(ep, w0..w3)` — synchronous call

- Enqueue request + open Transact + block caller (T1+T2 composite) with the
  reply words returned into the caller's saved return registers upon resume.
- The blocked client's return register file lives in its `TaskContext`; the
  reply LP (T5) writes the reply words into the client's saved registers under
  `ep.lock` **before** `add_task`, so resume can never observe a half-written
  reply (this resolves UNRESOLVED-5 in favor of not retaining the request in
  the Transact record).
- Errors: `-EINVAL` bad handle; `-EDEADLK` if calling self-owned endpoint
  **[UNRESOLVED-10: single-task deadlock detection scope]**.

### 3.5 `reply(token, w0..w3)` — single-use response

- Validate token under `ep.lock`: serial must match an outstanding Transact in
  `PENDING_REPLY` whose server is the caller.
- LP: Transact → ANSWERED (dead), reply words written into client context,
  client `add_task` (T5). Wake = requeue only, no direct handoff.
- Errors: `-EINVAL` unknown/consumed token (duplicate reply), `-EACCES`
  wrong-server token, `-EDESTROYED` endpoint died mid-transaction **[order of
  these checks is itself normative: aliveness → token → ownership]**.

### 3.6 Ownership summary

| Asset | Owner sequence |
|---|---|
| MessageSlot | free-pool → sender (during syscall) → endpoint → receiver (at recv LP) → free-pool |
| ReplyToken value | kernel-issued to server at recv; voided at reply LP or destroy LP |
| Client reply registers | client → (frozen while blocked) → written once at reply LP |
| Endpoint | creating task; kernel reclaims at destroy LP or task-exit cleanup |
| Transact record | endpoint pool; live from call LP to reply/destroy/exit cleanup LP |

### 3.7 Endpoint destruction semantics

`destroy(ep)` LP: `state → DEAD` + generation++ (stale handles fail-closed).
Then, still atomic with the flip: pending slots returned to pool; every
`waiting` receiver woken with `-EDESTROYED`; every outstanding Transact marked
DEAD and its blocked client woken with `-EDESTROYED` reply-zero words.

- **[UNRESOLVED-11: refuse-if-busy (`-EBUSY`) vs revoke-and-wake.** Current
  proposal: revoke-and-wake, because M00-08.03 has no drain protocol and
  Hostboot's blind delete is rejected.]
- Token held by a server for a destroyed endpoint: first use returns
  `-EDESTROYED`; the server must drop it. (Hostboot analog: response lookup
  fails → `-EBADF`; same observable behavior class.)

### 3.8 Task-exit semantics

- A task that exits normally is never blocked (blocked tasks cannot run
  `task_end`), so the normal path only needs: endpoints owned by the exiting
  task are destroyed (3.7) and MessageSlots/transacts it owns are reclaimed.
- Crash/kill cleanup of a *blocked* task (T8): remove from `ep.waiting`, void
  its outstanding server tokens (their clients wake with `-ECONNRESET`-class
  error **[UNRESOLVED-12: error code naming]**), free its Transact records.
  **Scope decision: M00-08.03 implements the data-structure hooks and
  acceptance tests forbid killing a blocked IPC participant; full crash-path
  acceptance is deferred.**

### 3.9 Lock ordering and linearization points (normative table)

Lock order: `ep.lock` → run-queue lock → TaskManager lock → TimeManager lock
(from the concurrency model §3; never two endpoint locks; no allocation inside
any critical section — all pools static).

| Operation | Steps (all kernel-side) | LP |
|---|---|---|
| endpoint_create | allocate table slot, init, generation=1 | slot publish (table lock or single-hart boot) |
| send | validate handle → lock → waiter? pop+deliver+wake (T3) : enqueue (T1) → unlock | enqueue or pop |
| try_recv | validate → lock → pop (T1-consumer) → unlock | successful pop / EWOULDBLOCK decision |
| recv | validate → lock → pop? deliver : block (T2) → unlock (+ set_next_runnable if blocked) | pop or block-publish |
| call | validate → lock → enqueue + open Transact + block (T1+T2) → unlock → set_next_runnable | the composite block-publish |
| reply | validate → lock → token check (T6/T5) → write client regs → add_task → unlock | token consumption |
| destroy | validate owner → lock → DEAD flip + purge + wake-all (T7) → unlock | DEAD flip |
| task-exit cleanup | for each owned ep: destroy; for blocked-self case: T8 | removal from waiting |

Wake-after-unlock vs wake-inside-lock: `add_task` is called while `ep.lock` is
held (order 1→2 respected, Hostboot-equivalent). This keeps {token consume,
reply write, wake} one atomic group, eliminating the reply-consumed-but-
client-not-woken window. **[UNRESOLVED-13: wake-inside-lock adds ep.lock
  latency to scheduler ops on other harts; acceptable at 16 endpoints, revisit
  at scale.]**

## 4. SMP posture (designed-for but not tested in this milestone)

- All LPs are lock-serialized; no lock-free IPC structures in M00-08.03
  (JIXIA_CONCURRENCY_CORRECTNESS_RULES §1: prefer less sharing — one lock per
  endpoint at 16 endpoints is the low-sharing point).
- No IPI: a woken remote-hart task runs at that hart's next scheduler entry.
  Safety unaffected; liveness bounded by timer slice. Acceptance on --smp 1;
  --smp 2 smoke behavior documented as eventual.
- Memory ordering: `Spinlock` acquire/release + the `ready`-publish-inside-
  run-queue-lock invariant cover publication; no relaxed loads on IPC state.

## 5. What deliberately stays OUT of generic IPC code

- service name registry (VFS-class; later Root Component Registry milestone)
- payload typing/validation (services define word semantics)
- timeouts/cancellation (layered later via delay queue integration)
- buffer/grant transfer (M00-08.04+ user-copy milestone)
- kernel-as-client MessageHandler pattern (future VMM RP milestone; documented
  in the Hostboot study as the design shape to reuse)
- interrupt delivery endpoints (MSG_INTR_* analog) — requires IPI semantics

## 6. Consolidated UNRESOLVED list

| # | Question | Current lean |
|---|---|---|
| 1 | Endpoint single-owner vs any-task recv | any-task recv (owner check only for destroy) |
| 2 | pending depth + full policy | depth 16; send → -EAGAIN (no sender blocking) |
| 3 | global handles vs per-task capability table | global for M00-08.03; capability table later |
| 4 | ReplyToken scope: per-endpoint serial vs global serial | per-endpoint 32-bit serial + endpoint index |
| 5 | retain request in Transact vs write reply to client context | write reply to client context at reply LP |
| 6 | wrong-server reply error | -EACCES (distinct from -EINVAL) |
| 7 | payload width 4 vs 6 words | 4 words now; revisit with TrapFrame cost data |
| 8 | distinct state for call-wait | reuse 'M' + state_info discrimination |
| 9 | send authorization | unrestricted in M00-08.03 |
| 10 | self-call deadlock detection | detect trivial self-call only (-EDEADLK) |
| 11 | destroy semantics | revoke-and-wake with -EDESTROYED |
| 12 | crash-reset error naming | -ECONNRESET-class code, exact value TBD |
| 13 | wake-inside-ep.lock latency trade | keep (atomicity wins at this scale) |

## 7. Acceptance hook
Every invariant in §3 must map to a machine-checkable case in
`JIXIA_M00_08_03_IPC_ACCEPTANCE_PLAN.md`; any invariant without a test is
deferred by decision, not omission.

---

## 8. Roles and ownership Q&A (second-pass addendum, required by the task list)

Direct answers to the mandated questions; references are to §4/§5 definitions.

**Who is the client?**
Any task that invokes `send` or `call` on an EndpointHandle it possesses
(§4.2 possession rule). "Client" is a *role at a call site*, not a permanent
identity: the kernel identifies the caller as `current hart -> current task`
inside the syscall path (same authority basis as the M00-08.02 task syscalls).
The Hostboot precedent is broader — the kernel itself can be a client via
`MessageHandler` (study §2.11); for M00-08.03 the kernel stays out of the
peer role (deferred, gap doc item GS-11).

**Who is the server?**
Whoever's `recv`/`try_recv` consumes a message from the endpoint. The server
role is acquired *by receiving*, not by creation: endpoint creation grants
only a possession handle, not the receiver slot. There is no stored
"server TaskId" in M00-08.03 (§4.1 deliberately has none); multi-receiver
endpoints are allowed by the design and are server-side policy (UNRESOLVED-2).

**What role does the kernel play?**
Sole owner and sole mutator of all IPC kernel state: endpoint table,
generations, Transact pool, token space, both FIFOs, and all transitions
T1–T10 (§5). It never interprets payloads, never routes by content, never
queues on behalf of a task without that task's syscall. Policy (access
control, naming, service discovery) stays outside the kernel (§5 policy
note). In Hostboot terms: Jixia takes the *mechanism* half of MessageQueue
and leaves the *kernel-as-peer* and *blocking-fault* halves out (gap doc).

**What exactly is owned by the endpoint?**
Exactly the §4.1 fields: `{state, lock, generation, pending slots, waiting
FIFO, outstanding Transacts}` — i.e. the *capability to exchange messages
here*, plus every message parked in `pending` and every transaction
`outstanding`. Crucially the endpoint does **not** own: tasks (only references
for waking), payloads after delivery (copied into receiver registers), or any
thread of control (no Hostboot-style async worker threads — study §2.12,
Jixia implication 6).

**What exactly is owned by the sender?**
Before the send LP: the register payload being assembled (its own registers).
At the LP: ownership of that payload value transfers to the endpoint (async)
or to the transaction (call). After the LP the sender owns nothing of the
message — async send is fire-and-forget by design; a sync caller owns only
its own blocked continuation and the *expectation* materialized as the
Transact (which the kernel, not the client, stores). No sender-side buffer,
no sender-side handle to the in-flight message exists (no cancellation in
M00-08.03 — UNRESOLVED-13).

**What exactly is owned by the receiver?**
After a successful `recv` LP: the delivered message value (in its registers —
ownership is by copy, so there is no shared buffer to arbitrate) and, for a
call transaction, the **reply right** (the ReplyToken). The receiver does not
own the endpoint's queues it consumed from, and it does not own the client's
fate: if it never replies, the client stays blocked until destroy/exit paths
intervene — a deliberate simplification with an explicit watchdog question
deferred (UNRESOLVED-13).

**What represents the right to reply?**
Possession of a *valid* ReplyToken: `Token{ep_index, ep_gen, serial, gen}`
where validity additionally requires (a) endpoint still ACTIVE, (b)
`serial ≤ high_water`, (c) the Transact at `serial` exists and is
PENDING_REPLY, (d) `caller TaskId == Transact.server` (bound at receive LP —
§4.3). It is a single-use, non-transferable (M00-08.03), kernel-validated
capability — the moral equivalent of Hostboot's `MessagePending*` returned by
`msg_wait`, minus the raw-kernel-pointer problem (study §2.11 implication 4).

**When may that reply right disappear?**
Exactly four ways, each with its LP:

| # | Cause | LP | Server observes |
|---|---|---|---|
| 1 | consumed by successful `reply` | token consume (T5) | 0 (registers written to client) |
| 2 | endpoint `destroy` | DEAD flip (T7) | -EINVAL on reply (token voided) |
| 3 | server task exit while holding it | exit cleanup (T8) | n/a (server is gone) |
| 4 | client task end while blocked in `call` | client cleanup removes Transact (T8, concurrency model §2.14) | -EINVAL on reply (dead-letter drop, client already woken/ended) |

In all four cases the disappearance is linearized under `ep.lock` and
published by its release; a racing `reply` re-validates inside the lock and
fails closed. The right can never disappear because the server merely called
other syscalls, and can never survive its endpoint's destruction.

**Second-pass note on the Hostboot contrast (FACT → IMPLICATION):** in
Hostboot the reply right is the `MessagePending*` pointer itself — valid
exactly as long as the record lives in `responses`, and enforceable only by
subsystem lock discipline plus the object-manager shutdown purge (study §2.3,
§2.7). Jixia's typed serial+generation token keeps the same lifetime rule
(*right lives with the transaction record, not with the server*) while making
every stale use fail closed with an errno instead of corrupting memory.
