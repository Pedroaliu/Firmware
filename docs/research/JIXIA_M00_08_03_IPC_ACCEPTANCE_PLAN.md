# Jixia M00-08.03 IPC Acceptance / Litmus-Test Plan (pre-implementation)

- **Date:** 2026-08-19
- **Branch:** `agent/m00-08-03-research`
- **Design source:** `JIXIA_M00_08_03_IPC_ARCHITECTURE_CANDIDATE.md`
  (candidate; UNRESOLVED items resolve before these tests are frozen).
- **Rule:** every architecture invariant has a machine-checkable case below.
  A case failing → the candidate is wrong, not the test.

## 0. Test harness contract (follows existing milestone practice)

- QEMU RV64, `--smp 1` for correctness acceptance; `--smp 2` smoke run for the
  documented eventual-visibility behavior (no safety assertions beyond it).
- U-mode probe tasks emit kernel-log markers of the form
  `IPCxx-STEP-<name>` and `IPCxx-PASS-<name>`; the wrapper script greps the
  ordered marker sequence (same pattern as `test-m00-08-01/02 scripts`).
- Determinism: each script run ×3 must produce identical marker transcripts.
- Two probe tasks A (client) and B (server) are created by the M00-08.01-style
  bootstrap; markers interleave, so ordering assertions are expressed as
  per-task sequences plus explicit cross-task happened-before markers emitted
  by the kernel (e.g. `IPC03-HB-A-BLOCKED` printed at the block LP).

## 1. Invariant → test matrix

| # | Invariant (from candidate doc) | Case id |
|---|---|---|
| I1 | send before recv persists; recv before send blocks then wakes | C01, C02 |
| I2 | FIFO order within an endpoint | C03 |
| I3 | multiple clients serialize; each reply reaches its own caller | C04 |
| I4 | blocking receiver wakeup (exactly one receiver per message) | C05 |
| I5 | synchronous call/reply round trip with payload integrity | C06 |
| I6 | client resumed only after its own reply (not before, not by other's) | C07 |
| I7 | wrong reply token rejected (-EACCES/-EINVAL), no client woken | C08 |
| I8 | duplicate reply rejected (-EINVAL), client state untouched | C09 |
| I9 | message ownership transfer (slot lifecycle, no leak) | C10 |
| I10 | task exit while blocked: cleanup hooks run; forbidden in acceptance | C11 |
| I11 | endpoint destruction wakes/voids everything exactly once | C12 |
| I12 | timer preemption during IPC paths is benign | C13 |
| I13 | lost-wakeup impossible (disjunct invariant) | C02, C05, C13 |
| I14 | typed handles fail closed (stale/dead/garbage) | C14 |
| I15 | try_recv never blocks | C15 |
| I16 | queue-full policy (-EAGAIN) and recovery | C16 |
| I17 | client teardown while blocked in call; late reply is a dead letter | C17 |
| I18 | server exit voids reply rights; blocked client reset exactly once | C18 |
| I19 | two-hart exactly-once delivery; wake liveness bounded by slice | C19, C13 |

## 2. Case specifications

### C01 send-before-recv
- **Initial:** endpoint EP active; B not yet receiving.
- **A:** `send(EP, w)` → returns 0. Marker `IPC01-A-SENT`.
- **B:** later `recv(EP)` → immediate return with w, sender=A. Marker
  `IPC01-B-GOT-<w>`.
- **Expected transition:** T1 then T1-consumer; no block ever.
- **Required:** both markers, in A-before-B order.
- **Forbidden:** B blocking; B receiving wrong word/sender; EWOULDBLOCK.

### C02 recv-before-send
- **Initial:** EP active; B enters recv first.
- **B:** `recv(EP)` blocks. Kernel emits `IPC02-B-BLOCKED` at T2 LP.
- **A:** `send(EP, w)` → 0; kernel emits `IPC02-A-WOKE-B` at T3 LP.
- **B:** resumes with w.
- **Expected:** T2 then T3 under ep.lock; wake exactly once.
- **Required:** BLOCKED marker strictly before WOKE marker; B's GOT marker last.
- **Forbidden:** B consuming twice; send returning error; no wake (lost wakeup).

### C03 FIFO ordering
- **Initial:** EP empty.
- **A:** send w=1,2,3 sequentially (markers SENT-1..3).
- **B:** recv ×3.
- **Required:** B-GOT-1, B-GOT-2, B-GOT-3 in order.
- **Forbidden:** any permutation; duplicate; loss.

### C04 multiple clients
- **Initial:** EP; server B ready.
- **A1:** call(EP, x1) blocks; **A2:** call(EP, x2) blocks (order markers).
- **B:** recv → token t1(x1) → reply(t1, r1); recv → t2(x2) → reply(t2, r2).
- **Required:** A1 resumes with r1 ∧ exactly after its reply; A2 with r2.
- **Forbidden:** crossed replies (A1 sees r2); either client waking before its
  own reply LP; a token valid for the wrong transaction.

### C05 blocking receiver wakeup (one message, one of two waiters)
- **Initial:** B1 and B2 both blocked in recv on EP (kernel marks both).
- **A:** single send(EP, w).
- **Required:** exactly one `WOKE` marker (FIFO head = B1); B1 returns w;
  B2 remains blocked (kernel asserts B2 state still blocked_message at end).
- **Forbidden:** both woken; neither woken; wrong waiter woken (B2).

### C06 synchronous call/reply round trip
- **A:** call(EP, req) blocks; **B:** recv gets req + token; B replies rsp.
- **Required:** payload words identical end-to-end; A's resume marker after
  B's reply marker; A's return status 0.
- **Forbidden:** A running between call and reply (kernel asserts no A READY/
  RUNNING interval between block LP and reply LP — emits `IPC06-A-NEVER-RAN`).

### C07 client resumed only after reply (negative control)
- Like C06 but B **delays** (sleep via existing task sleep) before replying.
- **Required:** A blocked for ≥ the sleep window (kernel timestamp markers);
  resume strictly after reply marker.
- **Forbidden:** early resume; resume on the *other* client's reply (use two
  clients with staggered replies).

