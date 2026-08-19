# Jixia Firmware-Native Server Architecture v0.3

## Status

This is the canonical Jixia architecture after the 2026-08-05 decision to separate project codenames from source-code vocabulary.

Jixia remains the public project/platform name. Cultural names remain implementation codenames. Source layout, APIs, schemas, symbols, and C++ namespaces use clear English technical meaning.

## 1. Vision

Jixia is a RISC-V firmware-native server platform built for learning and architecture research.

It combines ideas from:

- IBM POWER Hostboot, PHYP/PowerVM, LPAR, PFW, VIOS, and RAS;
- IBM System z CECSIM-style firmware/full-system simulation;
- RISC-V privilege architecture, H extension, AIA/IMSIC, and IOMMU;
- seL4, QNX, Zircon, L4, and other microkernel systems;
- UEFI/PI, coreboot, U-Boot, LinuxBoot, and Petitboot;
- roots of trust, attestation, firmware resiliency, and confidential computing;
- BOOM and XiangShan as modern Core references.

The project asks:

> What would a modern RISC-V server look like if Partition were a first-class object from the first firmware instruction, and if the SoC, firmware, hypervisor, RAS, security, and simulator were designed together?

## 2. Naming and language policy

### 2.1 Project and codenames

```text
Jixia / 稷下
  Entire firmware-native platform and public project brand

Pangu / 盘古
  Codename for immutable Boot0

Mozi / 墨子
  Codename for the host firmware microkernel

Nuwa / 女娲
  Codename for PlatformGraph/topology construction and repair

Luban / 鲁班
  Codename for the Linux driver and boot service domain

Yuange / 元歌
  Codename for the firmware personality framework

Bianque / 扁鹊
  Codename for RAS diagnosis

Taiyi / 太乙
  Codename for recovery

Guigu / 鬼谷
  Codename for dynamic debug and introspection

Jingjie / 镜界
  Codename for the full-system simulation world
```

ArchHV remains the technical name of the firmware-native type-1 hypervisor. Yixing, Shouyue, Dunshan, and Sunbin are codenames for scheduler, resource-contract, isolation, and virtual-time subsystems.

### 2.2 Semantic source names

Codenames do not become public code vocabulary.

```text
boot/                    jixia::boot
microkernel/             jixia::microkernel
platform/model/          jixia::platform
hypervisor/              jixia::hypervisor
services/driver_domain/  jixia::services::driver_domain
firmware_personality/    jixia::firmware_personality
ras/diagnosis/           jixia::ras::diagnosis
ras/recovery/            jixia::ras::recovery
debug/                   jixia::debug
virtualization/time/     jixia::virtualization::time
interfaces/simulator/    jixia::simulator
security/confidential/   jixia::security::confidential
```

A contributor should be able to understand the repository without knowing Chinese history or game references.

### 2.3 ABI boundary

The minimum bootstrap boundary remains C-compatible:

```text
assembly / hardware entry
        |
small jixia_ C ABI symbols
        |
freestanding C++ jixia::* implementation
```

Examples:

```cpp
namespace jixia::microkernel {}
namespace jixia::microkernel::trap {}
namespace jixia::platform::graph {}
namespace jixia::hypervisor::scheduler {}
namespace jixia::ras::diagnosis {}
namespace jixia::debug::replay {}
```

No exceptions, RTTI, global-constructor dependency, or hosted C++ runtime is assumed in the trusted core.

## 3. High-level architecture

