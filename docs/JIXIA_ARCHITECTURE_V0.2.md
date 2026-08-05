# Jixia Firmware-Native Server Architecture v0.2

## Status

This is the canonical architecture entry for the Jixia project after the 2026-08-05 naming and direction decision.

Older `ARCHFW_*` documents remain useful historical design records, but the names and boundaries in this document are canonical.

## 1. Vision

Jixia is a RISC-V firmware-native server platform designed as a learning and architecture research project.

It combines ideas from:

- IBM POWER Hostboot, PHYP/PowerVM, LPAR, PFW, VIOS, and RAS;
- IBM System z CECSIM-style firmware and full-system simulation;
- RISC-V privilege architecture, H extension, AIA/IMSIC, and IOMMU;
- microkernel systems such as seL4, QNX, Zircon, and L4;
- UEFI/PI, coreboot, U-Boot, LinuxBoot, and Petitboot;
- secure firmware development, roots of trust, attestation, and confidential computing;
- BOOM and XiangShan as understandable modern Core references.

The project asks:

> What would a modern RISC-V server look like if Partition were a first-class object from the first firmware instruction, and if the SoC, firmware, hypervisor, RAS, security, and simulator were designed together?

## 2. Project identity

```text
Jixia / 稷下
  Entire firmware-native platform

Pangu / 盘古
  Immutable Boot0 and first-instruction root

Mozi / 墨子
  Host firmware microkernel

Nuwa / 女娲
  PlatformGraph and topology construction/repair

ArchHV
  Firmware-native type-1 hypervisor

Luban / 鲁班
  Linux device and boot service domain

Yuange / 元歌
  Firmware personality framework

Bianque / 扁鹊
  RAS diagnosis

Taiyi / 太乙
  Recovery actions

Guigu / 鬼谷
  Dynamic debug and introspection

Jingjie / 镜界
  Full-system simulation and co-simulation world
```

ArchHV retains a technical whole-system name. Its planned subsystems include:

```text
Yixing / 弈星
  Scheduling and placement

Shouyue / 守约
  Entitlement, resource contracts, and accounting

Dunshan / 盾山
  Isolation, IOMMU, DMA, and ownership enforcement

Sunbin / 孙膑
  Virtual time and migration continuity
```

## 3. High-level architecture

```text
                         ArchMC / BMC
                              |
                    management protocols
                              |
+----------------------------------------------------------------+
| Jixia                                                          |
|                                                                |
|  Pangu Boot0                                                   |
|       |                                                        |
|  Mozi microkernel                                              |
|       +-- Nuwa PlatformGraph                                   |
|       +-- ownership / capability / secure launch               |
|       +-- Bianque diagnosis                                    |
|       +-- Taiyi recovery                                       |
|       +-- Guigu probe and debug protocol                       |
|       +-- service lifecycle                                    |
|                                                                |
|  Boot and device services                                      |
|       +-- Luban Linux driver domain                            |
|       +-- image / policy / measurement services                |
|                                                                |
|  Firmware personalities                                        |
|       +-- Yuange UEFI + ACPI                                   |
|       +-- Yuange SBI + DT                                      |
|       +-- Yuange U-Boot/FIT                                    |
|       +-- minimal test personality                             |
|                                                                |
|  Runtime choice                                                 |
|       +-- Native HS-mode Linux/KVM                             |
|       `-- ArchHV -> one or more peer LPARs                     |
+----------------------------------------------------------------+
                              |
                     RISC-V SoC / Jingjie
```

## 4. Execution profiles

### 4.1 Native host

```text
Pangu -> Mozi -> Luban/boot services -> native HS-mode Linux -> KVM guests
```

This is the mainstream compatibility path. Linux owns the H extension and runs ordinary KVM guests.

### 4.2 Single LPAR

```text
Pangu -> Mozi -> ArchHV -> one VS-mode LPAR
```

This preserves the Jixia partition contract, lifecycle, telemetry, ownership, and RAS model even when only one logical machine exists.

It is not identical to native Linux/KVM: a VS-mode Linux cannot act as an ordinary HS-mode KVM host without nested virtualization support.

### 4.3 Multiple peer LPARs

```text
Pangu -> Mozi -> ArchHV
                    +-- Luban service LPAR
                    +-- Linux LPAR
                    +-- UEFI/ACPI LPAR
                    `-- other peer LPARs
```

