# Jixia IPC Concurrency / Ownership Model — M00-08.03 Pre-Implementation Audit

- **Date:** 2026-08-19
- **Branch:** `agent/m00-08-03-research`
- **Inputs:** `docs/research/JIXIA_HOSTBOOT_IPC_STUDY_2026-08-19.md`, current
  `microkernel/core/scheduler.*`, `task.*`, `task_manager.*`, `time_manager.*`,
  `spinlock.h`, M00-08.02 semantics (mtime preemption, per-hart delay queues,
  global+local run queues, no IPI), and
  `docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md` (Ownership / Ordering /
  Visibility review framework).
- **Status:** RESEARCH. No kernel code is implemented or decided here.

## 0. Ground model assumed for the audit

The audit is written against the *candidate* M00-08.03 mechanism shape (details
in the architecture-candidate document), because hazards only exist relative to
a mechanism:

```text
Endpoint (kernel object)  ::= { lock, pending message FIFO (kernel-owned copies),
                                 waiting receiver FIFO (Tasks),
                                 outstanding reply tokens }
ReplyToken                ::= single-use kernel identity of one call transaction
send(ep, payload)         ::= enqueue or direct handoff to waiting receiver
call(ep, payload)         ::= send + client BLOCKED_MSG with pending token
recv(ep)/try_recv(ep)     ::= dequeue or (recv only) block in waiting FIFO
reply(token, payload)     ::= consume token, wake/complete client
```

Current Jixia facts this model must respect:

1. `RunQueue::insert` publishes `state = ready` **inside** the queue lock and
   is fail-closed (M00-08.02 hardening). Wake paths must use `add_task`.
2. `set_next_runnable()` is the deschedule primitive (also used from timer).
3. Timer preemption can fire between any two kernel instructions in a syscall
   path **on the same hart**; on other harts nothing observes local progress
   except via shared locks (no IPI in this milestone).
4. `Task.state_info` exists and currently names the object a blocked task waits
   on (mirrors Hostboot).
5. Single-hart acceptance first; the model must still be SMP-race-free.

## 1. Review framework applied

Every operation below is audited with three questions from
`JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`:

- **Ownership** — who uniquely owns each object/field at each step; every
  transfer is an explicit, atomic event.
- **Ordering** — the global order in which atomic events linearize; each
  operation names its linearization point.
- **Visibility** — what other harts/observers can see and when; publication is
  a release-store/lock-unlock boundary, and no observer may act on unpublished
  state.

## 2. Interleaving scenarios

Legend for timelines: `H0`/`H1` are harts; `A` client task; `B` server task;
`LP` = linearization point; `…` arbitrary delay.

### 2.1 Send before recv (message waits in queue)

```text
H0: A: send(ep,msg) ──LP: enqueue under ep.lock──> A keeps running
H1:                          …                       B not yet recv
H1: B: recv(ep) ──LP: dequeue under ep.lock──> B gets msg immediately
```
- Ownership: msg copy owned by `ep.pending` between the two LPs; by B after.
- Ordering: enqueue LP strictly precedes dequeue LP (same lock).
- Visibility: enqueue is published at `ep.lock` release; recv cannot miss it
  because it re-checks the FIFO under the same lock.
- **Atomic transition required:** {FIFO-append + sender-return} must be one
  critical section; splitting them lets a receiver see “empty” forever after a
  successful send.

### 2.2 Recv before send (receiver waits in queue)

```text
H0: B: recv(ep) ──LP: {FIFO empty ∧ insert B into ep.waiting ∧ B.state=BLOCKED_MSG}──> B deschedules
H1:        …
H1: A: send(ep,msg) ──LP: {pop B ∧ write payload to B's return slot ∧ add_task(B)}──> A keeps running
```
- Ownership: B's wake is owned by the sender that pops it; exactly one sender
  can (queue pop is atomic).
- Ordering: B's block LP precedes A's wake LP; both under `ep.lock`.
- Visibility: B's BLOCKED state is published before `ep.lock` release, so a
  concurrent sender sees either non-empty FIFO or B in `waiting` — never both
  empty. This disjunct is the **core lost-wakeup invariant**.
- **Atomic transitions required:** (a) emptiness-check + waiter-insert +
  state-publish + deschedule-request as one section; (b) waiter-pop +
  payload-write + add_task as one section.

### 2.3 Multiple senders

