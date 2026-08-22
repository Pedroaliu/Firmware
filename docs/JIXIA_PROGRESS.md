# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-22
- **Stable integration branch:** `main`
- **Latest completed milestone:** `M00-08.02 Hostboot Scheduler Alignment` — DONE
- **M00-08.02 integration:** `de4df0e` (PR #26); full RV64 QEMU regression CI run `32219284629` SUCCESS
- **M00-08.03 status:** ACTIVE — M00-08.03.01 (non-blocking Endpoint/Message IPC) DONE (`9c617ec`, PR #29; CI `32437093429`); M00-08.03.02 (blocking recv + FIFO wakeup) DONE (squash `ba27c4c1a520`, PR #30; CI `32460452557`); M00-08.03.03 (call/reply + ReplyToken) ABI candidate at Revision 3 — three review-fix rounds applied (2026-08-22), review pending
- **Primary M00-07 acceptance evidence:** GitHub Actions run `32005255564` — full RV64 QEMU regression SUCCESS
- **Architecture research gate:** Hostboot kernel/VFS/InitService/service-startup path — SUFFICIENTLY CLOSED FOR IMPLEMENTATION
- **Current implementation milestone:** `M00-08.03 Message IPC Foundation` — ACTIVE (M00-08.03.01/.03.02 DONE; M00-08.03.03 ABI candidate under review; full task-exit IPC cleanup split to M00-08.03.04)
- **Immediate next step:** third-round review of the M00-08.03.03 call/reply + ReplyToken ABI candidate (`docs/JIXIA_M00_08_03_03_IPC_CALL_REPLY_ABI.md`, Revision 3), then flip it FROZEN FOR IMPLEMENTATION and implement; task-exit IPC cleanup stays M00-08.03.04
- **Architecture checkpoint:** `docs/JIXIA_BOOT_SERVICE_NATIVE_SBI_ARCHITECTURE_2026-08-17.md`

## Status legend

| Status | Meaning |
|---|---|
| `DONE` | implementation/research checkpoint accepted and evidence retained |
| `ACTIVE` | the single current implementation milestone |
| `RESEARCH` | architecture study required before a milestone is frozen |
| `NEXT` | ordered immediately after the current milestone |
| `PLANNED` | accepted roadmap item, not started |
| `FROZEN` | implementation blocked by missing prerequisites |

## Milestone / feature ledger

| Work item | Status | Evidence | Notes |
|---|---|---|---|
| M00-00 Minimal RV64 boot, stack, BSS, UART | DONE | `c30c0405b388a0fba4c528856236ff02267f1a77` | QEMU virt reset/bootstrap |
| M00-01 Minimal fatal M-mode trap | DONE | `ce661a8c1f1798861cab2ef766749cae38bcdc69` | `mtvec`, `mcause`, `mepc`, `mtval` fatal path |
| M00-02 Complete RV64 TrapFrame | DONE | `scripts/test-trap-frame.sh` | complete integer context and common restore |
| M00-03 Recoverable trap and `mret` | DONE | `scripts/test-recoverable-trap.sh` | 32-bit `EBREAK` and 16-bit `C.EBREAK` |
| M00-04 Machine timer interrupt | DONE | `scripts/test-timer-interrupt.sh` | first recoverable asynchronous M interrupt |
| F00-01 Kernel Print foundation | DONE | `scripts/test-kernel-print.sh` | formatter, KernelLogBuffer, temporary UART mirror |
| M00-05 SMP foundation | DONE | `scripts/test-m00-05-population.sh` | HartLocal, private stacks, FDT population, per-hart timers |
| M00-06 Privilege transition foundation | DONE | CI `31664329150` and later regressions | trusted M trap stack, M->S, S->M->S, hostile lower stack proof |
| M00-07 Pre-DDR Memory Foundation | DONE | CI `32005255564`; `docs/JIXIA_M00_07_MEMORY_FOUNDATION.md` | FFS pflash, contained EarlyMemory, Sv39, PNOR paging, mainstore mechanism prototype |
| Hostboot service/InitService startup study | DONE | 2026-08-17 source study; Drive research record | kernel->root VFS->InitService; lower-privilege tasks; provider-backed faults |
| M00-08.01 Hostboot-shaped Task Executive | DONE | `e930a24`; `scripts/test-m00-08-01-task-lifecycle.sh` | U task dispatch, task/tracker lifecycle, ready queues, idle, create/yield/end/wait/detach |
| M00-08.02 Hostboot Scheduler Alignment | DONE | `de4df0e` (PR #26); CI run `32219284629`; `scripts/test-m00-08-02-preemptive-scheduler.sh` | mtime preemption, per-hart delay queue, deadline-aware idle, stack pre-mapping |
| M00-08.03 Message IPC Foundation | ACTIVE | M00-08.03.01 DONE (`9c617ec`); M00-08.03.02 DONE (squash `ba27c4c1a520`, PR #30, CI `32460452557`); M00-08.03.03 ABI candidate at Revision 3 (three review-fix rounds applied; review pending) | 03.03 call/reply + ReplyToken under review; full task-exit IPC cleanup split to M00-08.03.04 |
| Provider-backed pageable component foundation | NEXT | architecture checkpoint | replace direct kernel FlashProvider fault path with blocking provider IPC |
| Real InitService/ISTEP memory continuation | NEXT | roadmap | DDR/exit-contained returns after service/provider substrate exists |
| Structured event and trace ABI | PLANNED | `docs/JIXIA_TRACE_OBSERVABILITY_VISION.md` | ordering follows prerequisites |
| Consolidated automated QEMU harness | PLANNED | milestone scripts remain authoritative | later consolidation only |

---

## Accepted foundation through M00-06

### M00-02 — TrapFrame

The software trap ABI contains x0-x31 plus `mstatus`, `mepc`, `mcause`, and `mtval`. Assembly and C++ share one checked layout.

### M00-03 — recoverable synchronous traps

Recovery is whitelist-based. `EBREAK` and `C.EBREAK` are decoded before saved `mepc` is advanced by the actual instruction length.

### M00-04 — machine timer interrupt

Asynchronous timer handling does not artificially advance saved `mepc`.

### F00-01 — Kernel Print

```text
printk
   -> shared freestanding formatter
   -> KernelLogBuffer
   -> temporary raw-UART mirror
```

### M00-05 — per-hart state and SMP foundation

Accepted invariants:

```text
private stack before normal C/C++
HartId != dense HartIndex
boot hart owns global initialization
release/acquire publication
HartLocal anchored through mscratch
per-hart timer ownership
population discovered from bounded FDT
1/2/4 harts accepted
5 harts rejected as controlled over-capacity case
```

### M00-06 — privilege transition foundation

Accepted runtime stack model:

```text
normal M stack
trusted per-hart M trap stack
synthetic lower-privilege acceptance stack
```

M00-06 proves controlled M->S and S->M->S transitions, trusted lower-origin trap storage, hostile lower-stack resistance, and fail-closed HartLocal anchoring.

The S-mode probes do not define the production Jixia service privilege model.

---

## DONE — M00-07 Pre-DDR Memory Foundation

M00-07 established the minimum memory/storage substrate required before real firmware services exist:

```text
pflash / PNOR-equivalent
    -> XIP Stage0
    -> OpenPOWER-compatible FFS
    -> JXBASE resident Base
    -> contained EarlyMemory
    -> 4 KiB PageManager
    -> Sv39
    -> JXEXT remains pageable
    -> real pre-DDR instruction page fault
    -> pflash -> EarlyMemory page
    -> install mapping
    -> retry exact faulting instruction
```

M00-07.04 also established the future mainstore publication invariant:

```text
DDR hardware online
    !=
mainstore backing committed
    !=
allocator metadata ready
    !=
allocation published
```

Final policy is prepare-before-publish; after mainstore publication there is no hidden fallback to contained allocation.

Full closure evidence: GitHub Actions run `32005255564` passed formatting, build, and all M00-02 through M00-07.04 regressions.

M00-07 is closed as **Pre-DDR Memory Foundation**. Production DDR initialization, real InitService/istep execution, post-DDR provider-backed paging, and real cache-contained retirement are later milestones.

---

## DONE — Hostboot service/InitService architecture gate

The 2026-08-17 source study answered enough of the boot execution model to begin coding.

Key source-derived conclusions:

```text
Hostboot kernel bootstrap
    -> PageManager / HeapManager / VmmManager before first firmware task
    -> create first init task
    -> init task directly creates resident root VFS
    -> barrier waits until root VFS/module table is usable
    -> task_exec("libinitservice.so")
    -> Base InitService
    -> PNOR Base task
    -> Extended VFS Resource Provider Base task
    -> Extended InitService / pageable world
```

Hostboot does not require a Linux-style runtime ELF dynamic linker for Base modules. Its build linker extracts module metadata/entry points into a module table; runtime lookup resolves a module name to the prebuilt `_start` entry.

The Extended VFS Resource Provider registers a virtual block with the already-existing VMM and backs it from PNOR. On a nonresident fault, the kernel allocates a frame, sends a Resource Provider read request, marks the faulting task blocked, schedules the provider, installs translation state on response, and wakes the original task.

Hostboot task dispatch sets POWER problem-state privilege for firmware tasks. This supports a real kernel/service privilege boundary rather than treating firmware services as ordinary kernel callbacks.

Detailed source paths, snippets, and research notes are retained in Google Drive rather than duplicated here.

---

## ACCEPTED — Jixia boot/runtime architecture checkpoint

Canonical record: `docs/JIXIA_BOOT_SERVICE_NATIVE_SBI_ARCHITECTURE_2026-08-17.md`.

### Production boot privilege model

```text
M-mode
    Jixia microkernel
    bare / physical address domain

S-mode
    not used for Jixia boot services
    reserved for later OS/hypervisor

U-mode
    disposable firmware boot services
    Sv39 translated
```

A U task/VSpace owns an explicit translation identity. M00-08 APIs must not hard-code one global `satp`, even if the first acceptance path temporarily shares one simple root.

Kernel code must not dereference U virtual pointers as physical addresses; user-memory access will use explicit copy/translation abstractions.

### Boot-service lifecycle

Boot U-mode services are ephemeral firmware processes. `InitService`, MemoryInit, FabricInit, PCIe/CXL initialization, boot VSpaces/stacks/IPC endpoints, and boot-scoped capabilities must be destroyable at `exit_boot_services()`.

No permanent runtime function may depend on a boot-only U-mode service.

### Component model

ELF remains the compiler/build interchange format. Jixia moves deterministic loader work to image-build time and generates a resident component catalog/manifest containing identity, entry, virtual ranges, permissions, backing, dependencies, and later capability/security metadata.

Pre-DDR runtime does not require a general-purpose Linux dynamic linker.

### Native Jixia SBI/runtime

Jixia Runtime remains the persistent M-mode authority after boot-service teardown and implements the RISC-V SBI standard ABI with Jixia-owned mechanisms.

OpenSBI is a reference implementation, compatibility target, and differential oracle. The complete `lib/sbi` executive is not planned as the authoritative Jixia runtime core because it also owns trap, hart/HSM, domain, PMP/protection, IPI, timer, TLB, IRQ/PMU, heap/scratch, and related machine state.

Persistent runtime responsibilities include SBI, machine-level RAS, security/trust continuity, confidential-computing hooks, and Management Complex coordination.

---

## ACTIVE — M00-08 Boot Service Execution Foundation

Primary goal:

> Promote the accepted privilege-transition and VMM mechanisms into a real first-class boot task model.

Actual increment ledger:

```text
08.01    DONE    TaskContext + U dispatch + task/tracker lifecycle
                 + ready queues + idle + create/yield/end/wait/detach
08.02    DONE    mtime preemption + sleep/wakeup + deadline-aware idle
08.03    ACTIVE  message queue + blocking/wakeup IPC foundation
  .03.01 DONE    non-blocking Endpoint/Message IPC: static 16-slot endpoint
                 table, per-endpoint spinlock + depth-16 FIFO, typed
                 index+generation handles, syscalls 6/7/8/11 (9/10/12
                 reserved -> -ENOSYS); local acceptance x3 PASS
                 (squash 9c617ec, PR #29; CI 32437093429 SUCCESS)
  .03.02 DONE    blocking recv + multi-receiver FIFO wakeup: syscall 10
                 ipc_recv (9/12 stayed -ENOSYS), per-task MessageWaitNode,
                 per-endpoint waiting FIFO, atomic block under ep.lock,
                 send pops exactly one waiter, destroy wakes with -EIDRM,
                 C02/C05/C12/C13a + smp2 cross-hart C19 litmus PASS x3
                 (squash ba27c4c1a520, PR #30; CI 32460452557 SUCCESS)
  .03.03 CAND    call/reply + ReplyToken ABI candidate (Revision 3):
                 syscalls 9/12, self-locating 64-bit token (endpoint
                 epoch + txn slot + txn generation), a6 token out on
                 recv/try_recv (a5 keeps sender TaskId), per-endpoint
                 16-slot transaction table with stable server_task
                 binding, transaction RETIRED ceiling state, current-
                 task-only end_task + frozen state_info blocked-state
                 contract + acquire-release obligation atomic contract,
                 C32/C33 acceptance; docs only — review pending
  .03.04 NEXT    full task-exit IPC cleanup (split from .03.03 by design)
08.04    NEXT    safe user-copy/translation syscall boundary
08.05    NEXT    resident Root Component Registry
08.06    NEXT    init_main -> registry -> InitService
08.07    NEXT    minimal Base InitService task-list lifecycle
```

M00-08.01 absorbed the originally separate minimal scheduler and task-syscall increments because a
real create/yield/end/wait lifecycle could not be accepted as an M-mode sequential call chain.

### Accepted M00-08.01 boundary

```text
M kernel remains bare/physical
TaskContext has explicit VSpace identity
mret enters real U-mode code through Sv39
U ECALL traps to M
current task identity and arguments survive the round trip
termination/return is represented by task state, not probe-specific control flow
```

The accepted path is single-hart and cooperative. It runs real statically resident U code, but it
does not yet launch a named protected firmware service.

### Accepted M00-08.02 direction

```text
machine timer interrupt
    -> save current TaskContext
    -> release expired sleepers
    -> requeue interrupted runnable task
    -> select local/global/idle task
    -> restore selected TaskContext
    -> program next task or sleep deadline
    -> mret
```

The shared bootstrap root pre-maps every fixed-pool task stack before secondary-hart release. This
avoids a live shared-page-table mutation before Jixia has a TLB-shootdown protocol.

### Active M00-08.03 direction

M00-08.03.01 (DONE, `9c617ec`, PR #29) delivered the non-blocking substrate:

```text
static endpoint table (16) + per-endpoint spinlock + depth-16 FIFO
typed index+generation handles (one uint64_t; bit 63 always 0;
generation in [1, 0x7fffffff]; ceiling retires the slot, never wraps)
EndpointManager constructed boot-hart-first (Kernel::ipc_bootstrap,
before secondary hart release; thread-safe-statics are disabled)
endpoint_create/destroy (owner-only) + ipc_send + ipc_try_recv
full register ABI: 4 payload words + receiver-visible sender TaskId
reserved numbers 9/10/12 (call/recv/reply) fail closed with -ENOSYS
```

M00-08.03.02 (DONE, squash `ba27c4c1a520`, PR #30) added blocking recv +
multi-receiver FIFO wakeup: syscall 10, atomic block-under-endpoint-lock
protocol, `.03.02` membership refusals, `DESTROY_WOKE_ALL_EIDRM`-class
acceptance; accepted ABI record `docs/JIXIA_M00_08_03_02_IPC_BLOCKING_RECV_ABI.md`.

M00-08.03.03 (call/reply + ReplyToken) is at the ABI-candidate stage:
`docs/JIXIA_M00_08_03_03_IPC_CALL_REPLY_ABI.md` (CANDIDATE / FROZEN FOR
REVIEW, PR #32, Revision 3) — 64-bit self-locating ReplyToken, per-endpoint
16-slot transaction table, single-endpoint-lock reply path, stable
`server_task` binding, current-task-only `end_task`, frozen
`state_info`/obligation atomic contracts, fail-closed task-exit boundary;
full task-exit IPC cleanup is split to M00-08.03.04. Capability
tables and registry routing remain open; M00-08.03 is not DONE.

---

## NEXT — provider-backed pageable components

Once scheduler and IPC exist:

```text
U service fault
    -> M VMM
    -> allocate page
    -> provider request
    -> faulting task BLOCKED
    -> resident/pinned U provider
    -> PNOR/backing -> page
    -> response
    -> install PTE
    -> wake and retry
```

This milestone replaces M00-07's direct kernel-side `FlashProvider` materialization with the intended service/provider architecture.

---

## Deferred memory continuation

Only after real InitService and provider-backed paging exist:

```text
InitService / memory isteps
    -> host-driven DDR discovery/configuration/training/diagnostics
    -> address map / decode viable
    -> exit contained
    -> mainstore/VMM extension
    -> continue the same firmware/service execution
    -> natural post-DDR PNOR-backed fault
    -> DDR-backed page allocation
```

The point is continuity across the transition, not rebuilding the VMM/page tables after DDR.

---

## Management Complex boundary

Current direction:

```text
Boot Engine / prerequisite management
    -> minimum reset, power, PLL/clock, RoT work needed to release host

Host firmware
    -> heavy HWP/istep work
    -> DDR discovery/config/training/diagnostics
    -> address-map and platform initialization

Management Complex
    -> always-on runtime/OOB plane
    -> RAS aggregation and monitoring
    -> telemetry
    -> watchdog/recovery
    -> power/thermal supervision
    -> BMC communication
    -> predictive health/rule execution

Jixia M Runtime
    -> architectural machine authority
    -> SBI
    -> machine RAS/security/confidential-computing actions
    -> Management Complex bridge
```

Do not inflate Management Complex SRAM/software into a second Hostboot.

---

## Progress history

### 2026-08-22 — M00-08.03.03 third review round applied (docs only)

- Applied round 3 to the PR #32 ABI candidate
  (`docs/JIXIA_M00_08_03_03_IPC_CALL_REPLY_ABI.md`, now Revision 3):
  `end_task` frozen to current-task
  only (`ending_current == false` fails closed at entry, before any state /
  `current_task` / tracker / Task-slot mutation; the non-current teardown
  UAF window is recorded and closed structurally — three atomic loads are no
  longer claimed to support concurrent non-current teardown); the
  `Task.state_info` blocked-state contract frozen (recv → `Endpoint*`, call →
  `Transaction*` + `call_wait_token`; `state_info` is diagnostics only, never
  a membership guard; frozen wake order writes registers → clears token →
  clears `state_info` → `add_task`); the `reply_obligation_count` atomic
  contract frozen (0-init, bound 256, acquire-release RMW, acquire loads at
  end/release, checked increment/decrement, overflow/underflow/double
  decrement fail closed); destroy-first staleness corrected to "endpoint
  epoch mismatch **or** endpoint inactive/retired → `-EINVAL`"; C31 hermetic
  probe contract strengthened (full snapshot/restore coverage, Spinlock never
  copied/reset, static buffers, no-probe build carries no workload); C32
  (cross-endpoint obligation atomicity) and C33 (task-end lifecycle guard,
  probe must exercise the production entry judgment) added.
- Docs only: no production code, `verification/`, host torture, trace
  checker, nightly benchmark, or CI workflow change. Status stays
  CANDIDATE / FROZEN FOR REVIEW; M00-08.03 stays ACTIVE; PR #32 stays
  OPEN / DRAFT / unmerged. Next: ChatGPT third-round review.

### 2026-08-21 — M00-08.03.02 integration recorded; increment flipped DONE

- PR #30 (`feat(ipc): add M00-08.03.02 blocking recv and FIFO wakeup`) merged to
  `main` as squash `ba27c4c1a520ae817a1980c764c89581518a50fd` at
  2026-08-21T08:12:57Z; branch CI run `32460452557` SUCCESS (patch scope and
  hygiene, RV64 QEMU full regression, required regression gates) before merge.
- Increment ledger flipped `.03.02 IMPL* -> .03.02 DONE`;
  `docs/JIXIA_M00_08_03_02_IPC_BLOCKING_RECV_ABI.md` status flipped FROZEN FOR
  IMPLEMENTATION -> ACCEPTED.
- M00-08.03 stays ACTIVE. Next: M00-08.03.03 call/reply + ReplyToken ABI
  candidate (`docs/JIXIA_M00_08_03_03_IPC_CALL_REPLY_ABI.md`, CANDIDATE /
  FROZEN FOR REVIEW); full task-exit IPC cleanup is split to M00-08.03.04.

### 2026-08-21 — M00-08.03.02 blocking recv implemented; PR pending

- Implemented M00-08.03.02 on `agent/m00-08-03-02-ipc-blocking-recv`: syscall 10
  moved from reserved to the blocking `ipc_recv` (9/12 stay `-ENOSYS`, count stays
  13). Success returns `a0=0, a1..a4=payload, a5=sender TaskId`; malformed/stale
  handles return `-EINVAL` immediately; destroy while blocked wakes with `-EIDRM`.
- Data structures: independent per-task `MessageWaitNode` (never aliasing runqueue
  links or `DelayNode`) and per-endpoint `waiting_head/waiting_tail/waiting_count`,
  all statically allocated. The generation-ceiling probe snapshot/restore now
  includes the waiting-FIFO state.
- Atomic block under `endpoint.lock` (enqueue -> `blocked_message` -> `state_info`
  -> `set_next_runnable`, lock released only after the CPU switch; the syscall
  handler never touches the old caller after a `blocked` result). Send pops
  exactly one waiter and publishes READY via `add_task` inside the lock (no
  handoff); destroy wakes every blocked receiver with `-EIDRM`. Waiting receivers
  and pending messages are never both non-empty (checked at every entry);
  `add_task` failure fails closed.
- Scheduler/TaskManager guards: `RunQueue::insert/remove`, `Scheduler::add_task`,
  `set_current_task`, and `release_task_locked` refuse a task still in a waiting
  FIFO — no early run, no silent UAF. Lock order extended one-way to
  `table -> endpoint -> {runqueue, delay-queue}`. Production `printk` remains
  unchanged; SMP correctness uses the verification-only structured trace.
- Acceptance: `scripts/test-m00-08-03-02-ipc-blocking-recv.sh` builds the distinct
  `jixia-verify.bin` test image. The normal `jixia.bin` contains no trace/jitter
  machinery or test-only IPC outputs. Its `--smp 1`
  deterministic C02 (recv-before-send), C05 (two-receiver FIFO pairing + third
  send pends), C12 (destroy `-EIDRM` + stale `-EINVAL`), C13a (timer preemption
  while blocked), C19 drained + reserved `-ENOSYS`, ordered workload markers;
  the independent trace checker reconstructs FIFO waiters and proves one
  wake/result/READY publication per block. Its `--smp 2` C19 64-round stress
  with `-EAGAIN` retry requires both harts and a cross-hart wake. M00-08.01,
  M00-08.02, M00-08.03.01 regressions and the default no-probe build stay green.
- ABI frozen in `docs/JIXIA_M00_08_03_02_IPC_BLOCKING_RECV_ABI.md` (FROZEN FOR
  IMPLEMENTATION / PR pending). M00-08.03 stays ACTIVE: call/reply, ReplyToken,
  task-exit IPC cleanup, capabilities, and registry routing remain open.

### 2026-08-21 — M00-08.03.01 integration recorded; increment flipped DONE

- M00-08.03.01 integration evidence recorded: squash merge `9c617ec` (PR #29,
  `9c617ec7070893086adda1ae33021987dffe513d`) with main-branch CI run
  `32437093429` SUCCESS → `DONE*` flipped to `DONE` in the increment ledgers.
- The squash includes the final PR #29 review round: the generation-ceiling
  probe made hermetic (full-table snapshot before, restore on every exit path,
  pass or fail; Spinlock objects never reset) and the user_task.S C14b
  acceptance-comment relabel.
- M00-08.03 stays ACTIVE: blocking recv/call/reply, ReplyToken/Transact,
  blocked_message transitions, task-exit IPC cleanup, capabilities, and
  registry routing remain open.

### 2026-08-20 — M00-08.02 closed DONE; M00-08.03 activated; M00-08.03.01 implemented

- M00-08.02 integration evidence recorded: squash merge `de4df0e` (PR #26) with full RV64 QEMU
  regression CI run `32219284629` SUCCESS → milestone flipped DONE; M00-08.03 activated.
- Implemented M00-08.03.01 non-blocking Endpoint/Message IPC:
  `microkernel/core/ipc_manager.{h,cpp}` (EndpointManager singleton, 16-slot static table,
  per-endpoint spinlock + depth-16 ring FIFO, typed index+generation handles), frozen syscall
  numbers 6/7/8/11 implemented with 9/10/12 reserved (`-ENOSYS`),
  `JIXIA_TASK_SYSCALL_COUNT` 13 defined exactly once in `task_syscall_abi.h`.
- Lock rules held: send/try_recv take only the endpoint lock; create/destroy nest table →
  endpoint lock only; never two endpoint locks; no TaskManager/TimeManager interaction; no
  scheduler/runqueue call; zero dynamic allocation.
- Accepted ABI/design record added: `docs/JIXIA_M00_08_03_01_IPC_NONBLOCKING_ABI.md`
  (explicitly covers the research candidate's non-blocking leans; all blocking-side
  UNRESOLVED items remain open).
- New acceptance `scripts/test-m00-08-03-01-ipc-nonblocking.sh`: C01 send-before-recv with the full
  register ABI (four distinct payload words + a second sender task, receiver-asserted sender
  TaskId), C03 FIFO order, C14 malformed/stale handles (including a bit-63-set epoch), C14b
  destroy/recreate generation isolation (renamed from the first-pass "C15" label), C15 try_recv
  never blocks (the research acceptance plan's real C15: empty endpoint returns `-EAGAIN`
  immediately, before and after an interleaved send), C16 queue-full `-EAGAIN` + recovery, a
  boot-time generation-ceiling/slot-retirement white-box probe, plus owner-only destroy
  (`-EACCES`), table capacity (`-ENOSPC`), and reserved-ABI `-ENOSYS` evidence — 41 required
  markers with 7 ordered-marker chains; deterministic ×3 local runs PASS; M00-08.01 and
  M00-08.02 regressions PASS; clang-format and `git diff --check` clean.
- Final-review fixes (PR #29 round): restored the acceptance plan's real C15 semantics and
  renamed the destroy/recreate case to C14b; made `Kernel::ipc_bootstrap` construct
  EndpointManager on the boot hart before secondary hart release (thread-safe-statics disabled);
  capped the handle generation at `0x7fffffff` (bit 63 of a legal handle is always 0) with
  permanent slot retirement at the ceiling instead of epoch wraparound (ABA-free); added the
  four-word + sender-TaskId register ABI test.
- M00-08.03 is NOT done: blocking recv/call/reply, ReplyToken/Transact, blocked_message
  transitions, task-exit IPC cleanup, capabilities, and registry routing are all still pending.

### 2026-08-19 — M00-08.02 closure implemented; CI/integration evidence pending

- Un-gated timer arming from the M00-08.02 probe: first dispatch, every rescheduling ECALL exit,
  and every timer-driven dispatch re-arm the absolute `mtimecmp` deadline in all M00-08 builds;
  only the acceptance workload and markers stay probe-gated (M00-08.01 convention).
- Strengthened M00-08.02 acceptance with quantitative assertions: `scheduler_preemption_count >= 1`
  and sleeper wake elapsed at/above the request and below one task timeslice, with the bound
  derived from the kernel-published `M00_08_SCHED_SLICES` constants rather than a magic number
  (observed: 20129/20106/20160 of 20000 ticks against a 100000 task slice, count 1). This fails
  both fixed task-slice polling (~100000) and fixed idle-slice polling (~1000000). Stack
  pre-mapping stays a constructive invariant (mapping loop precedes hart release); no runtime
  page-table instrumentation was added for it.
- Hardened `Scheduler::add_task` to publish READY only after queue insertion succeeds.
- Full local regression chain PASS: M00-02..M00-05, M00-06-02..04, M00-07-01..04, M00-08.01 x2
  (now preemptible, markers unchanged), M00-08.02 x3, default no-probe RV64 build, clang-format.
- Remaining for closure: push `agent/m00-08-02-close`, record the GitHub Actions run ID,
  integrate to `main`. M00-08.02 stays ACTIVE — it is not flipped to DONE in this ledger until
  the CI evidence is recorded. M00-08.03 activates only after that.

### 2026-08-18 — M00-08.01 accepted; M00-08.02 scheduler alignment activated

- Integrated the Hostboot-shaped task executive at `e930a24`.
- Proved U-mode create, cooperative yield/context switch, clean end, completed and blocking wait,
  idle fallback, detach, and full tracker lifecycle with GNU RV64/QEMU.
- Confirmed executive managers are image-wide singletons while current task, local queue, delay
  queue, idle task, and counters are per hart.
- Pinned Hostboot `release-fw1120` reference commit
  `22e3c409ab8b439d4c8eb31b644acb498032a487`.
- Activated M00-08.02 to add mtime-driven preemption, per-hart sleep queues, deadline-aware idle,
  and pre-release task-stack mapping.
- Kept the service boundary explicit: static U tasks are executable; named service startup still
  requires IPC, user-copy, component registry, and protected VSpaces.

### 2026-08-17 — boot-service/runtime architecture frozen; M00-08 activated

- Closed M00-07 and integrated it into `main`.
- Traced Hostboot kernel bootstrap through first init task, root VFS, Base InitService, PNOR, Extended VFS Resource Provider, and Extended InitService.
- Traced the Hostboot provider-backed fault path: kernel allocates and blocks; userspace provider fills; kernel attaches translation and resumes.
- Confirmed Hostboot firmware tasks execute at lower privilege than the kernel.
- Selected Jixia production boot model: M-mode bare microkernel + Sv39 U-mode disposable boot services; S-mode reserved for the later OS/hypervisor.
- Selected build-time ELF preprocessing plus runtime component catalog instead of a general pre-DDR dynamic ELF loader.
- Defined `exit_boot_services()` as a real teardown boundary for the U-mode boot world.
- Selected a native Jixia M-mode SBI/runtime as the persistent RAS/security machine authority; OpenSBI remains reference/differential oracle rather than the authoritative runtime library.
- Activated M00-08 Boot Service Execution Foundation; immediate target is M00-08.01 TaskContext/U-mode dispatch.

### 2026-08-17 — M00-07 accepted

- Adopted FFS-based PNOR image construction and runtime parsing.
- Proved pflash Stage0 -> resident JXBASE transfer.
- Established explicit contained EarlyMemory and 4 KiB PageManager bootstrap pool.
- Built Sv39 page tables before DDR.
- Proved a real instruction page fault can page JXEXT from pflash into EarlyMemory and retry the original instruction.
- Prototyped explicit DDR lifecycle and stable-address contained->mainstore semantics.
- Found and fixed allocator publication ordering: PageManager metadata is prepared before DDR allocation becomes visible.
- Removed the label-only `early_retired` memory-domain idea.
- Full closure CI run `32005255564` passed all M00-02 through M00-07.04 regressions.

Earlier milestone details remain in their dedicated design records and Git history.