### C08 wrong reply token
- **B1, B2** each hold tokens t1, t2 from two calls; **B2** replies with t1.
- **Expected:** -EACCES (UNRESOLVED-6 lean); A1 not woken; t1 still valid for
  B1, which then replies successfully.
- **Required:** error marker with code; A1 wakes only after B1's correct reply.
- **Forbidden:** t1 consumed by the wrong server; A1 double-wake.

### C09 duplicate reply
- **B:** reply(t) succeeds; then reply(t) again.
- **Expected:** second → -EINVAL; A's registers unchanged after second attempt
  (A verifies its reply words once).
- **Forbidden:** second reply waking A again or mutating A's reply registers.

### C10 message ownership transfer
- Kernel-side slot accounting marker at each LP: pool→ep, ep→B.
- **A:** N=K MaxDepth sends; **B:** N recvs; then kernel prints
  `IPC10-POOL-FULL-FREE` when the free-pool count returns to max.
- **Required:** exact pool-count equality at end.
- **Forbidden:** leaked slots (count > 0 used); double-freed slots (kernel
  asserts slot state machine legality at every transition).

### C11 task exit while blocked (scope: forbidden-path guard)
- **Setup:** A blocked in call on B-owned transaction; acceptance task E
  attempts `task_end` semantics that would reap a blocked task → kernel
  returns the documented refusal / logs the cleanup hook invocation.
- **Required:** kernel emits `IPC11-CLEANUP-HOOK-RAN` for the *data-structure*
  hooks; client A woken with the crash-reset class error (UNRESOLVED-12) or
  the operation refused, exactly per the frozen decision.
- **Forbidden:** silent leak of Transact/waiting entry; kernel crash.

### C12 endpoint destruction
- **Initial:** EP with 2 pending messages, 1 blocked receiver R, 1 outstanding
  call client A.
- **Owner:** destroy(EP).
- **Expected:** -EDESTROYED delivered to R and A (single wake each); pending
  slots reclaimed (pool marker); subsequent send/recv/reply on EP → -EINVAL
  (generation bump makes the stale handle fail closed).
  **[OPEN divergence: the reply-case errno here (`-EINVAL`) vs candidate
  §3.5/§3.7 `-EDESTROYED` under the aliveness-first check order is
  unresolved — decision pending with UNRESOLVED-6/11.]**
- **Forbidden:** any participant left blocked forever; double wake; stale
  handle aliasing a recycled endpoint.

### C13a preemption while receiver is blocked
- **B:** blocks in `recv` (C02-style); preemption timer fires repeatedly while
  B is blocked (kernel emits `IPC13a-SLICE` per expiry); A sends after k>1
  slices have elapsed.
- **Required:** B wakes with the message; exactly one BLOCKED and one WOKE
  happened-before marker; wake observed within one slice of the send.
- **Forbidden:** lost wake; double wake; B resuming with stale registers after
  the extra slices; block-state corruption by the timer path.

### C13b preemption during message traffic
- **A** and **B** run under the M00-08.02 preemption load (short slices) while
  executing the C02/C05/C06 flows repeatedly (R=50 iterations).