```text
H0: A1: send(ep,m1) ─LP→ enqueue m1
H1: A2: send(ep,m2) ─LP→ enqueue m2 (may spin on ep.lock while A1 holds it)
H0/H1: B: recv() returns m1 then m2 (FIFO order per LP order)
```
- Ordering: total order of sends = lock acquisition order; FIFO preserves it.
- No sender state lives in the endpoint beyond its message → no sender
  cancellation needed. Fairness of `Spinlock` (TTAS with backoff note in
  JIXIA_CONCURRENCY_CORRECTNESS_RULES) is acceptable; starvation-freedom not
  claimed (documented limitation).

### 2.4 Multiple waiting receivers

```text
H0: B1: recv(ep) ─LP→ waiting=[B1]
H1: B2: recv(ep) ─LP→ waiting=[B1,B2]
H0: A: send(ep,m) ─LP→ pop B1 (FIFO), deliver to B1; B2 stays blocked
```
- Receiver wake order = FIFO of `waiting`; each message wakes exactly one
  receiver (atomic pop).
- Forbidden state: two receivers handed the same message (impossible if the
  pop+deliver is one critical section).
- Jixia note: Hostboot has the same shape; no kernel receiver pool needed.

### 2.5 sendrecv → recv → respond (normal call)

```text
H0: A: call(ep,m) ─LP→ {enqueue m+token; A.state=BLOCKED_MSG; A in ep.waiting-clients; set_next_runnable}
H1: B: recv(ep) ─LP→ pop m; token recorded outstanding on ep
H1: B computes
H1: B: reply(tok,m') ─LP→ {consume token; write reply to A; add_task(A)}
H0: A resumes with m'
```
- Ownership chain of the *reply slot*: A owns it (empty) → token owner B owns
  it after recv → A owns it (filled) after reply LP.
- **Atomic transitions required:** token consumption + reply publication +
  client wake in one critical section; token insert at recv LP must be atomic
  with message pop.

### 2.6 Client blocks before server runs (direct-handoff absent)

Hostboot switches directly to the server inside `MsgSendRecv`. Jixia candidate:
requeue-based wake only.

```text
H0: A: call(ep,m) blocks; scheduler picks idle/other
H0: (timer) B (already READY) dispatched later
H0: B: recv(ep) gets m …
```
- Hazard if Jixia *did* direct handoff: `set_current_task` from inside a
  syscall bypasses the run-queue invariants unless it re-publishes the current
  task through the same contract as `add_task`. Decision for M00-08.03: no
  direct handoff; wake = add_task. Cost: one scheduling latency; benefit: one
  wake protocol.

### 2.7 Server responds while another hart schedules the client

```text
H1: B: reply(tok) ─LP→ A.state=ready published under run-queue lock (add_task)
H0: A is NOT running: it is BLOCKED; H0 may be in set_next_runnable right now
```
- Race window: H0's scheduler might have already dequeued… impossible: A was
  not READY, so H0 could not have dequeued it; `add_task` fail-closed insert
  guarantees single publication (second `add_task` returns false).
- **Required invariant:** a task is wakeable exactly once per block event;
  `RunQueue::insert` failure on non-queued/blocked task is the guard. Verified
  present in current code (`insert` checks `queued`/state).
- Visibility: add_task's publication (state=ready inside queue lock) precedes
  any hart's ability to dequeue it. Without IPI, H0 sees it at its next
  scheduler boundary — liveness only, not safety.

### 2.8 Task exits while waiting (blocked in recv/call)

Jixia tasks end via `task_end` syscall or crash. Hostboot: impossible to kill a
blocked task; `msg_wait_timeout` comments treat this as a design constraint.

Jixia candidate decision (see architecture doc): task exit while blocked is
**not permitted through the normal path in M00-08.03** — `task_end` from a
blocked task never executes (it is not running). The real hazards:

```text
H0: parent/kernel ends B (crash cleanup) while B ∈ ep.waiting
     ─ must atomically remove B from ep.waiting AND release its token obligations
H1: A: send(ep) concurrently — must not wake the dead B
```
- **Atomic transition required:** blocked-task reclamation takes `ep.lock`,
  removes the task from `waiting`, marks its outstanding tokens dead, wakes or
  fails its clients. Lock order: `ep.lock` before TaskManager lock or vice
  versa — must be fixed globally (see §3).
- M00-08.03 minimal stance: acceptance forbids ending a task that is blocked;
  kernel cleanup path documented as UNRESOLVED-future. (Second-pass update:
  the data-structure cleanup hooks moved into M00-08.03 scope and are
  exercised by crash-simulation teardown — architecture candidate §3.8,
  acceptance C11/C17; full crash-path acceptance stays deferred and the
  crash-reset error naming remains UNRESOLVED-12.)

### 2.9 Endpoint destruction with pending messages

