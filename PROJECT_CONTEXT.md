# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
>
> In a new conversation, scan the repository and read this file before relying on conversational memory.

## 1. Canonical identity

- **Project/platform name:** 稷下 / **Jixia**
- **Primary repository:** `Pedroaliu/Firmware`
- **Canonical branch:** `main`
- **Project type:** RISC-V firmware-native server platform research project
- **Purpose:** learning, architecture exploration, and executable system research—not a short path to a commercial UEFI/KVM clone

Jixia studies what a machine looks like when firmware, logical partitions, RAS, trusted/confidential computing, and a full-system simulator are designed together from the first instruction.

IBM POWER/PowerVM/LPAR and System z/CECSIM are studied as alternative systems perspectives to the dominant x86/Arm + Linux/KVM path. The goal is better trade-off reasoning, not a claim of universal superiority.

## 2. Naming policy

Jixia is the project and product brand.

Chinese cultural names are **implementation codenames**, not source-code vocabulary. They may appear in architecture diagrams, releases, presentations, banners, and module descriptions. Source directories, public interfaces, types, functions, schemas, and C++ namespaces use clear English technical meaning.

| Codename | Technical responsibility | Semantic code location / namespace |
|---|---|---|
| **Pangu / 盘古** | Immutable Boot0 | `boot/`, `jixia::boot` |
| **Mozi / 墨子** | Host firmware microkernel | `microkernel/`, `jixia::microkernel` |
| **Nuwa / 女娲** | PlatformGraph/topology | `platform/model/`, `jixia::platform` |
| **ArchHV** | Type-1 firmware hypervisor | `hypervisor/`, `jixia::hypervisor` |
| **Yixing / 弈星** | Scheduling and placement | `jixia::hypervisor::scheduler` |
| **Shouyue / 守约** | Resource contracts/accounting | `jixia::hypervisor::contract` |
| **Dunshan / 盾山** | Isolation/IOMMU/DMA | `jixia::hypervisor::isolation` |
| **Luban / 鲁班** | Linux driver/boot service | `services/driver_domain/`, `jixia::services` |
| **Yuange / 元歌** | Firmware personalities | `firmware_personality/`, `jixia::firmware_personality` |
| **Bianque / 扁鹊** | RAS diagnosis | `ras/diagnosis/`, `jixia::ras::diagnosis` |
| **Taiyi / 太乙** | Recovery | `ras/recovery/`, `jixia::ras::recovery` |
| **Sunbin / 孙膑** | Virtual time/migration continuity | `virtualization/time/`, `jixia::virtualization::time` |
| **Guigu / 鬼谷** | Dynamic debug/introspection | `debug/`, `jixia::debug` |
| **Jingjie / 镜界** | Full-system simulator | `interfaces/simulator/`, `jixia::simulator` |

For confidential computing, keep the technical name **Confidential LPAR** until a stable codename is deliberately chosen.

### Language and ABI rule

- Minimum reset/assembly boundaries remain C-compatible.
- Cross-language symbols use stable `jixia_` C ABI names.
- Freestanding implementation code uses C++ nested namespaces.
- Do not encode codenames into public symbol names, file paths, data schemas, or protocols.

Current examples:

```cpp
namespace jixia::microkernel {}
namespace jixia::microkernel::trap {}
namespace jixia::platform::graph {}
namespace jixia::hypervisor::scheduler {}
namespace jixia::ras::diagnosis {}
namespace jixia::debug::replay {}
```

## 3. Architectural baseline

Jixia supports three execution profiles:

```text
NATIVE_HOST
  Boot0/microkernel -> native HS-mode Linux -> KVM guests

SINGLE_LPAR
  Boot0/microkernel -> ArchHV -> one VS-mode logical partition

MULTI_LPAR
  Boot0/microkernel -> ArchHV -> multiple peer logical partitions
```

A single LPAR is not automatically equivalent to native Linux/KVM. Linux must run in HS-mode to own the RISC-V H extension and act as an ordinary KVM host. A Linux partition under ArchHV runs in VS-mode unless nested virtualization is implemented.

Core principles:

1. Platform model first.
2. The microkernel owns minimum trusted mechanisms, not every feature.
3. Global orchestration belongs to host firmware; local agents contain local faults.
4. Complex physical device drivers do not belong in the minimum hypervisor.
5. Linux endpoint drivers live in a driver service domain.
6. UEFI/ACPI/DT/U-Boot personalities are projections of one filtered PlatformGraph.
7. Resource ownership has one authoritative manager.
8. Partition identity eventually spans translation, interrupts, IOMMU, counters, trace, energy, and RAS.
9. Debug/replay and fault injection are first-class architecture features.
10. Protection, detection, and recovery are designed separately.
11. Normal, measured, and confidential LPARs share one lifecycle model with different trust assumptions.

## 4. CECSIM-style co-design rule

Jixia firmware and the full-system simulator are co-designed.

The simulator is an executable architecture specification and firmware-verification platform covering CPU/SoC execution, firmware state machines, LPARs, I/O, management interactions, topology mismatches, semantic fault injection, trace, checkpoint/replay, coverage, and invariant checking across functional, timing, cycle, and RTL modes.

Every major firmware interface must consider how the simulator observes it, synchronizes with it, injects failures, and verifies recovery.

## 5. Current implementation state

Completed:

- `M00-00`: RV64 QEMU virt reset entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap using `mtvec`, `mcause`, `mepc`, and `mtval`.
- build artifacts renamed from `archfw.*` to `jixia.*`.
- executable implementation moved to semantic `microkernel/` paths.
- low-level C ABI now enters freestanding C++ code under `jixia::microkernel`.

Next milestone:

```text
M00-02  Complete TrapFrame
M00-03  Recoverable trap and mret
M00-04  Timer interrupt
M00-05  Per-hart state
M00-06  Privilege transition
```

Do not jump directly to Linux, migration, split-core, or memory encryption before the trap/privilege foundation is correct and testable.

## 6. New-conversation scan protocol

Before answering a Jixia/Firmware project question in a new chat:

1. Inspect `Pedroaliu/Firmware`, its default branch, and latest commits.
2. Read `PROJECT_CONTEXT.md` and `README.md`.
3. Read `docs/JIXIA_ARCHITECTURE_V0.3.md` and `docs/JIXIA_PROJECT_SOURCES.md`.
4. Inspect current source paths, namespaces, build files, and milestone state.
5. Read relevant historical `ARCHFW_*` documents when useful.
6. Locate referenced PDFs through the current conversation or File Library using the source manifest.
7. Confirm the active simulator repository before editing it.

Repository state wins over remembered chat state unless the user explicitly says the repository is stale.

## 7. Confirmed repositories

### Primary

- `Pedroaliu/Firmware` — Jixia firmware platform; `main` is canonical.

### Related

- `Pedroaliu/RVSoC-Sim-v2` — related newer simulator work; confirm before treating it as active.
- `Pedroaliu/archlab_rvsoc_sim` — earlier simulator repository.
- `Pedroaliu/archlab-rvsoc-sim-t` — related timing experiment.
- `Pedroaliu/archlab-virt` — KVM/virtualization comparison project.
- `Pedroaliu/my-cs-arch-notes` — architecture notes.

### External source

- `open-power/hostboot`, reference branch `release-fw1120`.

The Firmware repository is never interchangeable with similarly named simulator repositories.

## 8. Decisions not to forget

- Jixia is not another EDK II implementation or mini-KVM.
- Native Linux/KVM remains a supported profile and comparison baseline.
- LPAR is a logical-machine contract, not merely `vCPU + RAM`.
- The driver domain uses Linux endpoint drivers directly; host firmware manages platform control and ownership.
- ACPI and DT are generated PlatformGraph views.
- Dynamic debug is cross-backend engineering infrastructure, not a production backdoor.
- Confidential computing constrains debug, DMA, RAS, attestation, and migration designs now.
- BOOM and XiangShan are Core references; IBM POWER contributes partition/RAS/co-design ideas; CECSIM contributes the firmware-simulator verification method.
- Cultural codenames give the project identity; semantic English names and `jixia::*` namespaces keep the implementation globally readable.

## 9. Maintenance

Update this file whenever canonical names, naming policy, repositories, direction, milestone state, core sources, execution profiles, or trust assumptions change.