There is no general-purpose Host OS that is naturally above all guests. Service LPARs may have special responsibilities and capabilities, but remain partition objects governed by explicit contracts.

## 5. Mozi microkernel contract

Mozi contains the smallest practical trusted platform mechanism set:

- trap and interrupt entry;
- hart lifecycle;
- memory-domain primitives;
- capability and ownership enforcement;
- typed IPC and service discovery;
- secure component launch;
- minimum scheduler/service lifecycle mechanisms;
- root RAS event routing;
- measurement and audit hooks;
- Guigu debug probe with explicit policy.

Mozi should not contain:

- full NVMe, NIC, RAID, USB, FC, or GPU drivers;
- complete UEFI, ACPI, DT, or U-Boot compatibility logic;
- large filesystems and network stacks;
- arbitrary management-plane policy;
- simulator-only analysis UI.

Logical service separation does not always require immediate physical isolation. Early milestones may link services statically; later versions can move the same versioned interfaces into isolated service domains or LPARs.

## 6. Pangu Boot0

Pangu is a future, deliberately tiny first-instruction root.

Responsibilities:

- establish the minimum execution environment;
- identify the boot hart;
- initialize minimum immutable trust state;
- authenticate/measure Mozi;
- select normal or recovery path;
- transfer control with a minimal handoff structure.

The current `mozi/arch/riscv/start.S` is an M00 bootstrap and is not yet a fully separated Pangu implementation.

## 7. Nuwa PlatformGraph

Nuwa owns the canonical model of platform facts and relationships.

The source model is expected to be data-driven, initially using CUE or a similarly typed configuration language.

It represents:

- CPUs, harts, cores, clusters, sockets, and dies;
- caches, TLB domains, NoC links, and memory controllers;
- memory ranges, NUMA nodes, and RAS domains;
- PCIe roots, switches, functions, CXL devices, and IOMMU contexts;
- interrupt controllers and routes;
- BMC, SCP, SBE-like agents, GPU/NPU/DPU firmware agents;
- power, clock, reset, security, ownership, and health relations.

Nuwa produces filtered views:

```text
Physical PlatformGraph
        |
ownership and policy filter
        |
LPAR Virtual PlatformGraph
        +-- DTB
        +-- ACPI tables
        +-- SMBIOS/inventory
        +-- Yuange UEFI view
        +-- simulator topology
        `-- management inventory