Hostboot `MSGQ_DESTROY` deletes blindly → use-after-free class. Jixia
requirements derived:

```text
H0: destroy(ep) while ep.pending nonempty OR ep.waiting nonempty OR tokens outstanding
```
- Safe options (decided in architecture doc): (a) refuse with -EBUSY until
  drained; (b) revoke: wake all waiters with -EDESTROYED, drop queued
  messages, dead-token all clients. Hostboot-compatible quickness (blind
  delete) is rejected.
- **Atomic transition required:** destruction LP = state flips to DEAD under
  `ep.lock`; every other op's LP must re-check aliveness under the same lock.
  Handle generation numbers make stale U-mode handles fail-closed (-EINVAL)
  instead of aliasing a recycled object.

### 2.10 Duplicate reply

```text
H1: B: reply(tok) ─LP→ token consumed; A woken
H1: B: reply(tok) again ─LP→ token lookup fails → -EINVAL (Hostboot: -EBADF)
```
- Ownership: token is a **single-use capability**; consumption is the LP.
- Second reply must not touch A (A may already have re-blocked on something
  else — writing its return slot twice is a correctness bug).

### 2.11 Wrong reply token

- Token not found on this endpoint → -EINVAL; token found but caller is not
  the task that owns the transaction → -EACCES-class error (or -EINVAL;
  decision in architecture doc). Must never wake the wrong client.
- Requires: token validity check and client wake in one critical section.

### 2.12 Timer preemption during send/recv

```text
H0: A in send(ep): acquired ep.lock? ──timer fires anywhere──
  case a: before lock: A preempted as RUNNING→requeued READY; later resumes; no
           IPC state exists yet; benign.
  case b: inside critical section: kernel syscall path runs with the lock held
           until it releases; M-mode timer trap on same hart must NOT re-enter
           IPC paths (it doesn't: timer only touches delay queues + scheduler).
           Lock is eventually released; other harts spin meanwhile. Benign but
           adds latency; M-mode critical sections must be bounded.
  case c: after LP, before returning to U: A preempted; message already
           linearized; wakeup already scheduled. Benign.
```
- **Required invariant:** trap/timer paths never acquire endpoint locks
  (context contract: `ep.lock` is syscall-context-only). Current Jixia timer
  path satisfies this by construction (it only calls TimeManager/Scheduler).
- Reentrancy: single-hart preemption inside a spinlock'd section requires the
  trap handler to not touch the same lock — confirmed by the path audit above.

### 2.13 Lost-wakeup proof sketch

Claim: with the one-lock discipline, no send can complete while a receiver
believes the queue empty-and-unwaited.

Proof obligation: receiver R's LP is “FIFO empty ⇒ R inserted into waiting AND
state=BLOCKED published, atomically under ep.lock”. Sender S's LP is “pop
waiting OR append FIFO, atomically under ep.lock”. Suppose a message M is sent
after R blocked but R never wakes. Then at S's LP, either waiting contained R
(contradiction: R woken) or FIFO append happened while R not in waiting — but
R's LP precedes S's LP in lock order and R's LP established R ∈ waiting while
FIFO empty; S's LP sees the same FIFO non-empty only if it linearized after
another sender; in all cases the disjunction {FIFO nonempty ∨ R ∈ waiting}
holds at every post-LP state. ∎ (standard argument; matches Hostboot
construction and JIXIA_CONCURRENCY_CORRECTNESS_RULES §2.1 obligations.)

Residual hazard class (documented, not solved here): wake *publication* to a
remote hart's scheduler is eventual (no IPI); a just-woken task on another hart
waits for that hart's next scheduler boundary. Liveness-only; must be stated
in the acceptance plan as expected behavior.

### 2.14 Reply racing client teardown (second-pass case)

```text
H0: A blocked in call; Transact T{client=A, server=B, PENDING_REPLY} on ep
H1: B: reply(T) ────────────┐
H0: kernel reaps A (crash) ─┴─ race for ep.lock
```

- **State before:** T ∈ ep.outstanding (PENDING_REPLY); A `blocked_message`, state_info=T.
- **State transition:** exactly one of {T5 token-consume, T8 client-cleanup-removes-T}; the Transact state machine `PENDING_REPLY → ANSWERED | DEAD` permits a single transition under `ep.lock`.
- **Owner:** first lock holder linearizes; the loser re-reads post-state and fails closed.
- **Lock/atomic requirement:** reply path and teardown path must both re-validate the Transact *inside* `ep.lock` (no TOCTOU between lookup and action).
- **Linearization point:** whichever of token-consume / Transact-removal happens first under `ep.lock`.
- **Visibility requirement:** the state flip is published at `ep.lock` release; the second arrival must not act on a cached pre-state.
- **Forbidden outcome:** reply writing into a freed/reused TaskContext; double wake; reply "succeeding" silently after the client is gone (kernel must observe and drop, not fault).

