# 稷下 Jixia

**A RISC-V firmware-native server platform, RAS, security, and full-system co-design project.**

> 稷下容百家，墨子立其规；女娲构其形，鲁班驭百器。

<p align="center">
  <a href="docs/images/jixia-firmware-architecture.svg">
    <img src="docs/images/jixia-firmware-architecture.svg" alt="Jixia firmware platform architecture overview" width="100%">
  </a>
</p>

Jixia is a learning-driven server firmware/platform project. It studies what a machine looks like when boot firmware, a firmware microkernel, logical partitions, RAS, confidential computing, management-plane behavior, and a full-system simulator are designed together from the first instruction.

It is not an attempt to clone IBM PowerVM, Hostboot, EDK II, seL4, or KVM. Existing systems are used as architectural references with explicit responsibility boundaries.

## Architecture reference policy

For firmware lifecycle and boot-flow questions, Jixia uses the following priority:

```text
Hostboot whole-system firmware flow
    -> Jixia platform requirements
    -> seL4 protection/capability mechanisms
    -> NXP component/package mechanisms
    -> Linux/other implementation comparisons
```

Hostboot is the primary reference for:

```text
kernel/bootstrap flow
VMM and PNOR/VFS/resource providers
user/service startup
InitService and istep orchestration
HWP execution
memory initialization
cache-contained -> mainstore transition
RAS integration patterns
```

seL4 is mainly a protection/isolation reference. NXP-style firmware frameworks are mainly component manifest, dependency, and packaging references.

## Naming policy

**Jixia / 稷下** is the public project and platform name.

Chinese cultural names are implementation codenames used in architecture diagrams and module descriptions. Source directories, public interfaces, types, functions, schemas, and C++ namespaces use clear English technical meaning.

| Codename | Technical responsibility |
|---|---|
| Jixia / 稷下 | entire firmware-native platform |
| Pangu / 盘古 | immutable Boot0 / first-instruction root |
| Mozi / 墨子 | host firmware microkernel |
| Nuwa / 女娲 | PlatformGraph and topology |
| ArchHV | firmware-native type-1 hypervisor |
| Yixing / 弈星 | LPAR scheduling/placement |
| Shouyue / 守约 | resource contracts/accounting |
| Dunshan / 盾山 | isolation, IOMMU, DMA ownership |
| Luban / 鲁班 | Linux driver/boot service domain |
| Yuange / 元歌 | firmware personalities |
| Bianque / 扁鹊 | RAS diagnosis |
| Taiyi / 太乙 | recovery |
| Sunbin / 孙膑 | virtual time/migration continuity |
| Guigu / 鬼谷 | dynamic debug/introspection/replay |
| Jingjie / 镜界 | full-system simulator |

Low-level assembly and cross-language boundaries use a small stable C ABI with `jixia_` symbols. C++ implementation uses semantic namespaces such as:

```cpp
namespace jixia::microkernel {}
namespace jixia::platform {}
namespace jixia::hypervisor {}
namespace jixia::ras::diagnosis {}
namespace jixia::debug {}
```

## Host and Management Complex boundary

The Management Complex is not intended to become a second Hostboot.

```text
Boot Engine / minimum prerequisite logic
    -> root-of-trust and secure-load prerequisites
    -> minimum power/PLL/clock/reset work
    -> make the host safely executable

Host Jixia firmware
    -> make the platform operational
    -> microkernel and firmware services
    -> InitService / istep / HWP execution
    -> heavy DDR configuration/training/diagnostics
    -> memory topology/interleave/address map
    -> PCIe/CXL and later platform initialization

Management Complex
    -> keep the platform manageable
    -> always-on runtime/OOB control
    -> RAS event collection and monitoring
    -> telemetry
    -> watchdog/recovery coordination
    -> power/thermal supervision
    -> BMC communication
```

Pre-DDR host code is not limited to a tiny static blob: Jixia's M00-07 foundation allows a resident Base to demand-page Extended firmware from PNOR into contained EarlyMemory, following the same high-level reason Hostboot can execute a rich pre-mainstore software environment.

