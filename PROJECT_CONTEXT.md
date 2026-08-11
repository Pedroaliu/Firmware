# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
>
> In a new conversation, scan the repository and read this file before relying on conversational memory.

## 1. Canonical identity

- **Project/platform name:** 稷下 / **Jixia**
- **Primary repository:** `Pedroaliu/Firmware`
- **Stable integration branch:** `main`
- **Current progress branch:** `milestone/m00-06-privilege-transition`
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

Completed milestones merge promptly into `main`; new milestone branches start from the latest integrated checkpoint.

### Learning/implementation workflow

- ChatGPT may provide and commit complete reference implementations for syntax-heavy or repetitive scaffolding.
- The developer is expected to understand architectural state transitions, invariants, failure modes, and debugging evidence.
- New mechanisms are taught through complete reference code first, then explanation, guided modification, and progressively larger independent implementation tasks.
- Debugging should prefer GDB, CSR/register state, disassembly, QEMU logs, and machine-checkable tests over guessing.

## 3. Naming policy

Jixia is the project and product brand.

Chinese cultural names are implementation codenames, not source-code vocabulary. Source directories, public interfaces, types, functions, schemas, and C++ namespaces use clear English technical meaning.

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

Core principles:

1. Platform model first.
2. The microkernel owns minimum trusted mechanisms, not every feature.
3. Global orchestration belongs to host firmware; local agents contain local faults.
4. Complex physical device drivers do not belong in the minimum hypervisor.
5. Linux endpoint drivers live in a driver service domain.
6. UEFI/ACPI/DT/U-Boot personalities are projections of one filtered PlatformGraph.
7. Resource ownership has one authoritative manager.
8. Debug/replay and fault injection are first-class architecture features.
9. Protection, detection, and recovery are designed separately.
10. Firmware and the full-system simulator are co-designed.

## 5. CECSIM-style co-design rule

Jixia firmware and Jingjie are co-designed.

The simulator is an executable architecture specification and firmware-verification platform covering CPU/SoC execution, firmware state machines, LPARs, I/O, management interactions, topology mismatches, semantic fault injection, trace, checkpoint/replay, coverage, and invariant checking.

Every major firmware interface must consider how the simulator observes it, synchronizes with it, injects failures, and verifies recovery.

## 6. Current implementation state

Integrated on `main`:

- `M00-00`: RV64 QEMU virt reset entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap using `mtvec`, `mcause`, `mepc`, and `mtval`.
- `M00-02`: complete RV64 integer `TrapFrame`, shared assembly/C++ ABI, full save/restore path.
- `M00-03`: recoverable 32-bit `EBREAK` and 16-bit `C.EBREAK` through common restore + `mret`.
- `M00-04`: recoverable machine timer interrupt.
- `F00-01`: Kernel Print foundation.
- `M00-05`: per-hart state, private stacks, dense HartIndex, boot rendezvous, FDT population discovery, per-hart timer state/compare, and SMP acceptance for 1/2/4 harts with controlled over-capacity rejection.
- developer workflow helpers: `scripts/setup-dev-env.sh`, `scripts/jixia.sh`, and `docs/JIXIA_DEVELOPER_WORKFLOW.md`.
- AI-era RAS architecture/research records under `docs/`.

Current queue:

```text
ACTIVE  M00-06 Privilege transition foundation
NEXT    M00-07 Early physical allocator
NEXT    M00-08 Structured event and trace ABI
NEXT    M00-09 Automated QEMU test harness
```

### M00-05 accepted invariants

- private per-hart stack before C/C++;
- only boot hart owns BSS/global initialization;
- explicit release/acquire publication;
- atomic slot allocation for uniqueness;
- `HartId` is architectural identity, `HartIndex` is a dense software slot;
- physical topology belongs to PlatformGraph;
- `HartLocal` is per-hart state and `mscratch` points to the current `HartLocal`;
- per-hart/single-writer ownership is preferred to global locks;
- only boot hart is a normal `printk` writer during the milestone.

Design record: `docs/JIXIA_M00_05_SMP_FOUNDATION.md`.

### M00-06 design boundary

M00-06 introduces the first controlled lower-privilege execution path.

Initial target:

```text
M-mode Mozi
    -> configure mstatus.MPP = S
    -> configure mepc
    -> mret
    -> S-mode payload with satp = 0
    -> controlled ecall/trap back to M-mode
```

The core new invariant is trap-stack trust: after S-mode controls its own `sp`, M-mode trap entry must not blindly use that lower-privilege stack as trusted kernel storage.