```text
                         ArchMC / BMC
                              |
                    management protocols
                              |
+----------------------------------------------------------------+
| Jixia                                                          |
|                                                                |
|  Boot0                                                         |
|       |                                                        |
|  Host firmware microkernel                                     |
|       +-- PlatformGraph / ownership / capability               |
|       +-- secure launch / measurement                          |
|       +-- RAS diagnosis and recovery routing                   |
|       +-- dynamic-debug probe                                  |
|       +-- service lifecycle                                    |
|                                                                |
|  Boot and device services                                      |
|       +-- Linux driver domain                                  |
|       +-- image / policy / measurement services                |
|                                                                |
|  Firmware personalities                                        |
|       +-- UEFI + ACPI                                          |
|       +-- SBI + Device Tree                                    |
|       +-- U-Boot/FIT                                           |
|       +-- minimal test personality                             |
|                                                                |
|  Runtime choice                                                 |
|       +-- Native HS-mode Linux/KVM                             |
|       `-- ArchHV -> one or more peer LPARs                     |
+----------------------------------------------------------------+
                              |
                  RISC-V SoC / full-system simulator
```

## 4. Execution profiles

### Native host

```text
Boot0 -> microkernel -> boot/driver services -> HS-mode Linux -> KVM guests
```

Linux owns the RISC-V H extension and uses the mainstream KVM ecosystem.

### Single LPAR

```text
Boot0 -> microkernel -> ArchHV -> one VS-mode LPAR
```

This preserves Jixia ownership, lifecycle, telemetry, security, and RAS semantics even with one logical machine. It is not ordinary Linux/KVM unless nested virtualization is added.

### Multiple peer LPARs

```text
Boot0 -> microkernel -> ArchHV
                             +-- driver/service LPAR
                             +-- Linux LPAR
                             +-- UEFI/ACPI LPAR
                             `-- other peer LPARs
```

There is no general-purpose Host OS naturally above every workload. Service LPARs can have special capabilities but remain explicit partition objects.

## 5. Host firmware microkernel

The microkernel contains the smallest practical trusted mechanism set:

- trap and interrupt entry;
- hart lifecycle;
- memory-domain primitives;
- capability and ownership enforcement;
- typed IPC and service discovery;
- secure component launch;
- minimum service scheduling/lifecycle;
- root RAS event routing;
- measurement and audit hooks;
- policy-controlled dynamic-debug hooks.

It does not contain full NVMe/NIC/RAID/USB/FC/GPU drivers, large filesystems/network stacks, complete UEFI/ACPI/DT/U-Boot compatibility, arbitrary cloud policy, or simulator UI.

Logical service separation can precede physical isolation. Early milestones may statically link a service behind a versioned interface; later milestones can move it into a protected service domain or LPAR.

## 6. Boot0

Boot0 is deliberately tiny:

- establish minimum reset state;
- identify the boot hart;
- initialize immutable trust state;
- authenticate and measure the microkernel;
- choose normal or recovery flow;
- pass a minimal handoff structure.

The current `microkernel/arch/riscv/start.S` is an M00 bootstrap, not yet a separated Boot0 implementation.

## 7. Platform model

The PlatformGraph is the authoritative model of platform facts and relationships.

It represents:

- CPUs, harts, cores, clusters, sockets, and dies;
- caches, TLB domains, NoC links, and memory controllers;
- memory ranges, NUMA nodes, and RAS domains;
- PCIe roots, switches, functions, CXL devices, and IOMMU contexts;
- interrupt controllers and routes;
- BMC/SCP/SBE-like and accelerator firmware agents;
- power, clock, reset, security, ownership, and health relations.

It generates filtered views:

```text
Physical PlatformGraph
        |
ownership and policy filter
        |
LPAR Virtual PlatformGraph
        +-- Device Tree
        +-- ACPI tables
        +-- SMBIOS/inventory
        +-- UEFI configuration view
        +-- simulator topology
        `-- management inventory