- **Required:** identical PASS transcript every iteration; kernel emits
  `IPC13-NO-LOSTWAKE` summary (B always woke within one slice).
- **Forbidden:** any lost wakeup, duplicate delivery, or marker reordering
  within an iteration.

### C14 handle validation
- send/recv/destroy with: garbage handle, out-of-range index, stale generation
  (post-destroy), handle of an ACTIVE endpoint but wrong operation class (none
  in M00-08.03 — reserved for capability era).
- **Required:** -EINVAL in all three cases; no kernel state change (pool count
  unchanged).
- **Forbidden:** kernel fault; aliasing.

### C15 try_recv never blocks
- **B:** try_recv on empty EP → -EWOULDBLOCK immediately (marker before next
  instruction marker). Repeat interleaved with A's sends; every try_recv
  returns either a message or EWOULDBLOCK, never blocks.

### C16 queue-full policy
- **A:** send until -EAGAIN (count = depth); marker `IPC16-FULL`.
- **B:** recv one; **A:** send succeeds again (space freed).
- **Forbidden:** overwrite of oldest; send blocking (timer-bounded marker
  proves the syscall returned).

### C17 client exits before reply
- **Initial:** A blocked in `call` (Transact PENDING_REPLY, token held by B);
  B deliberately delays.
- **Actor A teardown trigger:** A cannot invoke `task_end` while blocked (the
  M00-08.02 refusal), so the case drives the kernel-initiated teardown path
  (crash simulation) exactly as frozen by UNRESOLVED-12.
- **Operations:** trigger A teardown; then **B:** `reply(token)`.
- **Expected transition:** cleanup under ep.lock removes the Transact (T8);
  A is not left blocked; B's reply fails closed → dead-letter drop.
- **Required markers:** `IPC17-CLEANUP-RAN`, then `IPC17-REPLY-REJECTED` with
  -EINVAL; Transact/slot pool counts exact at end.
- **Forbidden:** kernel fault; B blocked; leaked Transact; reply "succeeding"
  after the client is gone; double processing of A's teardown.

### C18 server exits before reply
- **Initial:** B consumed the message and holds the reply right; A blocked in
  `call`.
- **Actor B:** finishes processing, invokes `task_end` normally *before*
  replying.
- **Expected transition:** exit cleanup voids all tokens with server=B (T8);
  A woken exactly once with the crash-reset class error; pool restored.
- **Required markers:** `IPC18-TOKENS-VOIDED`, `IPC18-CLIENT-RESET`;
  `IPC18-POOL-EXACT`.
- **Forbidden:** A blocked forever; the token usable after B's exit; double
  wake of A; kernel fault during exit with outstanding transactions.

### C19 future two-hart send/receive litmus (`--smp 2`)
- **Initial:** smp2; EP active; B enters `recv` and blocks (whichever hart it
  ran on becomes the "remote-observer" hart); A runs later.
- **Operations:** B `recv` (blocks, HB-BLOCK marker) → A `send` → observe B's
  GOT marker; R=20 repetitions.
- **Expected:** exactly-once delivery every repetition; the wake is observed
  within K scheduler slices (K counted by `IPC19-SLICE` markers, harness-set,
  generous); no immediacy assertion.
- **Required markers:** BLOCK < WOKE < GOT per repetition; `IPC19-PASS-SUMMARY`.
- **Forbidden:** double delivery; lost wake; any cross-hart *immediacy*
  assertion (an immediacy claim would be a wrong test, not a kernel failure).
- **Status:** informational smoke for M00-08.03 (single-hart gate); becomes a
  hard gate before M00-08.05 SMP work.

## 3. Happened-before observability requirements (kernel-side)

The kernel must emit, at the LP itself (inside the ep.lock section, before the
possible add_task): `IPC-HB-<event> <ep> <task>` for events BLOCK, WAKE, TOKEN-
ISSUE, TOKEN-CONSUME, DESTROY. These are the cross-task ordering oracle the
  scripts assert on; they are debug-only (compiled out in default builds via
  the existing probe-gating mechanism used by M00-08.01/02).

## 4. Exit criteria

- All C01–C18 green on `--smp 1`, ×3 deterministic (C13 = C13a + C13b).
- C19 executed on `--smp 2` as the two-hart litmus: pass summary + bounded
  liveness required; **informational for M00-08.03**, hard gate before
  M00-08.05 SMP work.
- Prior milestone chain (M00-02..M00-08.02) still green.
- Pool/leak markers: exact counts.
- Any invariant failing → candidate revision recorded before implementation
  continues (no test weakening to pass).