### 2.15 Endpoint destruction with blocked sender (second-pass case)

In M00-08.03 `send` never blocks (`-EAGAIN` policy), so a "blocked sender" is
exactly a caller blocked in `call` (the analog of Hostboot's waiting sync
client riding in `MessagePending`).

```text
H0: A: call(ep) blocked; T{client=A} outstanding
H1: owner: destroy(ep) ──LP: DEAD flip + purge (T7)──> A woken with -EDESTROYED
                                          token issued to B voided
```

- **State before:** ep ACTIVE; T PENDING_REPLY; A blocked.
- **Transition:** ep.state→DEAD ∧ generation++ ∧ purge {pending slots, waiting FIFO, outstanding Transacts} ∧ wake-each-once — one critical section.
- **Owner:** destroying task owns the teardown; kernel owns wake-once.
- **Lock/atomic:** T7 all-inclusive under `ep.lock`.
- **LP:** DEAD flip.
- **Visibility:** stale handle lookups (old generation) fail closed afterwards.
- **Forbidden:** A blocked forever; A resumed with garbage reply registers; B's token surviving destruction; double wake.

### 2.16 Server exit while owning reply state (second-pass case)

```text
H0: B received msg + token (B is the reply-right holder) ; B ends normally
    B exit cleanup ──LP: void tokens{server=B} (T8)──> A woken with -ECONNRESET-class
```

- **State before:** T PENDING_REPLY, server=B; A blocked; B RUNNING.
- **Transition:** all Transacts with server=B → DEAD; their clients woken with the crash-reset class error (UNRESOLVED-12).
- **Owner:** exit path (TaskManager) initiates; endpoint executes under `ep.lock`.
- **Lock/atomic:** exit cleanup must take `ep.lock` (order: ep → task table per §3) and re-validate.
- **LP:** removal of the Transact from `outstanding`.
- **Visibility:** token invalidation published at `ep.lock` release; any racing `reply` (2.14 machinery) fails closed.
- **Forbidden:** A blocked forever; token usable after server death; kernel fault during exit with outstanding tokens.

### 2.17 Wakeup concurrent with timeout (second-pass case; future mechanism)

M00-08.03 ships no timeout — this case is specified now because adding one
later creates the classic cancel-vs-wake race, and the resolution must not
require redesign.

