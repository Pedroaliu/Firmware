# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
>
> In a new conversation, scan the repository and read this file before relying on conversational memory.

## 1. Canonical identity

- **Project/platform name:** 稷下 / **Jixia**
- **Primary repository:** `Pedroaliu/Firmware`
- **Stable integration branch:** `main`
- **Current progress branch:** `feature/console-foundation`
- **Parked timer branch:** `milestone/m00-04-timer-interrupt` at `299aff177497399236a848724b56c2e040ce4db4`
- **Project type:** RISC-V firmware-native server platform research project
- **Purpose:** learning, architecture exploration, and executable system research—not a short path to a commercial UEFI/KVM clone

Jixia studies what a machine looks like when firmware, logical partitions, RAS, trusted/confidential computing, and a full-system simulator are designed together from the first instruction.

IBM POWER/PowerVM/LPAR and System z/CECSIM are studied as alternative systems perspectives to the dominant x86/Arm + Linux/KVM path. The goal is better trade-off reasoning, not a claim of universal superiority.

## 2. Development mode

The project currently has one human developer working with ChatGPT as a research, teaching, architecture, review, debugging, and implementation partner.

The project is intentionally single-threaded:

```text
NOW      exactly one primary feature/milestone
NEXT     at most three ordered milestones
BACKLOG  accepted later work
FROZEN   work blocked by architectural prerequisites
```

The canonical execution plan is `docs/JIXIA_SOLO_ROADMAP.md`. The canonical live status is `docs/JIXIA_PROGRESS.md`.

A feature or milestone is not complete merely because code exists. Completion requires test evidence and a recorded design/learning result.

### Learning/implementation workflow

The current teaching workflow deliberately separates syntax fluency from systems reasoning:

- ChatGPT may provide and commit complete reference implementations for syntax-heavy, repetitive, or foundational scaffolding.
- The developer is expected to understand the architectural state transitions, invariants, failure modes, and debugging evidence behind core mechanisms.
- New mechanisms are taught through complete reference code first, then explanation, guided modification, and progressively larger independent implementation tasks.
- Debugging should prefer observable evidence (GDB, CSR/register state, disassembly, QEMU logs, tests) over guessing.
- The pace should remain milestone-driven: do not turn every syntax detail into a separate blocking exercise.
- Independent facilities should use independent branches and acceptance evidence; do not hide a large Console/platform feature inside an unrelated timer milestone.

## 3. Naming policy

Jixia is the project and product brand.

Chinese cultural names are implementation codenames, not source-code vocabulary. They may appear in architecture diagrams, releases, presentations, banners, and module descriptions. Source directories, public interfaces, types, functions, schemas, and C++ namespaces use clear English technical meaning.

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
- Local assembly macros and internal layout constants use concise semantic names without redundant project branding.
- Do not encode codenames into public symbols, file paths, data schemas, or protocols.

Examples:

```cpp
namespace jixia::microkernel {}
namespace jixia::microkernel::trap {}
namespace jixia::microkernel::console {}
namespace jixia::platform::graph {}
namespace jixia::hypervisor::scheduler {}
namespace jixia::ras::diagnosis {}
namespace jixia::debug::replay {}
```

## 4. Architectural baseline

Jixia retains three long-term execution profiles:

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

## 5. CECSIM-style co-design rule

Jixia firmware and the full-system simulator are co-designed.

The simulator is an executable architecture specification and firmware-verification platform covering CPU/SoC execution, firmware state machines, LPARs, I/O, management interactions, topology mismatches, semantic fault injection, trace, checkpoint/replay, coverage, and invariant checking across functional, timing, cycle, and RTL modes.

Every major firmware interface must consider how the simulator observes it, synchronizes with it, injects failures, and verifies recovery.

## 6. Current implementation state

Completed:

- `M00-00`: RV64 QEMU virt reset entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap using `mtvec`, `mcause`, `mepc`, and `mtval`.
- `M00-02`: complete RV64 integer `TrapFrame`, shared assembly/C++ ABI, full save/restore path, known-register test, machine-checkable `TRAP_FRAME_TEST: PASS`.
- `M00-03`: recoverable software breakpoints; `EBREAK` and `C.EBREAK` are verified at `mepc`, saved `mepc` advances by the decoded 4/2-byte length, and both return through the common restore path and `mret`; dedicated QEMU regression passed on 2026-08-07.
- build artifacts renamed from `archfw.*` to `jixia.*`.
- executable implementation moved to semantic `microkernel/` paths.
- low-level C ABI enters freestanding C++ code under `jixia::microkernel`.
- architecture overview and persistent project/source records established.
- solo-development roadmap and progress-recording process established.

Current queue:

```text
ACTIVE  F00-01 Console foundation
PAUSED  M00-04 Timer interrupt
NEXT    M00-05 Per-hart state and stacks
NEXT    M00-06 Privilege transition foundation
```

### F00-01 Console foundation