## Execution profiles

```text
NATIVE_HOST
  Boot0/microkernel -> native HS-mode Linux -> KVM guests

SINGLE_LPAR
  Boot0/microkernel -> ArchHV -> one VS-mode logical partition

MULTI_LPAR
  Boot0/microkernel -> ArchHV -> multiple peer logical partitions
```

The final RISC-V M/S/U placement for firmware kernel and services is not yet frozen. Existing M00-06/M00-07 S-mode code is an acceptance context, not a production user-service model.

## Current state

Accepted foundation:

```text
DONE  M00-00  RV64 QEMU boot, stack, BSS, UART
DONE  M00-01  minimal fatal M trap
DONE  M00-02  complete integer TrapFrame
DONE  M00-03  recoverable EBREAK/C.EBREAK
DONE  M00-04  machine timer interrupt
DONE  F00-01  Kernel Print
DONE  M00-05  per-hart state, private stacks, SMP foundation
DONE  M00-06  privilege transition foundation
DONE  M00-07  Pre-DDR Memory Foundation
```

M00-07 establishes:

```text
32 MiB pflash / PNOR-equivalent image
    -> OpenPOWER-compatible FFS
    -> XIP Stage0
    -> FFS-discovered JXBASE
    -> resident Base
    -> explicit contained EarlyMemory
    -> 4 KiB PageManager
    -> Sv39 page tables before DDR
    -> JXEXT remains pageable in pflash
    -> real instruction page fault
    -> pflash -> EarlyMemory fill
    -> retry original instruction
```

M00-07.04 also proves a fake DDR/mainstore mechanism prototype with stable firmware addresses and a prepare-before-publish allocator gate. It does **not** claim the final DDR boot flow is complete.

Full M00-07 code-closure regression: GitHub Actions run `32005255564` — SUCCESS.

Design record:

- [`docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`](docs/JIXIA_M00_07_MEMORY_FOUNDATION.md)

## Immediate next step

The next step is an architecture research gate, not another synthetic memory probe.

Study the Hostboot startup chain:

```text
Base/kernel entry
    -> task/scheduler
    -> VMM
    -> VFS / PNOR Resource Provider
    -> first user/service task
    -> InitService
    -> istep dispatch
    -> HWP invocation
    -> memory isteps
    -> proc_exit_cache_contained
    -> MM_EXTEND_REAL_MEMORY / mainstore extension
```

Only after this flow is understood will Jixia freeze the next implementation milestone for firmware services/InitService and later return to production host-driven DDR initialization, real exit-contained, and post-DDR PNOR paging.

See:

- [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md)
- [`docs/JIXIA_PROGRESS.md`](docs/JIXIA_PROGRESS.md)
- [`docs/JIXIA_SOLO_ROADMAP.md`](docs/JIXIA_SOLO_ROADMAP.md)

## M00-07 memory model

The key invariant is:

```text
firmware object/address identity
        !=
current backing/storage medium
```

QEMU uses semantic contained memory; it does not pretend to provide a POWER-style backing-cache mode.

The M00-07.04 transition prototype separates:

```text
DDR hardware online
    !=
mainstore backing committed
    !=
PageManager metadata ready
    !=
allocation published
```

Allocator availability is published last.

The old label-only `EARLY_RETIRED` memory domain was removed. Real SRAM/CAR/backing-cache retirement will later be modeled only when there are actual hardware/resource side effects to perform.

## PNOR direction

The image uses FFS partitions rather than fixed private offsets:

```text
BOOT0   readonly XIP bootstrap
JXBASE  readonly resident Base
JXEXT   readonly optional pageable Extended content
```

Read-side firmware paging is distinct from persistent mutation:

> Read is a pageable backing operation; write is a privileged persistent transaction.