M00-06 should use `mscratch -> HartLocal` to locate trusted per-hart kernel/trap state and preserve the interrupted lower-privilege stack pointer in the saved context.

Do not mix privilege transition with paging, allocator, scheduler, service IPC, or PMP isolation in the first proof.

## 7. Console / observability boundary

Minimum kernel diagnostics remain:

```text
printk
   |
shared formatter
   |
KernelLogBuffer
   |
   `---- temporary UART mirror
```

Console text, structured Trace, and structured RAS records are separate contracts.

## 8. RAS direction

Jixia RAS keeps Power-style deterministic diagnostic rules as the trusted spine and adds AI-era capabilities around that spine:

- Structured Event ABI;
- PlatformGraph/topology correlation;
- Incident Graph;
- Machine Health Journal;
- Case Memory;
- hypothesis-driven diagnosis and safe HWP probes;
- Fleet rule mining;
- Jingjie replay/counterfactual verification;
- deterministic, auditable recovery policy.

See:

- `docs/JIXIA_RAS_ARCHITECTURE.md`
- `docs/JIXIA_RAS_REASONING_VISION.md`
- `docs/JIXIA_AI_RAS_ARCHITECTURE_SUMMARY.md`

## 9. Frozen implementation scope

The following remain long-term architecture topics but are not current implementation work:

- ArchHV and LPAR runtime;
- HS/VS execution and G-stage translation;
- virtual interrupt and virtual I/O;
- Service LPAR;
- confidential LPAR runtime;
- secure migration.

They remain frozen until the Jingjie simulator prerequisites in `docs/JIXIA_SOLO_ROADMAP.md` are satisfied.

## 10. Developer workflow

Normal local entry points:

```bash
bash scripts/setup-dev-env.sh --check
bash scripts/jixia.sh env
bash scripts/jixia.sh build
bash scripts/jixia.sh run --smp 4
bash scripts/jixia.sh debug --smp 4
```

Detailed workflow: `docs/JIXIA_DEVELOPER_WORKFLOW.md`.

Milestone acceptance scripts remain the source of truth for pass/fail; generic `jixia.sh run` does not replace machine-checkable gates.

## 11. New-conversation scan protocol

Before answering a Jixia/Firmware project question in a new chat:

1. inspect `Pedroaliu/Firmware`, `main`, the current progress branch, and recent commits;
2. read this file;
3. read `docs/JIXIA_PROGRESS.md`;
4. read `docs/JIXIA_SOLO_ROADMAP.md`;
5. read `README.md`;
6. read `docs/JIXIA_ARCHITECTURE_V0.3.md` and relevant design records;
7. inspect current source paths, namespaces, build files, and tests;
8. locate referenced PDFs through the active conversation or File Library;
9. confirm the active simulator repository before editing it.

Repository state wins over remembered chat state unless the user explicitly says the repository is stale.

## 12. Confirmed repositories

### Primary

- `Pedroaliu/Firmware` — Jixia firmware platform.

### Related

- `Pedroaliu/RVSoC-Sim-v2` — related newer simulator work; confirm before treating it as active.
- `Pedroaliu/archlab_rvsoc_sim` — earlier simulator repository.
- `Pedroaliu/archlab-rvsoc-sim-t` — related timing experiment.
- `Pedroaliu/archlab-virt` — KVM/virtualization comparison project.
- `Pedroaliu/my-cs-arch-notes` — architecture notes.

### External source

- `open-power/hostboot` — IBM/OpenPOWER host firmware source reference.

## 13. Decisions not to forget

- Jixia is not another EDK II implementation or mini-KVM.
- Native Linux/KVM remains a supported future profile and comparison baseline.
- LPAR is a logical-machine contract, not merely `vCPU + RAM`, but its implementation is deferred until simulator prerequisites exist.
- The driver domain uses Linux endpoint drivers directly; host firmware manages platform control and ownership.
- ACPI and DT are generated PlatformGraph views.
- Kernel Print and the future Console Service are separate failure/runtime domains.
- Trace and RAS remain structured interfaces rather than `printk` text.
- Dynamic debug is cross-backend engineering infrastructure, not a production backdoor.
- For concurrency, prefer ownership/per-hart partitioning before shared locks.
- One active milestone at a time is a deliberate learning and quality strategy.

## 14. Maintenance

Update this file whenever naming policy, repositories, project direction, working mode, ACTIVE milestone, feature gates, core sources, execution profiles, trust assumptions, or learning/implementation workflow change.

Routine test results and milestone-completion evidence belong in `docs/JIXIA_PROGRESS.md`.