- **State before:** A in `ep.waiting`; delay-queue deadline armed (future integration).
- **Transition:** timeout expiry and message arrival race; both must linearize on `ep.lock`: **timeout LP = successful removal of A from `waiting`** (→ return `-ETIMEDOUT`); **wake LP = pop of A from `waiting`** (→ deliver). Exactly one can succeed because each is an atomic FIFO removal under the same lock.
- **Owner:** the delay queue owns the timeout attempt; a sender owns the wake attempt; `ep.waiting` membership is the contended asset.
- **Lock/atomic:** removal-from-waiting is the single arbiter; no "check-then-block-then-sleep" sequence may exist outside the lock (Hostboot's `msg_wait_timeout` could not do this and had to make its TOCTOU benign with a helper task — study §2.15; Jixia must do better when timeouts land).
- **LP:** the winning removal.
- **Visibility:** the loser observes empty membership slot and returns without acting.
- **Forbidden:** A both woken and timeout-returned; A neither (lost wakeup by cancellation).

### 2.18 Queue insertion vs task READY publication (second-pass case)

The scheduler-side half of every wake (T3/T5 call `add_task` while holding
`ep.lock`):

- **State before:** target task `blocked_message`, not queued.
- **Transition:** `RunQueue::insert` publishes `state=ready ∧ queued=true` **inside** the run-queue lock (current Jixia fact, M00-08.02 hardening); fail-closed if already queued/ended.
- **Owner:** run queue owns runnability publication; the waker owns the decision.
- **Lock/atomic:** with `ep.lock` held across `add_task`, no second waker can even attempt the same task (it would need `ep.lock`); the rq lock then makes the publication atomic. Two guards, zero windows.
- **LP:** insert's in-lock publication.
- **Visibility:** no hart may dequeue the task before the publication release; `set_next_runnable` on another hart sees it only after the rq lock is dropped.
- **Forbidden:** task observable in a run queue with state ≠ ready (pre-hardening bug class); double enqueue; enqueue-after-end.

### 2.19 Two-hart send/receive litmus (second-pass case)

```text
H0: B: recv(ep) ──LP: block (T2)──> B descheduled, H0 runs other/idle
H1: A: send(ep,w) ──LP: pop B + deliver + add_task(B) (T3)──> A continues
H0: (timer/scheduler boundary) B dispatched with w
```

- **State before:** ep ACTIVE, empty, no waiters; B on H0; A on H1.
- **Transition:** T2 then T3; delivery exactly once, wake exactly once.
- **Owner:** as in 2.1/2.2 — the locks, not the harts, carry correctness.
- **Lock/atomic:** both LPs on `ep.lock`; add_task per 2.18.
- **LP:** each T's own in-lock action.
- **Visibility:** without IPI, H0 observes B's readiness at its next scheduler entry — **liveness bounded by slice; safety immediate** (this is the M00-08.03 SMP posture: race-safe by construction, eventually-visible by policy).
- **Forbidden:** double delivery; lost wake; safety depending on remote hart being interrupted.

### 2.20 Simultaneous send/recv on one endpoint (tie case)

Both a `recv` and a `send` arrive at `ep.lock` "at the same time":

- recv wins the lock: FIFO empty ⇒ receiver blocks (T2); send then finds it in `waiting` and wakes it (T3). Pairing: delivered.
- send wins the lock: message enqueued (T1); recv then pops it immediately. Pairing: delivered.

Either serialization yields *the same observable outcome* (message delivered
exactly once to that receiver) — the defining property of LP-based reasoning:
the interleaving cannot change the result, only who spins first.

## 3. Lock-order and atomic-transition inventory

Global lock order (candidate, single order for all IPC code):

```text
1. endpoint lock (ep.lock)
2. scheduler run-queue lock(s)
3. TaskManager task-table lock
4. TimeManager delay-queue lock
```

Rules: never take a lower-numbered lock while holding a higher-numbered one;
never acquire two endpoint locks in one critical section (no cross-endpoint
atomic ops in M00-08.03); wake paths (add_task) are called *after* the endpoint
LP but MAY be called inside the endpoint critical section only if rule order
1→2 is respected (Hostboot calls addTask inside the queue lock; equivalent
order endpoint→scheduler is consistent with the order above).

Atomic ownership/state transitions (complete list):

| # | Transition | Protected by | LP |
|---|---|---|---|
| T1 | msg append to `pending` | ep.lock | successful enqueue |
| T2 | receiver insert into `waiting` + BLOCKED publish | ep.lock | block event |
| T3 | waiter pop + payload→receiver return slot + add_task | ep.lock(+rq) | wake event |
| T4 | dequeue msg + token issue (call transactions) | ep.lock | recv event |
| T5 | token consume + reply publish + client add_task | ep.lock(+rq) | reply event |
| T6 | token invalidate (duplicate/wrong) | ep.lock | failed lookup |
| T7 | endpoint DEAD flip + waiter purge | ep.lock | destroy event |
| T8 | blocked-task removal on end/crash | ep.lock + task lock | cleanup event |
| T9 | run-queue insert with ready-publish | run-queue lock | schedule publication |
| T10 | delay-queue insert/remove | time lock | sleep/expire |

Every syscall operation is a composition of T1–T10 with exactly one LP; all
other steps are pure computation on kernel-owned data.

## 4. Ownership / Ordering / Visibility summary table

| Object | Owner | Publication | Lifetime | Observers |
|---|---|---|---|---|
| queued message copy | endpoint → receiver | ep.lock release | enqueue→dequeue | recv/reply paths |
| reply slot of blocked client | client → token holder → client | T5 LP | block→resume | reply path only |
| reply token | endpoint (issued) → server (held) → dead | T4 issue / T5,6 consume | recv→reply | reply path |
| waiting-FIFO membership | endpoint | T2/T3 under ep.lock | block→wake | send path |
| endpoint liveness | creator → kernel registry | T7 flip | create→destroy | all ops re-check |
| task runnability | scheduler (T9) | rq lock release | block→ready | all harts' schedulers |

## 5. Hazards NOT accepted into M00-08.03 (explicitly out)

- blind destroy (Hostboot `MSGQ_DESTROY` semantics)
- user-pointer message bodies dereferenced by M-mode
- kernel pointer values as U-mode handles
- reply-token reuse or forgery (no generation/ownership check)
- double wake of one block event (guarded by fail-closed add_task)
- IPI-dependent correctness (correctness must hold with polling visibility)
- direct handoff `set_current_task` from syscall context (deferred decision,
  see architecture doc UNRESOLVED list)
