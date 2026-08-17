# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-17
- **Stable integration branch:** `main`
- **Milestone closure branch:** `milestone/m00-07-memory-foundation`
- **Latest completed milestone:** `M00-07 Pre-DDR Memory Foundation` — DONE
- **Primary acceptance evidence:** GitHub Actions run `32005255564` — full RV64 QEMU regression SUCCESS
- **Immediate next step:** Hostboot kernel/VFS/InitService/istep startup research gate
- **Next implementation milestone:** intentionally not frozen until that research gate closes

## Status legend

| Status | Meaning |
|---|---|
| `DONE` | implementation accepted and evidence retained |
| `ACTIVE` | the single current implementation milestone |
| `RESEARCH` | architecture study required before a milestone is frozen |
| `NEXT` | ordered immediately after the current gate |
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
| Hostboot service/InitService startup study | RESEARCH | current architecture gate | study kernel->VFS->user/service->InitService->istep->memory transition before coding |
| Structured event and trace ABI | PLANNED | `docs/JIXIA_TRACE_OBSERVABILITY_VISION.md` | ordering may move behind service substrate |
| Consolidated automated QEMU harness | PLANNED | milestone scripts remain authoritative | later consolidation only |

---

## Accepted foundation through M00-06

### M00-02 — TrapFrame

The software trap ABI contains x0-x31 plus `mstatus`, `mepc`, `mcause`, and `mtval`. Assembly and C++ share one checked layout.

```text
TRAP_FRAME_TEST: PASS
```

### M00-03 — recoverable synchronous traps

Recovery is whitelist-based. `EBREAK` and `C.EBREAK` are decoded before saved `mepc` is advanced by the actual instruction length.

```text
RECOVERABLE_TRAP_TEST: PASS
```

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
    ordinary M firmware execution

trusted per-hart M trap stack
    all runtime traps handled in M

lower-privilege probe stack
    synthetic S-mode acceptance contexts
```

M00-06 proves:

```text
controlled M->S
controlled S->M->S ECALL
lower-origin sp is preserved only as a value
TrapFrame remains in trusted HartLocal trap storage
hostile S sp cannot redirect privileged storage
missing trusted HartLocal anchor fails closed
```

The S-mode probes do not yet define the production Jixia service privilege model.

---

## DONE — M00-07 Pre-DDR Memory Foundation

### Objective achieved

Jixia can now treat firmware as a resident Base plus pageable Extended content and can service a real firmware page fault before DDR becomes allocator-visible.

Accepted pre-DDR path:

```text
pflash / PNOR-equivalent
    -> XIP Stage0
    -> FFS
    -> JXBASE resident Base
    -> contained EarlyMemory
    -> PageManager
    -> Sv39
    -> JXEXT remains in pflash
    -> real instruction page fault
    -> pflash -> EarlyMemory page
    -> install mapping
    -> retry exact faulting instruction
```

M00-07.04 also establishes a mechanism prototype for future mainstore transition:

```text
DDR lifecycle prototype
    -> DDR ONLINE
    -> TRANSITIONING
    -> contained flush/castout point
    -> MAINSTORE semantic commit
    -> PageManager promote/register DDR metadata
    -> allocation still gated
    -> explicit publish
    -> DDR allocation
```

### PNOR / FFS foundation

The accepted pflash image uses an OpenPOWER-compatible FFS v1 table rather than a private fixed Base/Extended header.

```text
BOOT0   fixed XIP bootstrap partition
JXBASE  resident Base, discovered by FFS identity
JXEXT   optional pageable Extended partition
```

Stage0 knows the FFS TOC bootstrap location, not a hard-coded JXBASE data offset. The resident FlashProvider reuses the FFS parser to locate pageable firmware content.

### EarlyMemory / PageManager

QEMU models an 8 MiB semantic contained domain beginning at `0x80000000`. This is not a claim about a physical QEMU cache.

A linker-reserved 64 KiB bootstrap pool provides 4 KiB allocator pages. Sv39 maps the resident 8 MiB using six page-table pages:

```text
1 L2 root
1 L1
4 L0
= 24 KiB
```

The accepted rule remains:

> 4 KiB is the unit of ownership; larger pages are later translation optimizations.

### Real pre-DDR paging acceptance

Primary command:

```bash
bash scripts/test-m00-07-03-pre-ddr-paging.sh
```

Core evidence:

```text
M00_07_PRE_DDR_PAGING_ARMED: PASS
M00_07_PRE_DDR_PAGE_FAULT: PASS
M00_07_PRE_DDR_FLASH_READ: PASS
M00_07_PRE_DDR_BACKING_EARLY: PASS
M00_07_PRE_DDR_PAGING_RESUME: PASS
M00-07.03 pre-DDR flash-backed paging: PASS
```

The synthetic S-mode context exists only to produce a real Sv39 instruction page fault. It is not a user service or InitService.

### Mainstore publish-last correctness

Review of M00-07.04 found a real state-machine gap: the old code enabled DDR allocation as part of `complete_mainstore_transition()` before PageManager had promoted the contained range and registered the remaining DDR range.

The corrected publication order is:

```text
DDR hardware online
    !=
mainstore backing committed
    !=
allocator metadata ready
    !=
