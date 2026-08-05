# Jixia Project Context

> This file is the persistent entry point for future chat sessions, contributors, and coding agents.
>
> Before discussing or modifying this project in a new conversation, scan the repository and read this file first. Do not rely on conversational memory alone.

## 1. Canonical identity

- **Project/platform name:** 稷下 / **Jixia**
- **Primary repository:** `Pedroaliu/Firmware`
- **Canonical branch:** `main`
- **Project type:** RISC-V firmware-native server platform research project
- **Primary purpose:** learning, architecture exploration, and executable system research; not a short path to a commercial UEFI/KVM clone

Jixia studies what a machine looks like when firmware, logical partitions, RAS, security, confidential computing, and a full-system simulator are designed together from the first instruction.

The project deliberately studies IBM POWER/PowerVM/LPAR and System z/CECSIM ideas because they provide a different systems perspective from the dominant x86/Arm + Linux/KVM path. The goal is not to claim universal superiority. The goal is to understand alternative placements of complexity and make better trade-offs.

## 2. Canonical component names

These names are now architectural names, not temporary chat nicknames.

| Name | Canonical responsibility |
|---|---|
| **Jixia / 稷下** | Entire firmware-native server platform and this repository |
| **Pangu / 盘古** | Future immutable Boot0 / first-instruction root |
| **Mozi / 墨子** | Host firmware microkernel; current executable code lives under `mozi/` |
| **Nuwa / 女娲** | PlatformGraph, topology construction, topology repair, and generated platform views |
| **ArchHV** | Firmware-native type-1 hypervisor; keep the technical name for the whole hypervisor |
| **Yixing / 弈星** | ArchHV scheduling and placement subsystem |
| **Shouyue / 守约** | Resource contracts, entitlement, accounting, and policy enforcement |
| **Dunshan / 盾山** | Isolation, IOMMU, DMA windows, and resource protection |
| **Luban / 鲁班** | Linux driver and boot service domain; device drivers, storage, network, filesystems, RAID tools |
| **Yuange / 元歌** | Firmware personality framework: UEFI, ACPI, SBI+DT, U-Boot/FIT, minimal test personality |
| **Bianque / 扁鹊** | RAS diagnosis, error classification, FFDC correlation, predictive failure |
| **Taiyi / 太乙** | Recovery, restart, failover, rollback, and degraded-mode actions |
| **Sunbin / 孙膑** | Virtual time, migration time continuity, deterministic replay time model |
| **Guigu / 鬼谷** | Dynamic debug, introspection, event breakpoints, fault injection, checkpoint/replay control plane |
| **Jingjie / 镜界** | Full-system simulator/co-simulation environment and the CECSIM-style execution world |

For confidential computing, use the technical term **Confidential LPAR** until a stable cultural name is deliberately chosen. Do not invent one casually.

## 3. Architectural baseline

Jixia supports three execution profiles:

```text
NATIVE_HOST
  Pangu/Mozi -> native HS-mode Linux -> KVM guests

SINGLE_LPAR
  Pangu/Mozi -> ArchHV -> one VS-mode logical partition

MULTI_LPAR
  Pangu/Mozi -> ArchHV -> multiple peer logical partitions
```

A single LPAR is not automatically equivalent to native Linux/KVM. Linux must run in HS-mode to own the RISC-V H extension and act as an ordinary KVM host. A Linux partition under ArchHV runs in VS-mode unless nested virtualization is implemented.

Core architectural principles:

1. Platform model first.
2. Mozi owns minimum trusted platform mechanisms, not every feature.
3. Global orchestration belongs to host firmware; local agents contain local faults.
4. Physical device drivers do not belong in the minimum hypervisor.
5. Complex drivers live in Luban or dedicated service domains.
6. Yuange personalities are projections of one filtered virtual PlatformGraph, not independent sources of truth.
7. Resource ownership has one authoritative manager.
8. Partition identity must eventually span translation, interrupts, IOMMU, counters, trace, energy, and RAS.
9. Debug/replay and fault injection are first-class architecture features.
10. Protection, detection, and recovery are designed separately.
11. Normal, measured, and confidential LPARs share one lifecycle model but have different trust assumptions.

## 4. CECSIM-style co-design rule

Jixia firmware and Jingjie/RVSoC-Sim are co-designed.

