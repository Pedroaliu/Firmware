# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-18
- **Stable integration branch:** `main`
- **Latest completed milestone:** `M00-08.01 Hostboot-shaped Task Executive` — DONE
- **M00-08.01 integration:** `e930a24ebcfc5be969feedca5733f5e906eee656`
- **M00-08.01 acceptance:** GNU RV64 build/link and single-hart QEMU task lifecycle PASS
- **Primary M00-07 acceptance evidence:** GitHub Actions run `32005255564` — full RV64 QEMU regression SUCCESS
- **Architecture research gate:** Hostboot kernel/VFS/InitService/service-startup path — SUFFICIENTLY CLOSED FOR IMPLEMENTATION
- **Current implementation milestone:** `M00-08.02 Hostboot Scheduler Alignment` — ACTIVE
- **Immediate next step:** close RV64/QEMU evidence for mtime preemption, sleep/wakeup, and idle deadlines
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
| M00-08.02 Hostboot Scheduler Alignment | ACTIVE | `docs/JIXIA_M00_08_02_HOSTBOOT_SCHEDULER_ALIGNMENT.md` | mtime preemption, per-hart delay queue, deadline-aware idle, stack pre-mapping |
| M00-08.03 Message IPC Foundation | NEXT | Hostboot syscall/message path | blocking/wakeup IPC required before real service registry |
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
08.01  DONE    TaskContext + U dispatch + task/tracker lifecycle
               + ready queues + idle + create/yield/end/wait/detach
08.02  ACTIVE  mtime preemption + sleep/wakeup + deadline-aware idle
08.03  NEXT    message queue + blocking/wakeup IPC
08.04  NEXT    safe user-copy/translation syscall boundary
08.05  NEXT    resident Root Component Registry
08.06  NEXT    init_main -> registry -> InitService
08.07  NEXT    minimal Base InitService task-list lifecycle
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

### Active M00-08.02 direction

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