allocation published
```

Final policy:

```text
CONTAINED                -> contained allocation allowed
TRANSITIONING             -> allocation forbidden
MAINSTORE, gate closed    -> allocation forbidden
MAINSTORE, gate open      -> DDR allocation only
```

There is no hidden fallback from exhausted/unavailable DDR to contained memory after mainstore publication.

Primary command:

```bash
bash scripts/test-m00-07-04-mainstore-transition.sh
```

Core evidence:

```text
M00_07_DDR_ALLOCATOR_GATED: PASS
M00_07_DDR_DISCOVERED: PASS
M00_07_DDR_TRAINING: PASS
M00_07_DDR_TRAINED: PASS
M00_07_DDR_TOPOLOGY_READY: PASS
M00_07_DDR_ADDRESS_MAP_READY: PASS
M00_07_DDR_DECODE_COMMITTED: PASS
M00_07_DDR_ONLINE: PASS
M00_07_CONTAINED_FLUSH: PASS
M00_07_STABLE_ADDRESS: PASS
M00_07_MAINSTORE_ALLOCATOR_GATED: PASS
M00_07_MAINSTORE_TRANSITION: PASS
M00_07_MAINSTORE_EXTEND: PASS
M00-07.04 fake DDR lifecycle and mainstore transition: PASS
```

### Full closure evidence

GitHub Actions run `32005255564` passed:

```text
patch hygiene / clang-format
configure and build
TrapFrame regression
recoverable trap regression
machine timer regression
Kernel Print regression
SMP population regression
M00-06.02
M00-06.03
M00-06.04
M00-07.01
M00-07.02
M00-07.03
M00-07.04
```

### Scope freeze

M00-07 is closed as **Pre-DDR Memory Foundation**.

The following are not unfinished 07.05 items:

```text
production Host-driven DDR initialization
real InitService/istep execution
post-DDR PNOR paging into DDR
pre-DDR page-table continuity across real exit-contained
real cache-contained retirement on hardware/SimSoc
```

They are deferred until the Hostboot-style firmware service execution flow exists.

The unused `MemoryDomain::early_retired` idea was removed. If future hardware requires real early-memory draining, unmapping, invalidation, or power gating, model that as a separate lifecycle with actual side effects rather than adding a label-only memory domain.

---

## RESEARCH — Hostboot service/InitService startup gate

### Why this gate is next

The M00-07 mechanisms are sufficient to support pageable pre-DDR firmware. The next architectural question is not another memory allocator feature; it is how Jixia transitions from early host bootstrap into real firmware services and an InitService/istep control flow.

Primary study chain:

```text
Hostboot Base/kernel entry
    -> task/scheduler foundation
    -> VMM
    -> VFS
    -> PNOR Resource Provider
    -> first user/service task
    -> InitService
    -> istep module execution
    -> HWP calls
    -> memory isteps
    -> proc_exit_cache_contained
    -> MM_EXTEND_REAL_MEMORY / VMM extension
```

### Required decisions before coding

- exact Hostboot kernel/user startup boundary;
- what must remain resident before DDR;
- how VFS/PNOR fault handling interacts with task scheduling and Resource Providers;
- InitService versus HWP/platform mechanism responsibilities;
- host versus Boot Engine/Management Complex DDR ownership;
- final RISC-V M/S/U mapping for Jixia kernel and services;
- where seL4-style capabilities/address-space isolation should strengthen the Hostboot model;
- where NXP-style component manifests/dependency packaging should be adopted.

No synthetic `ECALL -> initialize DDR` control flow will be used as a substitute for this architecture.

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
```

Do not inflate Management Complex SRAM and firmware into a second Hostboot unless a real hardware dependency requires it.

---

## Planned queue after the research gate

The next implementation milestone identifier is deliberately TBD. Likely direction is a minimal Hostboot-style firmware service execution substrate.

Existing planned work remains valid but may be reordered behind that prerequisite:

```text
PLANNED  firmware task/service execution
PLANNED  InitService/istep foundation
PLANNED  typed IPC and capability ownership
PLANNED  service address spaces and fault containment
PLANNED  structured event and trace ABI
PLANNED  consolidated QEMU harness
PLANNED  PlatformGraph runtime model
PLANNED  post-DDR memory continuation under real boot flow
```

Only one primary implementation milestone is active at a time.

---

## Branch / integration rule

Development branches may contain fine-grained commits. Accepted milestones are integrated into `main` as semantic checkpoints, normally by squash merge.

```text
main
  |
  +-- milestone branch
          -> implementation
          -> tests / CI
          -> design record
          -> closure records
          -> squash accepted checkpoint into main
```

Do not leave a completed milestone branch unintegrated while `main` becomes stale.

---

## Progress history

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
- Closed M00-07 as Pre-DDR Memory Foundation.
- Opened the Hostboot service/InitService startup research gate before the next implementation milestone.

### 2026-08-13 — M00-06 closed

- Hostile lower-privilege x2/sp proof accepted.
- Trusted M TrapFrame storage remained on the per-hart trap stack.
- Missing `mscratch -> HartLocal` lower-origin path failed closed.
- M00-07 Memory Foundation activated.

### 2026-08-11 to 2026-08-12 — M00-05/M00-06 foundation

- SMP population, HartLocal, per-hart stacks/timers accepted.
- Controlled M->S and S->M->S paths accepted.
- Repository formatting and CI enforcement established.

Earlier milestone details remain in their dedicated design records and Git history.