Ordinary CPU stores must never implicitly write firmware storage. Firmware update, VPD, GUARD, configuration, and persistent RAS records will use explicit authorized services/capabilities.

## Console and observability

The minimum kernel diagnostic path remains intentionally small:

```text
printk -> shared formatter -> KernelLogBuffer -> optional raw-UART mirror
```

Structured Trace, structured RAS events, and richer runtime Console Services are separate contracts.

Design records:

- [`docs/JIXIA_CONSOLE_DESIGN.md`](docs/JIXIA_CONSOLE_DESIGN.md)
- [`docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`](docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md)
- [`docs/JIXIA_TRACE_OBSERVABILITY_VISION.md`](docs/JIXIA_TRACE_OBSERVABILITY_VISION.md)

## RAS direction

Jixia keeps Power-style deterministic diagnostic rules as the trusted spine and studies AI-era extensions around them:

```text
Structured Event
    + PlatformGraph
    + deterministic PRD-style rules
    + Machine Health Journal / Case Memory
    + optional AI hypothesis/rule discovery
    + safe HWP active probes
    + Jingjie replay/validation
    -> deterministic, auditable recovery policy
```

The Management Complex is expected to be important for host-independent RAS collection, telemetry, monitoring, watchdog, and OOB recovery coordination.

## Quick start

For Debian/Ubuntu/Deepin/UOS development hosts:

```bash
git clone <repository-url>
cd Firmware

bash scripts/setup-dev-env.sh
bash scripts/jixia.sh run
```

Check an existing host without modifying it:

```bash
bash scripts/setup-dev-env.sh --check
```

Build only:

```bash
bash scripts/jixia.sh build
```

Run with a chosen hart population:

```bash
bash scripts/jixia.sh run --smp 1
bash scripts/jixia.sh run --smp 2
bash scripts/jixia.sh run --smp 4
```

Debug through QEMU GDB server:

```bash
bash scripts/jixia.sh debug --smp 4
```

Milestone-specific tests are authoritative. M00-07 acceptance commands are:

```bash
bash scripts/test-m00-07-01-pflash-stage0.sh
bash scripts/test-m00-07-02-contained-memory.sh
bash scripts/test-m00-07-03-pre-ddr-paging.sh
bash scripts/test-m00-07-04-mainstore-transition.sh
```

Detailed workflow:

- [`docs/JIXIA_DEVELOPER_WORKFLOW.md`](docs/JIXIA_DEVELOPER_WORKFLOW.md)

## Repository layout

```text
boot/                    Boot0 contract / Stage0
microkernel/             host firmware microkernel
  arch/riscv/            trap/ISA/Sv39 architecture code
  console/               minimum Kernel Print path
  core/                  kernel mechanisms and acceptance probes
  memory/                lifecycle/PageManager/FlashProvider
  firmware_store/        FFS/runtime firmware-store parsing
platform/qemu_virt/      current physical-platform backend
pnor/                    PNOR image manifests
linker/                  linker scripts
scripts/                 environment/build/run/debug/test helpers
tools/pnor/              FFS/PNOR host-side tooling
platform/model/          future PlatformGraph model
hypervisor/              future firmware-native partition runtime
services/                future firmware/driver service domains
ras/                     diagnosis/recovery architecture
debug/                   dynamic debug/introspection
interfaces/simulator/    firmware-simulator boundary
docs/                    architecture, design, sources, progress
```

Placeholder directories define ownership and planned interfaces; they do not claim completed implementations.

## Read first

Every new project conversation or coding session begins with:

1. [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md)
2. [`docs/JIXIA_PROGRESS.md`](docs/JIXIA_PROGRESS.md)
3. [`docs/JIXIA_SOLO_ROADMAP.md`](docs/JIXIA_SOLO_ROADMAP.md)
4. [`docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`](docs/JIXIA_M00_07_MEMORY_FOUNDATION.md)
5. relevant architecture/RAS records
6. current code, branch, recent commits, and CI evidence

Repository state wins over remembered chat state whenever they differ.
