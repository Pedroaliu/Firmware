# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
> Repository state and accepted design records are authoritative over conversational memory.

## 1. Canonical identity

- **Project/platform:** 稷下 / **Jixia**
- **Repository:** `Pedroaliu/Firmware`
- **Stable integration branch:** `main`
- **Latest completed milestone:** `M00-07 Pre-DDR Memory Foundation`
- **Current implementation branch during closure:** `milestone/m00-07-memory-foundation`
- **Project type:** RISC-V firmware-native server platform research project

Jixia studies firmware, logical partitions, RAS, confidential computing, management-plane design, and full-system simulation as one co-designed platform. It is a learning and architecture project, not a short path to cloning EDK II, KVM, PowerVM, or any single existing firmware stack.

Canonical live status: `docs/JIXIA_PROGRESS.md`.  
Canonical execution plan: `docs/JIXIA_SOLO_ROADMAP.md`.

## 2. Architecture reference priority

Whole-system firmware boot flow is Hostboot-first.

```text
1. IBM Hostboot
   primary reference for:
   - IPL control flow
   - kernel/user firmware split
   - InitService and istep orchestration
   - HWP execution model
   - PNOR/VFS/resource providers
   - cache-contained operation
   - memory initialization
   - exit-contained/mainstore transition
   - RAS/PRD integration patterns

2. Jixia platform requirements
   determine where Jixia intentionally differs.

3. seL4 and related microkernels
   secondary reference for:
   - capability security
   - address-space isolation
   - least privilege
   - kernel/service mechanism boundaries
   - fault containment

4. NXP and similar firmware frameworks
   secondary reference for:
   - component manifests
   - dependencies
   - versioning
   - standardized package/service boundaries

5. Linux/other operating systems
   implementation and comparison reference where applicable.
```

Do not invent a generic microkernel boot flow first and retrofit firmware behavior later. For boot, memory, PNOR, istep/HWP, runtime transition, and RAS lifecycle questions, inspect Hostboot first.

## 3. Host versus Management Complex boundary

The Management Complex is not intended to become a second Hostboot.

Preferred responsibility split:

```text
Boot Engine / minimum prerequisite logic
    -> make the host safely executable
    -> root of trust / secure load prerequisites
    -> reset release
    -> minimum power and PLL/clock prerequisites
    -> minimum fabric/pervasive setup needed to release the host

Host Jixia firmware
    -> make the platform operational
    -> microkernel and services
    -> InitService / istep orchestration
    -> heavy HWP libraries
    -> processor/fabric initialization
    -> SPD/VPD/attribute processing
    -> DDR configuration/training/diagnostics
    -> memory grouping/interleave/address map
    -> PCIe/CXL and later platform initialization

Management Complex
    -> keep the platform manageable even when the host is unhealthy
    -> always-on runtime and out-of-band control
    -> RAS event aggregation and monitoring
    -> watchdog and recovery coordination
    -> telemetry
    -> power/thermal supervision
    -> BMC/OOB communication
    -> predictive RAS/rule execution and health monitoring
```

Heavy boot algorithms should remain on host cores where resident Base + contained memory + PNOR demand paging can support code larger than the early-memory capacity. Avoid requiring a large Management Complex SRAM merely to execute host initialization libraries.

## 4. Development model

The project is intentionally single-threaded:

```text
NOW       one primary implementation milestone or one architecture research gate
NEXT      at most a few ordered items
BACKLOG   accepted later work
FROZEN    work blocked by missing prerequisites
```

Milestone completion requires:

```text
architecture/invariants
-> implementation
-> machine-checkable acceptance
-> regression preservation
-> design/progress records
-> integration into main
```

Development branches may contain fine-grained implementation/debug commits. Accepted milestones are integrated into `main` as semantic checkpoints, normally by squash merge.

## 5. Naming policy

Jixia is the project and product brand. Chinese cultural names are implementation codenames, not public source-code vocabulary.

| Codename | Responsibility | Semantic code area |
|---|---|---|
| Pangu / 盘古 | immutable Boot0 | `boot/`, `jixia::boot` |
| Mozi / 墨子 | host firmware microkernel | `microkernel/`, `jixia::microkernel` |
| Nuwa / 女娲 | PlatformGraph/topology | `platform/model/`, `jixia::platform` |
| ArchHV | firmware-native type-1 hypervisor | `hypervisor/` |
| Yixing / 弈星 | scheduling/placement | hypervisor scheduler |
| Shouyue / 守约 | resource contracts | hypervisor contracts |
| Dunshan / 盾山 | isolation/IOMMU/DMA | isolation layer |
| Luban / 鲁班 | Linux driver/boot domain | `services/driver_domain/` |
| Yuange / 元歌 | firmware personalities | `firmware_personality/` |
| Bianque / 扁鹊 | RAS diagnosis | `ras/diagnosis/` |
| Taiyi / 太乙 | recovery | `ras/recovery/` |
| Sunbin / 孙膑 | virtual time/migration | `virtualization/time/` |
| Guigu / 鬼谷 | dynamic debug/introspection | `debug/` |
| Jingjie / 镜界 | full-system simulator | `interfaces/simulator/` |

Source directories, interfaces, types, functions, schemas, and C++ namespaces use clear English technical names.

## 6. Long-term execution profiles

```text
NATIVE_HOST
    Boot0/microkernel -> native HS-mode Linux -> KVM guests

SINGLE_LPAR
    Boot0/microkernel -> ArchHV -> one VS-mode logical partition

MULTI_LPAR
    Boot0/microkernel -> ArchHV -> multiple peer logical partitions
```

