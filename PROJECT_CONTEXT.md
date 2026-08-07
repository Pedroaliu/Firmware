# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
>
> In a new conversation, scan the repository and read this file before relying on conversational memory.

## 1. Canonical identity

- **Project/platform name:** 稷下 / **Jixia**
- **Primary repository:** `Pedroaliu/Firmware`
- **Stable integration branch:** `main`
- **Current progress branch:** `feature/console-foundation`
- **Current work item:** `F00-01 Kernel print foundation`
- **Parked timer branch:** `milestone/m00-04-timer-interrupt` at `299aff177497399236a848724b56c2e040ce4db4`
- **Project type:** RISC-V firmware-native server platform research project
- **Purpose:** learning, architecture exploration, and executable system research

Jixia studies firmware, logical partitions, RAS, trusted/confidential computing, and a full-system simulator as one co-designed machine stack.

## 2. Development mode

The project currently has one human developer working with ChatGPT as a research, teaching, architecture, review, debugging, and implementation partner.

Execution policy:

```text
NOW      exactly one primary feature/milestone
NEXT     ordered work only
BACKLOG  accepted later work
FROZEN   work blocked by prerequisites
```

Canonical execution plan: `docs/JIXIA_SOLO_ROADMAP.md`.
Canonical live status: `docs/JIXIA_PROGRESS.md`.

A work item is not DONE merely because code exists. Completion requires test evidence plus recorded design/learning conclusions.

### Learning/implementation workflow

- ChatGPT may provide and commit complete syntax-heavy, repetitive, or foundational code.
- The developer must understand core architectural state transitions, invariants, failure modes, and debugging evidence.
- New mechanisms use: complete reference -> explanation -> guided modification -> increasingly independent implementation.
- Debugging prefers GDB/CSR/disassembly/QEMU/test evidence over guessing.
- Do not turn every C++ syntax detail into a blocking exercise.
- Independent facilities use independent branches and acceptance evidence.

## 3. Naming and ABI policy

Jixia is the project/product name. Chinese cultural names are architecture codenames, while source paths, public interfaces, types, functions, schemas, and namespaces use clear English technical meaning.

Key codenames:

| Codename | Responsibility | Semantic code |
|---|---|---|
| Pangu | immutable Boot0 | `boot/`, `jixia::boot` |
| Mozi | host firmware microkernel | `microkernel/`, `jixia::microkernel` |
| Nuwa | PlatformGraph/topology | `platform/model/`, `jixia::platform` |
| ArchHV | type-1 firmware hypervisor | `hypervisor/`, `jixia::hypervisor` |
| Luban | Linux driver/boot service | `services/driver_domain/` |
| Bianque | RAS diagnosis | `ras/diagnosis/` |
| Taiyi | recovery | `ras/recovery/` |
| Guigu | debug/introspection | `debug/` |
| Jingjie | full-system simulator | `interfaces/simulator/` |

Rules:

- minimum reset/assembly boundaries remain C-compatible;
- cross-language symbols use stable `jixia_` C ABI names;
- freestanding implementation uses C++ nested namespaces;
- local assembly/internal layout names avoid redundant project prefixes;
- codenames do not leak into public ABI/protocol/schema names.

## 4. Architectural baseline

Long-term profiles:

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
8. Partition identity eventually spans translation, interrupts, IOMMU, counters, trace, energy, and RAS.
9. Debug/replay and fault injection are first-class.
10. Protection, detection, and recovery are designed separately.
11. Normal, measured, and confidential LPARs share one lifecycle model with different trust assumptions.

## 5. Simulator co-design rule

Jixia firmware and Jingjie are co-designed.

The simulator is an executable architecture specification and firmware-verification platform covering CPU/SoC execution, firmware state machines, LPARs, I/O, management interactions, topology mismatch, semantic fault injection, trace, checkpoint/replay, coverage, and invariant checking across functional/timing/cycle/RTL modes.

Every major firmware interface should consider how the simulator observes, synchronizes, injects failures, and verifies recovery.

## 6. Current implementation state

Completed:

- `M00-00`: RV64 QEMU virt reset entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap.
- `M00-02`: complete RV64 integer `TrapFrame`; `TRAP_FRAME_TEST: PASS`.
- `M00-03`: recoverable 32-bit `EBREAK` and 16-bit `C.EBREAK`; dedicated QEMU regression passed on 2026-08-07.
- freestanding C++ compatibility and semantic source/namespace cleanup.
- solo roadmap and persistent progress records.

Current queue:

```text
ACTIVE  F00-01 Kernel print foundation
PAUSED  M00-04 Timer interrupt
NEXT    M00-05 Per-hart state and stacks
NEXT    M00-06 Privilege transition foundation
```