Console is intentionally a standalone feature, not a subtask of the timer interrupt milestone.

The active branch introduces:

- `docs/JIXIA_CONSOLE_DESIGN.md`;
- `microkernel/console/` with a freestanding sink/router/stream model;
- a fixed 36 KiB memory ring sink;
- a QEMU polling-UART sink;
- normal and emergency routes;
- `console::out << ...` without `std::iostream`;
- a dedicated `scripts/test-console.sh` acceptance test;
- M00-02/M00-03 regressions on the same firmware image.

The timer implementation is preserved separately on `milestone/m00-04-timer-interrupt` at commit `299aff177497399236a848724b56c2e040ce4db4`. It resumes only after Console is validated and integrated, so Console and timer correctness remain independently reviewable.

Do not jump directly to Linux, migration, split-core, or memory encryption before the trap/privilege foundation is correct and testable.

## 7. Console/output architecture baseline

Console follows these long-term rules:

1. Formatting and transport are separate.
2. UART is one sink, not the Console architecture.
3. Memory history is a first-class sink.
4. Future screen, BMC/SOL, and Jingjie outputs are separate sinks/services.
5. `console::out` is a frontend, not the transport contract.
6. Human-readable Console text and structured RAS/event records share infrastructure but remain different semantic layers.
7. Raw polling output remains below Console for reset/pre-console failures.
8. Emergency output uses only explicitly panic-safe sinks.
9. Panic-safe paths must not depend on heap, scheduler, normal locks, or asynchronous completion.
10. Multi-hart Console/ring ownership is deferred until per-hart state exists.

Canonical design record: `docs/JIXIA_CONSOLE_DESIGN.md`.

## 8. Frozen implementation scope

The following remain long-term architecture topics but are not current implementation work:

- ArchHV and LPAR runtime;
- HS/VS execution and G-stage translation;
- virtual interrupt and virtual I/O;
- Service LPAR;
- confidential LPAR runtime;
- secure migration.

They remain frozen until the Jingjie simulator prerequisites in `docs/JIXIA_SOLO_ROADMAP.md` are satisfied.

## 9. New-conversation scan protocol

Before answering a Jixia/Firmware project question in a new chat:

1. inspect `Pedroaliu/Firmware`, its default branch, progress branch, and latest commits;
2. read this file;
3. read `docs/JIXIA_PROGRESS.md` to identify the single ACTIVE work item;
4. read `docs/JIXIA_SOLO_ROADMAP.md` for phase order and gates;
5. read `README.md`;
6. read `docs/JIXIA_ARCHITECTURE_V0.3.md` and `docs/JIXIA_PROJECT_SOURCES.md`;
7. inspect current source paths, namespaces, build files, and tests;
8. read relevant historical `ARCHFW_*` documents when useful;
9. locate referenced PDFs through the active conversation or File Library;
10. confirm the active simulator repository before editing it.

Repository state wins over remembered chat state unless the user explicitly says the repository is stale.

## 10. Confirmed repositories

### Primary

- `Pedroaliu/Firmware` — Jixia firmware platform.

### Related

- `Pedroaliu/RVSoC-Sim-v2` — related newer simulator work; confirm before treating it as active.
- `Pedroaliu/archlab_rvsoc_sim` — earlier simulator repository.
- `Pedroaliu/archlab-rvsoc-sim-t` — related timing experiment.
- `Pedroaliu/archlab-virt` — KVM/virtualization comparison project.
- `Pedroaliu/my-cs-arch-notes` — architecture notes.

### External source

- `open-power/hostboot`, reference branch `release-fw1120`.

The Firmware repository is never interchangeable with similarly named simulator repositories.

## 11. Decisions not to forget

- Jixia is not another EDK II implementation or mini-KVM.
- Native Linux/KVM remains a supported future profile and comparison baseline.
- LPAR is a logical-machine contract, not merely `vCPU + RAM`, but its implementation is deferred until simulator prerequisites exist.
- The driver domain uses Linux endpoint drivers directly; host firmware manages platform control and ownership.
- ACPI and DT are generated PlatformGraph views.
- Dynamic debug is cross-backend engineering infrastructure, not a production backdoor.
- Confidential computing constrains debug, DMA, RAS, attestation, and migration designs now, even though its runtime is deferred.
- BOOM and XiangShan are Core references; IBM POWER contributes partition/RAS/co-design ideas; CECSIM contributes the firmware-simulator verification method.
- Cultural codenames give the project identity; semantic English names and `jixia::*` namespaces keep the implementation globally readable.
- One active work item at a time is a deliberate learning and quality strategy.
- Console and timer interrupt are independent features and must have independent branches and acceptance evidence.

## 12. Maintenance

Update this file whenever naming policy, repositories, project direction, working mode, ACTIVE work item, feature gates, core sources, execution profiles, trust assumptions, Console/logging architecture, or learning/implementation workflow change.

Routine test results and feature/milestone-completion evidence belong in `docs/JIXIA_PROGRESS.md`.