```

ACPI and Device Tree are views, not independent owners of platform truth.

The simulator maintains a separate physical model. Tests must permit deliberate disagreement:

```text
PhysicalModelGraph != FirmwarePlatformGraph
```

## 8. Logical partition contract

The logical partition architecture may use **Jiuzhou / 九州** as a presentation codename, while code uses `lpar`, `lpid`, `vmid`, `vcpu`, and other standard terms.

An LPAR is a logical-machine contract, not merely `vCPU + RAM`.

A complete contract eventually includes:

- LPID/VMID and BootEpoch;
- virtual processor count and maximum parallelism;
- entitled capacity;
- capped/uncapped policy and weight;
- dispatch, stolen, donated, and scaled time accounting;
- logical memory and translation context;
- interrupt context;
- virtual and dedicated device ownership;
- firmware personality;
- time view;
- measurement and attestation state;
- performance, energy, and I/O accounting;
- RAS and recovery state;
- migration compatibility profile.

Partition identity should eventually span G-stage/TLB tags, interrupts, IOMMU/DMA, device ownership, NoC/DDR traffic, counters, energy, trace, and FFDC.

## 9. Hypervisor subsystems

### Scheduler

- vCPU dispatch;
- shared processor pools;
- entitlement enforcement;
- capped/uncapped borrowing;
- affinity and NUMA placement;
- cede/prod/confer-like cooperation;
- migration and cache-affinity policy.

### Resource contracts

- CPU entitlement and weight;
- memory quotas and placement;
- I/O/bandwidth budgets;
- energy and power policy;
- consumption accounting;
- audit and SLA evidence.

### Isolation

- G-stage ownership enforcement;
- IOMMU domains and DMA windows;
- MMIO authorization;
- shared-memory grants;
- device ownership transfer;
- management-plane access boundaries.

### Virtual time

- virtual timers and timebase offsets;
- pause/resume semantics;
- migration time continuity;
- deterministic replay timeline.

## 10. Linux driver and boot service domain

The driver domain is a small Linux-based service appliance, not the platform owner and not automatically part of the minimum TCB.

It reuses:

- PCI endpoint drivers;
- NVMe/SCSI/RAID/HBA/NIC drivers;
- filesystems and network stacks;
- storage/RAID administration tools;
- boot-device and boot-candidate discovery;
- selected update tools.

Boundary:

```text
Host firmware manages platform control and ownership.
The driver domain manages assigned endpoint function.
```

Host firmware initializes clocks, resets, PCIe root foundations, IOMMU primitives, interrupt foundations, and RAS ownership. Linux initializes an assigned endpoint with its native driver.

Every device follows an explicit ownership state machine. Firmware, service domain, and production LPAR must never concurrently believe they freely own the same function.

## 11. Firmware personalities

The personality framework translates one filtered virtual PlatformGraph into an OS-facing machine model.

Initial personalities:

- SBI + Device Tree direct Linux boot;
- UEFI + ACPI;
- U-Boot/FIT/extlinux compatibility;
- minimal test payload.

A personality presents state; it does not decide CPU, memory, or device ownership. Static table generation and runtime event delivery are separate interfaces.

## 12. RAS diagnosis and recovery

Diagnosis:

- classify errors;
- correlate hardware, agent, firmware, LPAR, and device symptoms;
- identify source, owner, and affected scope;
- generate structured FFDC;
- establish whether containment is proven;
- recommend recovery.

Recovery:

- retry or alternate path;
- service restart;
- vCPU or LPAR restart;
- device reset/reassignment;
- page retirement and topology degradation;
- checkpoint restore;
- firmware/configuration recovery;
- failover and migration.

Key rule:

```text
closest component contains first
owner identifies responsibility
workload owner recovers
management plane preserves service history
```

RAS may know where a confidential workload failed without gaining authority to read customer plaintext.

## 13. Dynamic debug and introspection

The debug framework is a cross-backend engineering control plane operating on semantic objects:

- harts and vCPUs;
- LPARs;
- firmware services and transactions;
- PlatformGraph nodes;
- device ownership and IOMMU mappings;
- interrupts, boot states, and RAS cases.

Capabilities:

- PC/source and semantic-event breakpoints;
- selective or global pause;
- audited display/alter;
- trace filtering by LPID, BootEpoch, transaction, layer, and event;
- conditional fault injection;
- checkpoint/replay;
- successful/failed state diff;
- invariant checking.

Backends include QEMU, functional/timing simulator models, RTL/Verilator, emulation, FPGA, and approved silicon debug paths.

Security modes:

```text
OBSERVE  read-only trace, counters, and metadata
SERVICE  controlled dump, reset, and RAS operations
LAB      pause, alter, inject, checkpoint, and replay
```

LAB is disabled by default in production. Debug state is measured, signed, capability-controlled, and audited. Debug-enabled confidential LPARs do not receive production secrets.

## 14. Simulator and CECSIM-style co-design

The full-system simulator is the executable architecture and firmware-verification environment.

It includes or coordinates:

- functional, timing, and cycle CPU models;
- BOOM/XiangShan-inspired educational OoO Core development;
- cache, TLB, NoC, DDR, PCIe, CXL, IOMMU, and interrupt models;
- firmware agents and management processors;
- LPARs and virtual I/O;
- dynamic configuration;
- semantic event scripts and fault injection;
- trace, checkpoint, replay, target tests, coverage, and RTL co-simulation.

Firmware and simulator share PlatformGraph schemas, IDL, event/trace/fault schemas, LPID, BootEpoch, target tests, invariants, and ArchSimCall-style synchronization.

The simulator must not rely only on UART text or fixed tick numbers.

## 15. Security and confidential computing

Jixia separates Protection, Detection, and Recovery.

Every major feature defines control flow, data flow, assets, adversaries, trust boundaries, threats/mitigations, recovery flow, and security tests.

Jixia plans three partition security classes:

```text
Normal LPAR
  trusts the microkernel and ArchHV

