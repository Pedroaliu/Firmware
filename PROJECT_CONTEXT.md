# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
>
> In a new conversation, scan the repository and read this file before relying on conversational memory.

## 1. Canonical identity

- **Project/platform name:** 稷下 / **Jixia**
- **Primary repository:** `Pedroaliu/Firmware`
- **Stable integration branch:** `main`
- **Current progress branch:** `milestone/m00-05-smp-foundation`
- **Project type:** RISC-V firmware-native server platform research project
- **Purpose:** learning, architecture exploration, and executable system research—not a short path to a commercial UEFI/KVM clone

Jixia studies what a machine looks like when firmware, logical partitions, RAS, trusted/confidential computing, and a full-system simulator are designed together from the first instruction.

IBM POWER/PowerVM/LPAR and System z/CECSIM are studied as alternative systems perspectives to the dominant x86/Arm + Linux/KVM path. The goal is better trade-off reasoning, not a claim of universal superiority.

## 2. Development mode

The project currently has one human developer working with ChatGPT as a research, teaching, architecture, review, debugging, and implementation partner.

The project is intentionally single-threaded:

```text
NOW      exactly one primary milestone
NEXT     at most three ordered milestones
BACKLOG  accepted later work
FROZEN   work blocked by architectural prerequisites
```

The canonical execution plan is `docs/JIXIA_SOLO_ROADMAP.md`. The canonical live status is `docs/JIXIA_PROGRESS.md`.

When a milestone is completed, the progress ledger records:

- commit or pull request;
- test command and result;
- what was learned;
- design decisions;
- known limitations;
- documentation changes;
- the next ACTIVE milestone.

A milestone is not complete merely because code exists.

### Learning/implementation workflow

The current teaching workflow deliberately separates syntax fluency from systems reasoning:

- ChatGPT may provide and commit complete reference implementations for syntax-heavy or repetitive scaffolding.
- The developer is expected to understand architectural state transitions, invariants, failure modes, and debugging evidence.
- New mechanisms are taught through complete reference code first, then explanation, guided modification, and progressively larger independent implementation tasks.
- Debugging should prefer observable evidence such as GDB, CSR/register state, disassembly, QEMU logs, and tests over guessing.
- Do not turn every syntax detail into a blocking exercise.

### Branch/integration rule

Completed milestones and accepted foundational features are merged into `main` promptly. New major work starts from the latest integrated baseline.

Do not build a long chain of completed milestone branches while leaving `main` stale.

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

Integrated on `main`:

- `M00-00`: RV64 QEMU virt reset entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap using `mtvec`, `mcause`, `mepc`, and `mtval`.
- `M00-02`: complete RV64 integer `TrapFrame`, shared assembly/C++ ABI, full save/restore path, known-register test, `TRAP_FRAME_TEST: PASS`.
- `M00-03`: recoverable 32-bit `EBREAK` and 16-bit `C.EBREAK`, both returning through the common TrapFrame restore + `mret` path.
- `M00-04`: recoverable machine timer interrupt; asynchronous return preserves saved `mepc`.
- `F00-01`: Kernel Print foundation: shared freestanding formatter, 36 KiB append-only KernelLogBuffer, `printk`, and temporary raw-UART mirror.
- M00-02 through M00-04 integrated by PR #6.
- Console/timer conflict-resolved integration merged by PR #8.
- Canonical console, concurrency-correctness, and trace-observability design records are present under `docs/`.

Current queue:

```text
ACTIVE  M00-05 Per-hart state, stacks, and SMP foundation
NEXT    M00-06 Privilege transition foundation
NEXT    M00-07 Early physical allocator
NEXT    M00-08 Structured event and trace ABI
```

### M00-05 design boundary

M00-05 is the first multi-hart foundation milestone. It must establish private per-hart stacks/state and a correct boot-hart/secondary-hart rendezvous before allowing secondary harts into normal C++ execution.

Do not infer socket/core/NUMA topology from arithmetic on `hart_id`. Hart identity is architectural state; physical topology belongs to PlatformGraph.

Before adding shared synchronization, prefer per-hart ownership and document the RISC-V/C++ memory-order contract. See `docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`.

Kernel Print is intentionally not yet claimed to be multi-writer safe. M00-05 must define its concurrency policy before secondary harts use normal `printk` concurrently.

## 7. Console / observability boundary

The minimum kernel diagnostic path is:

```text
printk
   |
shared formatter
   |
KernelLogBuffer
   |
   `---- temporary UART mirror
```

The future runtime Console Service is a separate execution/failure domain and remains deferred until tasks, IPC, service lifecycle, allocator/runtime, and device ownership exist.

Console text, structured Trace, and structured RAS records are separate contracts. See:

- `docs/JIXIA_CONSOLE_DESIGN.md`
- `docs/JIXIA_TRACE_OBSERVABILITY_VISION.md`

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
3. read `docs/JIXIA_PROGRESS.md` to identify the single ACTIVE milestone;
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
- Kernel Print and the future Console Service are separate failure/runtime domains.
- Trace and RAS must remain structured interfaces rather than being collapsed into `printk` text.
- Dynamic debug is cross-backend engineering infrastructure, not a production backdoor.
- Confidential computing constrains debug, DMA, RAS, attestation, and migration designs now, even though its runtime is deferred.
- BOOM and XiangShan are Core references; IBM POWER contributes partition/RAS/co-design ideas; CECSIM contributes the firmware-simulator verification method.
- Cultural codenames give the project identity; semantic English names and `jixia::*` namespaces keep the implementation globally readable.
- For concurrency, prefer ownership/per-hart partitioning before introducing shared locks; original algorithms require explicit correctness and memory-order arguments.
- One active milestone at a time is a deliberate learning and quality strategy.

## 12. Maintenance

Update this file whenever naming policy, repositories, project direction, working mode, ACTIVE milestone, feature gates, core sources, execution profiles, trust assumptions, or learning/implementation workflow change.

Routine test results and milestone-completion evidence belong in `docs/JIXIA_PROGRESS.md`.