```

ACPI and DT are views, not independent owners of platform truth.

Jingjie maintains a separate `PhysicalModelGraph`. Tests must be able to create deliberate disagreement:

```text
PhysicalModelGraph != FirmwarePlatformGraph
```

## 8. ArchHV and the Jiuzhou partition model

The logical partition architecture may be described conceptually as **Jiuzhou / 九州**, while code continues to use precise terms such as `lpar`, `lpid`, and `vmid`.

An LPAR is a logical-machine object, not merely a large host process and not merely `vCPU + RAM`.

A complete contract eventually includes:

- LPID/VMID and BootEpoch;
- virtual processor count and maximum parallelism;
- entitled capacity;
- capped/uncapped policy and weight;
- dispatch, stolen, donated, and scaled time accounting;
- logical memory and translation context;
- interrupt context;
- virtual and dedicated device ownership;
- partition firmware personality;
- time view;
- measurement and attestation state;
- performance, energy, and I/O accounting;
- RAS and recovery state;
- migration compatibility profile.

Partition identity should eventually span:

- G-stage translation and TLB tags;
- interrupt targeting;
- IOMMU domains and DMA transactions;
- device ownership;
- NoC and memory traffic;
- performance counters;
- energy accounting;
- trace and replay;
- RAS events and FFDC.

## 9. ArchHV subsystems

### Yixing scheduler

- vCPU dispatch;
- shared processor pools;
- entitlement enforcement;
- capped/uncapped borrowing;
- affinity and NUMA placement;
- cede/prod/confer-like cooperative operations;
- migration and cache-affinity policy.

### Shouyue resource contracts

- CPU entitlement and weight;
- memory quotas and placement;
- I/O and bandwidth budgets;
- energy and power policy;
- actual consumption accounting;
- policy audit and SLA evidence.

### Dunshan isolation

- G-stage ownership enforcement;
- IOMMU domains;
- DMA windows;
- MMIO authorization;
- shared-memory grants;
- device ownership transfer;
- management-plane access boundaries.

### Sunbin virtual time

- virtual timers;
- timebase offsets;
- pause and resume semantics;
- migration time continuity;
- deterministic replay timeline.

## 10. Luban Linux driver and boot service

Luban is a small Linux-based service appliance, not the platform owner and not the root of trust.

It exists to reuse:

- PCI endpoint drivers;
- NVMe/SCSI/RAID/HBA/NIC drivers;
- filesystems and network stacks;
- RAID and storage administration tools;
- boot-device and boot-candidate discovery;
- firmware update tooling where policy permits.

Boundary:

```text
Mozi/Jixia manages platform control and ownership.
Luban manages device function through assigned MMIO, DMA, and interrupts.
```

Mozi initializes SoC-level roots, clocks, resets, PCIe RC foundations, IOMMU primitives, interrupt foundations, and RAS ownership. Luban initializes the assigned endpoint function using the native Linux driver.

A device has an explicit ownership state machine. Mozi and Luban must never concurrently treat the same endpoint as freely owned.

## 11. Yuange firmware personalities

Yuange translates one virtual PlatformGraph into an OS-facing machine personality.

Initial personalities:

- SBI + Device Tree direct Linux boot;
- UEFI + ACPI;
- U-Boot/FIT/extlinux compatibility;
- minimal test payload.

Yuange does not independently decide CPU, memory, or device ownership. It queries authoritative state and presents it in a compatible form.

Static table generation and runtime event adaptation are separate responsibilities. For example, ACPI table generation is distinct from ACPI SCI/GPE runtime event delivery.

## 12. RAS: Bianque and Taiyi

### Bianque diagnosis

- classify errors;
- correlate hardware, local-agent, firmware, LPAR, and device symptoms;
- identify source, owner, and affected scope;
- generate structured FFDC;
- decide whether containment is proven;
- recommend recovery actions.

### Taiyi recovery

- retry or alternate path;
- service restart;
- vCPU or LPAR restart;
- device reset/reassignment;
- page retirement and topology degradation;
- checkpoint restore;
- firmware/configuration recovery;
- failover and migration.

The key rule is:

```text
closest component contains first
owner diagnoses responsibility
workload owner recovers
management plane preserves service history
```

RAS may know where a confidential workload failed without gaining the right to read customer plaintext.

## 13. Guigu dynamic debug and introspection

Guigu is a cross-backend engineering control plane.

Objects include:

- harts and vCPUs;
- LPARs;
- firmware services;
- IPC transactions;
- PlatformGraph nodes;
- device ownership;
- IOMMU mappings;
- interrupts;
- boot states;
- RAS cases.

Capabilities include:

- source/PC breakpoints;
- event breakpoints;
- per-hart, per-vCPU, per-LPAR, per-service, or global pause;
- display/alter with audit;
- trace filtering by LPID, BootEpoch, transaction, layer, and event;
- conditional fault injection;
- checkpoint/replay;
- state diff between successful and failed runs;
- invariant checking.

Backends:

- QEMU;
- Jingjie functional/timing models;
- RTL/Verilator;
- Palladium/emulation;
- FPGA;
- silicon via DMI/JTAG/BMC-approved service paths.

Security modes:

```text
OBSERVE
  read-only trace, counters, and metadata

SERVICE
  controlled dump, reset, and RAS operations

LAB
  pause, alter, inject, checkpoint, and replay
