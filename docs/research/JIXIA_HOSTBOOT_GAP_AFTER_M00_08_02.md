# Hostboot ↔ Jixia Facility Gap After M00-08.02 — Classification and Roadmap Proposal

- **Date:** 2026-08-19
- **Branch:** `agent/m00-08-03-research`
- **Baseline:** Jixia after M00-08.02 (task executive + preemptive scheduler),
  integrated on `main` as `de4df0e` (PR #26 merged) with GitHub Actions CI run
  `32219284629` — RV64 QEMU regression SUCCESS — vs Hostboot `release-fw1120`
  kernel facilities (study: `JIXIA_HOSTBOOT_IPC_STUDY_2026-08-19.md`).
- **Baseline note (status refreshed 2026-08-20):** M00-08.02 is closed on
  `main`. The `agent/m00-08-02-close` tip analyzed at write time (`2503f89`)
  is the exact content integrated as `de4df0e` (identical tree
  `00d9de9b2e8d2ac70a68d2ee1932eb8d5a454a3b`); its CI run is `32219284629`.
  `main` has since advanced to `f52e542` (CI infrastructure PR #28 —
  hermetic/bounded validation; not an IPC milestone), which changes no
  analyzed baseline content. Canonical project files are not edited from
  this research branch.
- **Status:** PROPOSAL ONLY. The canonical roadmap
  (`docs/JIXIA_SOLO_ROADMAP.md`) is intentionally NOT edited from a research
  branch; adoption happens through the normal milestone gate.

## 1. Classification

Legend: **CRITICAL PATH** = required before the Hostboot-shaped boot-service
vision works end to end; **LATER** = needed eventually, sequenced after the
critical path; **DO NOT COPY** = Hostboot's form is wrong for Jixia, Jixia
deliberately differs.

| Facility | Hostboot has | Jixia after M00-08.02 | Classification | Notes |
|---|---|---|---|---|
| IPC/message queues | full (`msg_*` tri-list queue) | none (M00-08.03 next) | **CRITICAL PATH** | typed-handle register-ABI variant per candidate doc |
| task lifecycle | `TaskManager` create/end/CRASHED paths; kernel reaping of blocked tasks via `recvMessage` endTask | full static lifecycle: create/yield/end (M00-08.01), refusal to end a blocked task (M00-08.02) | **CRITICAL PATH (already adequate)** | shape matches Hostboot's create/end/status model minus dynamic stacks; exit-vs-IPC-state hooks are exactly M00-08.03 transition T8 |
| scheduler | per-CPU scheduler queues, `setNextRunnable` callable from interrupt context, `waiter->cpu` migration | global + local run queues, enqueue-only trap paths, timer preemption (M00-08.02) | **CRITICAL PATH (already adequate)** | sufficient for single-image services; migration/IPI deferred (see SMP rows) |
| MessageHandler (kernel-as-client) | `MessageHandler`+`iv_pending` dedup | none | **LATER** | pattern retained for future VMM RP; needed when kernel must call U services |
| safe user copy / pointer translation | `VmmManager::findKernelAddress` everywhere | none | **CRITICAL PATH** (M00-08.04) | Jixia needs access-checked copy across Sv39 spaces, not same-space translation |
| VMM / resource-provider paging | `Block`/sPTE/`attachSPTE` + RP messages | M00-07 FlashProvider inline materialization | **LATER** (post-registry) | current model pre-IPC by design; provider-over-IPC is the M00-10-class rework |
| component/VFS naming | root VFS msgq + `VFS_MSG_*` over IPC | none | **LATER** | Jixia analog = Root Component Registry (roadmap M00-08.05), kernel-side only a well-known endpoint |
| InitService | extended InitService, `task_exec` from VFS | none (static bootstrap tasks) | **LATER** | after registry + user-copy |
| task_exec / component loading | `task_exec` + VFS binary read into the new task space | none — build-time catalog + safe user-copy planned (08.04/08.06) | **LATER — JIXIA DIFFERENT DESIGN** | Jixia loads from a build-time catalog through checked copy, not from a VFS service, until the registry era; Hostboot's VFS-driven loader is the long-term shape only |
| error handling | errno-return syscall convention, `CritAssert`, checkstop/errl | errno-style negative syscall returns on every task/IPC ABI | **CRITICAL PATH (shape already adequate; containment gap)** | the convention matches Hostboot's errno style; the remaining gap is U-task fault containment (row above), not the return-code discipline |
| SMP IPC | tri-list under one subsystem lock; doorbell broadcast is a no-op → poll-visibility in practice | none yet — M00-08.03 is single-hart with LP-under-`ep.lock` and an smp2 litmus (C19) | **LATER — JIXIA DIFFERENT DESIGN** | per-endpoint locks instead of a global subsystem lock; safety without IPI by construction (litmus C19), IPI only ever a latency optimization |
| futex | `FutexManager` + `sync.h` primitives | none | **LATER** | Jixia prefers message passing first (concurrency-rules bias); add when a real U-mode need exists |
| doorbell/IPI | `msgsnd/msgclr` (+ no-op broadcast!) | none | **DO NOT COPY** blindly / **LATER** | RISC-V has no doorbell; software-generated interrupt (sgi) is the analog; correctness must not depend on it (same lesson as fw1120's stub broadcast) |
| dynamic heap/stack for U tasks | heapmgr + stacksegment growth | static pre-mapped stacks (M00-08.02) | **LATER** | guard-page + grow model after user-copy |
| device mapping | `devMap`/`devUnmap` syscalls | none (static UART only) | **LATER** | needs device-ownership design (Dunshan direction); not before driver-domain milestones |
| fault/crash handling | `CritAssert`, `endTask(TASK_STATUS_CRASHED)`, checkstop data | fatal M-mode trap; task-level end status exists | **CRITICAL PATH (minimal)** | U-task fault containment (kill task, not machine) is required before real services; Hostboot's crash-task path is the reference shape |
| trace/debug | tracemq daemon, `printkd`, errl | KernelLogBuffer + printk | **LATER** | structured trace is a planned foundation; not on the boot critical path |
| SMP scheduling | per-CPU scheduler queues + migration (`waiter->cpu = t->cpu`) | global + local run queues, no migration | **CRITICAL PATH (already adequate)** | M00-08.02 semantics suffice for single-image services; revisit with IPI milestone |

## 2. Proposed next six increments

Aligned with the existing roadmap sketch (08.03 IPC → 08.07 InitService) with
one insertion (fault containment) and explicit deferrals:

```text
1. M00-08.03 Message IPC Foundation          (CRITICAL)
   endpoint/send/recv/call/reply, register-only, per the candidate doc;
   acceptance = C01–C16 plan.
2. M00-08.04 Safe User-Copy Syscall Boundary  (CRITICAL)
   access-checked copy_in/copy_out across Sv39 U spaces; the door to
   buffer IPC, name strings, and real component loading.
3. M00-08.04b U-Task Fault Containment        (CRITICAL, small)
   U-mode exception → end task (crashed) + tracker cleanup, machine lives;
   precondition for any long-running service. (May fold into 08.04 if small.)
4. M00-08.05 Resident Root Component Registry (CRITICAL)
   well-known endpoint + registry service semantics; capability-table
   decision (UNRESOLVED-3) lands here at the latest.
5. M00-08.06 init_main → registry → InitService bootstrap (CRITICAL)
   first service started via IPC instead of static bootstrap.
6. M00-08.07 Minimal Base InitService Lifecycle + provider-over-IPC spike (LATER-borderline)
   InitService istep loop; design study porting the Hostboot MessageHandler/
   RP pattern onto Jixia IPC — implementation of provider paging itself stays
   a later gate.
```

Explicitly deferred beyond the six: futex, device mapping, dynamic stack
growth, structured trace, IPI/remote wake, SMP migration.

Second-pass note (2026-08-19): the expanded comparison (task lifecycle,
scheduler, task_exec/component loading, error handling, SMP IPC) does not
change the six increments — lifecycle/scheduler are already adequate,
task-exec loading is folded into increments 4–5 (08.05/08.06), the errno
shape is already satisfied by the existing syscall style, and SMP IPC stays
behind the litmus-gate policy.

## 3. Ordering rationale (dependencies)

- Registry/InitService cannot precede IPC (Hostboot proves naming itself is a
  service reached *through* IPC) nor user-copy (names, binaries).
- Fault containment must precede InitService: a service crash must not stop
  IPL (Hostboot ends the task and continues).
- Provider-over-IPC must not precede registry (who is the provider is a
  registry question) — hence spike-only in step 6.
- IPI stays out: fw1120's no-op `doorbell_broadcast` demonstrates that IPC
  correctness cannot be built on remote-wake infrastructure; Jixia keeps that
  property permanently (poll-visibility safety, IPI as latency optimization).

## 4. What Jixia will deliberately NOT take from Hostboot

1. raw kernel pointers as user handles / kernel-dereferenced user message
   objects (single-address-space trust model);
2. blind `MSGQ_DESTROY`;
3. `msg_wait_timeout`-style helper-task timeouts as the base mechanism (Jixia
   would integrate the existing delay queue when timeouts are added);
4. interrupt-context direct task switching (`setCurrentTask` from doorbell
   context) — Jixia's trap paths enqueue-only;
5. doorbell/IPI-dependent wake correctness.