Measured LPAR
  carries measured-boot and attestation evidence

Confidential LPAR
  does not require trust in ArchHV, driver domain, BMC, or cloud administrator
```

A future hardware-backed security monitor controls private memory ownership, memory-encryption/integrity keys, protected register save areas, shared/private transitions, protected interrupt/IOMMU context, attestation, debug lifecycle, and secure migration.

SoC research includes per-LPID memory encryption, integrity/anti-replay, protected state, LPID-aware translation and I/O, attestation, trusted DMA or shared-page I/O, and secure migration sessions.

## 16. Core direction

The first owned OoO Core should be understandable and observable before it is wide or fast.

- BOOM contributes clear modern OoO decomposition and educational readability.
- XiangShan contributes higher-performance front-end, memory-system, verification, and engineering ideas.
- IBM POWER contributes SMT, partition identity, RAS, telemetry, and hardware/firmware co-design.

Initial target:

```text
RV64 frontend
  -> rename/dispatch
  -> small issue queues
  -> integer + load/store execution
  -> ROB/commit
  -> cache/TLB
```

Use standard RISC-V H/AIA/IOMMU semantics first. Add custom partition hardware only when experiments demonstrate a concrete need.

## 17. Repository layout

```text
Firmware/
  PROJECT_CONTEXT.md
  README.md

  boot/
  microkernel/
    arch/riscv/
    core/
  platform/
    model/
    qemu_virt/
  hypervisor/
  services/driver_domain/
  firmware_personality/
  ras/diagnosis/
  ras/recovery/
  debug/
  virtualization/time/
  security/confidential/
  interfaces/simulator/

  linker/
  scripts/
  docs/
```

Directory READMEs define planned ownership. Placeholder directories do not imply completed implementation.

## 18. Milestones

```text
M00  Machine-mode microkernel foundation
M01  First HS/VS transition and first LPAR
M02  Partition contract, VPA, entitlement, accounting
M03  Two peer LPARs and virtual time
M04  Driver domain, virtual I/O, message queues, memory grants
M05  Partition-scoped RAS and recovery
M06  Measured LPAR and attestation
M07  Linux guest and native Linux/KVM profile
M08  Migration and compatibility profiles
M09  Confidential LPAR prototype
M10  Timing Core/SoC partition experiments
```

The current implementation increment is M00-08.02 Hostboot Scheduler Alignment: preemptive U-task
dispatch, sleep/wakeup, and idle deadline policy on top of the accepted TrapFrame and task
lifecycle foundation. Detailed live ordering remains in `docs/JIXIA_SOLO_ROADMAP.md`.