The simulator is not only a performance model. It is an executable architecture specification and firmware verification platform covering:

- CPU and SoC execution;
- firmware and service state machines;
- LPARs and virtual I/O;
- management-plane interactions;
- PhysicalModelGraph versus FirmwarePlatformGraph mismatches;
- precise event-driven fault injection;
- trace, checkpoint, replay, coverage, and invariant checking;
- fast functional, timing, cycle, and RTL co-simulation modes.

Every major firmware interface should consider how Jingjie observes it, synchronizes with it, injects failures into it, and verifies recovery.

## 5. Current implementation state

Completed before the Jixia naming transition:

- `M00-00`: RV64 QEMU virt reset entry, hart filtering, `gp`, stack, BSS, UART, and `kernel_main`.
- `M00-01`: minimal fatal M-mode trap entry using `mtvec`, `mcause`, `mepc`, and `mtval`.

Naming transition:

- build artifacts: `archfw.*` -> `jixia.*`;
- executable microkernel source: `kernel/` -> `mozi/`;
- `kernel_main` -> `mozi_main`.

Next milestone remains:

```text
M00-02  Complete TrapFrame
M00-03  Recoverable trap and mret
M00-04  Timer interrupt
M00-05  Per-hart state
M00-06  Privilege transition
```

Do not jump directly to Linux, migration, split-core, or memory encryption before the privilege/trap foundation is correct and testable.

## 6. Repository scan protocol for a new conversation

Before answering a Jixia/Firmware project question in a new chat:

1. Inspect `Pedroaliu/Firmware` metadata, default branch, and latest commits.
2. Read `README.md`.
3. Read this `PROJECT_CONTEXT.md`.
4. Read `docs/JIXIA_ARCHITECTURE_V0.2.md` and `docs/JIXIA_PROJECT_SOURCES.md`.
5. Inspect the current source tree and build files; code is newer than old chat summaries.
6. Read relevant historical design records under `docs/`, including older `ARCHFW_*` documents when useful.
7. Locate referenced PDFs in the current conversation or File Library by the titles recorded in `docs/JIXIA_PROJECT_SOURCES.md`.
8. Verify which simulator repository is active before editing it; do not silently edit a similarly named repository.

When repository state conflicts with conversational memory, repository state wins unless the user explicitly says the repository is stale.

## 7. Confirmed Git repositories

### Primary

- `Pedroaliu/Firmware` — Jixia firmware platform; `main` is canonical.

### Related user repositories

- `Pedroaliu/RVSoC-Sim-v2` — related newer simulator work; scan before assuming it is the active Jingjie implementation.
- `Pedroaliu/archlab_rvsoc_sim` — earlier simulator repository.
- `Pedroaliu/archlab-rvsoc-sim-t` — related simulator/timing experiment repository.
- `Pedroaliu/archlab-virt` — separate KVM/virtualization study and useful mainstream comparison baseline.
- `Pedroaliu/my-cs-arch-notes` — architecture notes library.

### External source repositories

- `open-power/hostboot`, reference branch `release-fw1120` — IBM/OpenPOWER host firmware source reference.

The exact active Jingjie repository must be confirmed from the current task before writes. `Pedroaliu/Firmware` is never interchangeable with the simulator repositories.

## 8. Decisions that should not be forgotten

- Jixia is not another EDK II implementation and not another mini-KVM.
- Native Linux/KVM remains a supported execution profile and comparison baseline.
- LPAR is a first-class logical-machine contract, not merely `vCPU + RAM`.
- Luban uses Linux endpoint drivers directly; Mozi manages platform control and ownership rather than reimplementing NVMe/NIC/RAID drivers.
- ACPI and DT are generated views from Nuwa's filtered PlatformGraph.
- Guigu is a cross-backend debug framework, not a simulator-only feature and not a production backdoor.
- Confidential computing is a long-term requirement and must constrain debug, DMA, RAS, attestation, and migration designs now.
- BOOM and XiangShan are Core references; IBM POWER contributes partition/RAS/co-design ideas; CECSIM contributes the firmware-simulator verification method.

## 9. Updating this file

Update this file whenever any of the following changes:

- canonical component names;
- primary or active repositories;
- project direction;
- completed milestone;
- next milestone;
- core reference sources;
- trust model or execution profiles.

This file is intentionally concise enough to scan at the start of every new project conversation.