Do not prematurely map Hostboot's logical kernel/user split directly onto RISC-V M/S/U privilege levels. M00-06/07 S-mode code is currently an acceptance context, not the final service model. The production M/S/U placement will be decided after studying the Hostboot kernel/VFS/InitService startup path and defining Jixia service isolation requirements.

## 7. Architectural baseline

Core principles:

1. Hostboot-first firmware lifecycle and flow.
2. Platform model before OS-facing projections.
3. The microkernel owns minimum trusted mechanisms, not every feature.
4. Global boot orchestration belongs to host firmware.
5. The Management Complex remains an always-on management/RAS control plane, not a heavy alternate host.
6. Complex device drivers should not inflate the minimum trusted kernel/hypervisor.
7. Resource ownership has one authoritative manager.
8. Message passing and explicit capabilities are preferred over shared implicit authority.
9. Protection, detection, diagnosis, and recovery are separate mechanisms.
10. Debug/replay/fault injection are first-class architecture features.
11. Firmware and Jingjie are co-designed.

## 8. Accepted implementation through M00-07

### M00-00 through M00-04

- RV64 QEMU virt reset entry, stacks, BSS, UART.
- minimal fatal M trap.
- complete integer TrapFrame and common save/restore path.
- recoverable 32-bit `EBREAK` and 16-bit `C.EBREAK`.
- recoverable machine timer interrupt.
- Kernel Print foundation.

### M00-05 — SMP foundation

Accepted:

```text
private per-hart stacks
HartId != dense HartIndex
boot-hart-owned global initialization
release/acquire publication
HartLocal
mscratch -> HartLocal
bounded FDT population discovery
per-hart timer state/compare
1/2/4-hart acceptance
controlled over-capacity rejection
```

### M00-06 — privilege transition foundation

Accepted:

```text
trusted per-hart M trap stack
M-origin and lower-origin M traps use trusted trap storage
interrupted lower-privilege sp preserved only as a value
controlled M->S transition
controlled S->M->S ECALL round trip
hostile S sp proof
missing HartLocal anchor fails closed
```

M00-06 does not yet define the production service privilege model.

### M00-07 — Pre-DDR Memory Foundation

Accepted:

```text
32 MiB pflash/PNOR-equivalent image
OpenPOWER-compatible FFS v1 partition table
XIP Stage0
JXBASE discovery by FFS partition identity
resident Base transfer
explicit contained EarlyMemory state
4 KiB PageManager bootstrap pool
Sv39 page-table construction from EarlyMemory
resident FFS parser and FlashProvider
JXEXT left pageable in pflash
real pre-DDR instruction page fault
pflash -> EarlyMemory fill
RX PTE install and exact instruction retry
fake DDR lifecycle/mainstore mechanism prototype
stable firmware address/content across backing transition
PageManager contained->DDR metadata promotion
prepare-before-publish allocator gating
no mainstore fallback to contained allocation
```

Design record: `docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`.

M00-07 intentionally does not finish a production DDR boot flow. Its DDR/mainstore code is a mechanism prototype used to establish invariants for the later Hostboot-style flow.

## 9. Immediate next architecture research gate

Before the next major implementation milestone, study the Hostboot startup chain end-to-end:

```text
Hostboot Base/kernel entry
    -> task/thread foundation
    -> VMM
    -> VFS and PNOR Resource Provider
    -> initial user/service execution
    -> InitService
    -> istep module loading/execution
    -> HWP invocation
    -> memory isteps
    -> proc_exit_cache_contained
    -> MM_EXTEND_REAL_MEMORY / VMM mainstore extension
```

Questions to settle before implementing Jixia services:

- When does Hostboot first leave pure kernel/bootstrap execution and start user/service tasks?
- Which pieces must remain resident before DDR?
- How do VFS/PNOR page faults block and resume a task/provider?
- What is the exact ownership boundary between InitService, HWP/platform code, and kernel VMM mechanisms?
- What RISC-V M/S/U mapping best preserves Hostboot-style flow while improving protection with capability/isolation ideas?
- At what exact point should Jixia host-driven DDR initialization occur?

Only after this research gate should the next service/InitService implementation milestone be frozen.

## 10. Deferred memory continuation

Later, under the real Hostboot-style boot flow:

```text
InitService / memory isteps
    -> host-driven DDR discovery/configuration/training/diagnostics
    -> address map / decode viable
    -> exit contained
    -> mainstore/VMM extension
    -> continue service execution
    -> natural post-DDR PNOR-backed page fault
    -> allocate DDR backing
    -> prove pre-DDR page tables and mappings survived the transition
```

Real cache-contained retirement and dirty-line castout semantics belong to Jingjie/real hardware validation rather than the QEMU semantic model alone.

## 11. PNOR persistence direction

M00-07 paging establishes read-side backing. Persistent mutation follows a separate rule:

> Read is a pageable backing operation; write is a privileged persistent transaction.

Ordinary CPU stores must never implicitly write firmware storage. Immutable firmware partitions are RO/RX; future updates/VPD/GUARD/config persistence use explicit scoped services and capabilities.

## 12. RAS direction

Power-style deterministic RAS diagnosis remains the trusted spine. Jixia extends it with structured evidence, topology/ownership correlation, active HWP probes, case memory, replay, and optional AI-assisted hypothesis/rule discovery while keeping accepted recovery policy deterministic and auditable.

The Management Complex is especially important for host-independent runtime RAS collection, monitoring, watchdog, telemetry, and OOB recovery coordination.

## 13. Read-first order for future sessions

1. `PROJECT_CONTEXT.md`
2. `docs/JIXIA_PROGRESS.md`
3. `docs/JIXIA_SOLO_ROADMAP.md`
4. `docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`
5. relevant architecture/RAS records
6. current branch, recent commits, and current code

Repository state wins over remembered chat state whenever they differ.