### F00-01 Kernel print foundation

Current design deliberately implements only the Mozi kernel diagnostic path:

```text
printk
   |
shared freestanding formatter
   |
36 KiB append-only KernelLogBuffer
   |
   +---- temporary QEMU raw-UART mirror
```

Key files:

```text
lib/format.{h,cpp}
microkernel/console/kernel_console.{h,cpp}
microkernel/console/printk.{h,cpp}
microkernel/core/kernel_print_test.cpp
scripts/test-kernel-print.sh
```

The candidate sources were compiled with a Clang RV64 bare-metal target under `-Wall -Wextra -Werror`; final acceptance requires the user's GNU `riscv64-unknown-elf` toolchain and QEMU.

### Console decision that must not be reopened from scratch

Kernel print and future usr/service Console are separate failure domains.

Current kernel scope:

```text
printk
shared formatter
append-only fixed kernel log
temporary raw UART mirror
raw UART primitive below formatted output
```

Deferred until a real service/user runtime exists:

```text
queue/daemon Console Service
logical DEFAULT/DEBUG channels
runtime UART device objects
screen/framebuffer
BMC/SOL
Jingjie console transport
console::out / cout-like frontend
service-level flush barrier
```

Console text is not the structured RAS/trace ABI.

The complete accepted architecture, Hostboot kernel/usr study, supplied `src.zip` library review, and future TODOs are in:

`docs/JIXIA_CONSOLE_DESIGN.md`

**Future sessions must read that file before redesigning Console.**

### Timer

Timer work is preserved independently on:

```text
milestone/m00-04-timer-interrupt
299aff177497399236a848724b56c2e040ce4db4
```

Resume it only after F00-01 acceptance.

## 7. Frozen implementation scope

Not current code work:

- ArchHV and LPAR runtime;
- HS/VS and G-stage translation;
- virtual interrupt and virtual I/O;
- Service LPAR;
- confidential LPAR runtime;
- secure migration.

These remain behind gates in `docs/JIXIA_SOLO_ROADMAP.md`.

## 8. New-conversation scan protocol

Before answering a Jixia/Firmware project question in a new chat:

1. inspect `Pedroaliu/Firmware`, its progress branch, and latest commits;
2. read this file;
3. read `docs/JIXIA_PROGRESS.md`;
4. if Console/Kernel Print is relevant, read `docs/JIXIA_CONSOLE_DESIGN.md`;
5. read `docs/JIXIA_SOLO_ROADMAP.md`;
6. inspect current source/build/tests;
7. use relevant architecture/source documents when needed;
8. locate referenced PDFs through the active conversation or File Library;
9. confirm the active simulator repository before editing simulator code.

Repository state wins over remembered conversation state unless the user explicitly says the repository is stale.

## 9. Confirmed repositories

Primary:

- `Pedroaliu/Firmware`

Related:

- `Pedroaliu/RVSoC-Sim-v2`
- `Pedroaliu/archlab_rvsoc_sim`
- `Pedroaliu/archlab-rvsoc-sim-t`
- `Pedroaliu/archlab-virt`
- `Pedroaliu/my-cs-arch-notes`

External reference:

- `open-power/hostboot`, branch `release-fw1120`

The Firmware repository is never interchangeable with similarly named simulator repositories.

## 10. Decisions not to forget

- Jixia is not another EDK II implementation or mini-KVM.
- Native Linux/KVM remains a supported future profile and comparison baseline.
- LPAR is a logical-machine contract, not merely `vCPU + RAM`.
- The driver domain uses Linux endpoint drivers directly; host firmware manages platform control/ownership.
- ACPI and DT are generated PlatformGraph views.
- Dynamic debug is engineering infrastructure, not a production backdoor.
- Confidential computing constrains debug, DMA, RAS, attestation, and migration design even before runtime implementation.
- IBM POWER contributes partition/RAS/co-design ideas; CECSIM contributes firmware-simulator verification ideas.
- One active work item at a time is deliberate.
- Kernel print and usr Console Service are separate.
- Formatting is reusable and transport-independent.
- Kernel log storage is authoritative; UART is currently only a bring-up mirror.
- The current kernel log is append-only; runtime ring/trace/RAS storage is a separate decision.
- Hostboot is a design/source reference; do not import its libc/runtime wholesale.

## 11. Maintenance

Update this file when identity, branches, ACTIVE work, architecture direction, feature gates, core sources, trust assumptions, Console/logging design, or learning workflow changes.

Routine test evidence belongs in `docs/JIXIA_PROGRESS.md`.