```

LAB is disabled by default in production. Debug state is measured, signed, capability-controlled, and audited. A debug-enabled confidential LPAR cannot receive production secrets.

## 14. Jingjie and CECSIM-style co-design

Jingjie is the full-system execution world for firmware development and verification.

It includes or coordinates:

- functional, timing, and cycle-accurate CPU models;
- BOOM/XiangShan-inspired educational OoO Core development;
- cache, TLB, NoC, DDR, PCIe, CXL, IOMMU, and interrupt models;
- firmware agents and management processors;
- LPARs and virtual I/O;
- dynamic configuration;
- event scripts;
- fault injection;
- trace and replay;
- target-side tests;
- coverage extraction;
- RTL co-simulation.

The simulator and firmware share:

- Nuwa schema;
- service IDL;
- event schema;
- trace schema;
- fault schema;
- LPID and BootEpoch;
- target test cases;
- ArchSimCall-style synchronization.

The simulator must support precise semantic events rather than relying only on UART text or fixed tick numbers.

## 15. Security architecture

Jixia separates:

```text
Protection
Detection
Recovery
```

It also distinguishes roots or chains of trust for:

- update;
- detection;
- recovery;
- measurement/reporting;
- secure device communication.

Every major feature should define:

1. control flow;
2. data flow;
3. assets;
4. adversaries;
5. trust boundaries;
6. threats and mitigations;
7. recovery flow;
8. security tests.

Luban, Yuange, BMC, devices, and management tools are not trusted merely because they are separate components.

## 16. Confidential computing

Jixia plans three partition security classes:

```text
Normal LPAR
  trusts Mozi and ArchHV

Measured LPAR
  carries measured boot and attestation evidence

Confidential LPAR
  does not require trust in ArchHV, Luban, BMC, or cloud administrator
```

A future confidential security monitor, likely in M-mode or a more isolated hardware-backed domain, controls:

- private memory ownership;
- encryption/integrity key lifecycle;
- protected register save areas;
- shared/private page transitions;
- protected interrupt and IOMMU context;
- measurement and attestation;
- debug lifecycle;
- secure migration.

SoC research requirements include:

- per-LPID memory encryption;
- memory integrity and anti-replay;
- protected register state;
- LPID-aware translation and I/O;
- attestation engine;
- trusted DMA or explicit shared-page I/O;
- secure migration sessions.

## 17. Core direction

Jingjie's first owned RISC-V OoO Core should be understandable and observable before it is wide or fast.

Reference split:

- BOOM: clear modern OoO decomposition and educational readability;
- XiangShan: higher-performance front-end, memory subsystem, verification, and engineering ideas;
- IBM POWER: SMT, partition identity, RAS, telemetry, and hardware/firmware co-design.

Initial target:

```text
RV64 frontend
  -> rename/dispatch
  -> small issue queues
  -> integer + load/store execution
  -> ROB/commit
  -> cache/TLB
```

Use standard RISC-V H/AIA/IOMMU semantics first. Add custom partition hardware only when experiments show a concrete need.

## 18. Current repository layout

```text
Firmware/
  PROJECT_CONTEXT.md
  README.md

  mozi/
    arch/riscv/
    core/

  pangu/                 future Boot0 contract
  nuwa/                  PlatformGraph contract
  archhv/                partition runtime contracts
  services/luban/        Linux driver/boot service
  personalities/yuange/  OS-facing firmware personalities
  ras/bianque/           diagnosis
  recovery/taiyi/        recovery
  debug/guigu/            debug and introspection
  time/sunbin/            virtual time and migration
  security/confidential/ confidential LPAR design
  interfaces/jingjie/     firmware-simulator boundary

  platform/qemu_virt/
  linker/
  scripts/
  docs/
```

Directory READMEs define planned ownership. Placeholder directories do not imply completed implementation.

## 19. Milestones

```text
M00  Mozi machine-mode foundation
M01  First HS/VS transition and first LPAR
M02  Partition contract, VPA, entitlement, accounting
M03  Two peer LPARs and virtual time
M04  Luban, ArchVIO, message queues, memory grants
M05  Partition-scoped RAS and recovery
M06  Measured LPAR and attestation
M07  Linux guest and native Linux/KVM profile
M08  Migration and compatibility profiles
M09  Confidential LPAR prototype
M10  Jingjie timing Core/SoC partition experiments
```

Current next task remains a complete, testable TrapFrame.
