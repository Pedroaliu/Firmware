# 稷下 Jixia

**A RISC-V firmware-native logical partition, RAS, security, and full-system co-design project.**

> 稷下容百家，墨子立其规；女娲构其形，鲁班驭百器。

<p align="center">
  <a href="docs/images/jixia-firmware-architecture.svg">
    <img src="docs/images/jixia-firmware-architecture.svg" alt="Jixia firmware platform architecture overview" width="100%">
  </a>
</p>

<p align="center"><em>Jixia firmware platform overview — click the diagram to open the full-size vector image.</em></p>

Jixia is a learning-driven server platform project. It studies what a machine looks like when firmware, logical partitions, RAS, trusted/confidential computing, and a full-system simulator are designed together from the first instruction.

It is not an attempt to clone IBM PowerVM, EDK II, or KVM. Native Linux/KVM remains a supported execution profile and the mainstream comparison baseline.

## Naming policy

**Jixia / 稷下** is the public project and platform name.

Chinese cultural names are implementation codenames used in architecture diagrams, releases, presentations, and module descriptions. Source directories, public interfaces, types, functions, and C++ namespaces use clear English technical meaning.

Examples:

```text
Mozi / 墨子      -> host firmware microkernel -> microkernel/ -> jixia::microkernel
Pangu / 盘古     -> immutable Boot0           -> boot/        -> jixia::boot
Nuwa / 女娲      -> PlatformGraph              -> platform/model/
Luban / 鲁班     -> Linux driver domain       -> services/driver_domain/
Yuange / 元歌    -> firmware personalities    -> firmware_personality/
Guigu / 鬼谷     -> dynamic debug             -> debug/       -> jixia::debug
Jingjie / 镜界   -> full-system simulator     -> interfaces/simulator/
```

Low-level assembly and cross-language boundaries use a small stable C ABI with `jixia_` symbols. C++ implementation code uses nested namespaces:

```cpp
namespace jixia::microkernel {}
namespace jixia::platform::graph {}
namespace jixia::hypervisor::scheduler {}
namespace jixia::ras::diagnosis {}
namespace jixia::debug::replay {}
```

A contributor does not need to know the cultural references to navigate or extend the code.

## Architecture codenames

| Codename | Technical responsibility |
|---|---|
| **Jixia / 稷下** | Entire firmware-native platform |
| **Pangu / 盘古** | Immutable Boot0 / first-instruction root |
| **Mozi / 墨子** | Host firmware microkernel |
| **Nuwa / 女娲** | PlatformGraph and topology construction/repair |
| **ArchHV** | Firmware-native type-1 hypervisor |
| **Yixing / 弈星** | LPAR scheduling and placement |
| **Shouyue / 守约** | Entitlement and resource contracts |
| **Dunshan / 盾山** | Isolation, IOMMU, DMA, and ownership protection |
| **Luban / 鲁班** | Linux driver and boot service domain |
| **Yuange / 元歌** | UEFI/ACPI, SBI/DT, and U-Boot/FIT personalities |
| **Bianque / 扁鹊** | RAS diagnosis and FFDC correlation |
| **Taiyi / 太乙** | Recovery and degraded-mode actions |
| **Sunbin / 孙膑** | Virtual time and migration continuity |
| **Guigu / 鬼谷** | Dynamic debug, introspection, injection, and replay |
| **Jingjie / 镜界** | Full-system simulator and co-simulation world |

## Execution profiles

```text
NATIVE_HOST
  Boot0/microkernel -> native HS-mode Linux -> KVM guests

SINGLE_LPAR
  Boot0/microkernel -> ArchHV -> one VS-mode logical partition

MULTI_LPAR
  Boot0/microkernel -> ArchHV -> multiple peer logical partitions
```

A single LPAR is not automatically a normal KVM host. Native Linux must own HS-mode and the RISC-V H extension to run ordinary KVM guests; a Linux LPAR under ArchHV normally runs in VS-mode.

## Current state

Integrated on `main`:

- `M00-00`: RV64 QEMU virt entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap path.
- `M00-02`: complete RV64 TrapFrame and save/restore ABI.
- `M00-03`: recoverable 32-bit `EBREAK` and 16-bit `C.EBREAK` through `mret`.
- `M00-04`: recoverable machine timer interrupt.
- `F00-01`: shared freestanding formatter, 36 KiB KernelLogBuffer, and `printk` with a temporary raw-UART mirror.
- completed milestone chain integrated through PR #6.
- timer/console divergence resolved and integrated through PR #8.

Current development:

```text
ACTIVE  M00-05 Per-hart state, stacks, and SMP foundation
NEXT    M00-06 Privilege transition foundation
NEXT    M00-07 Early physical allocator
NEXT    M00-08 Structured event and trace ABI
```

The M00-05 branch is `milestone/m00-05-smp-foundation`.

Before SMP changes, keep the integrated single-hart foundation green:

```bash
bash scripts/test-kernel-print.sh
bash scripts/test-timer-interrupt.sh
```

Both scripts check the live Kernel Print, recoverable-trap, machine-timer, and TrapFrame markers.

## Console and observability

The minimum kernel diagnostic path is intentionally small:

```text
printk -> shared formatter -> KernelLogBuffer -> optional raw-UART mirror
```

The richer runtime Console Service is deferred until tasks, IPC, service lifecycle, allocator/runtime, and device ownership exist. Structured Trace and RAS events remain separate contracts from console text.

Design records:

- [`docs/JIXIA_CONSOLE_DESIGN.md`](docs/JIXIA_CONSOLE_DESIGN.md)
- [`docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md`](docs/JIXIA_CONCURRENCY_CORRECTNESS_RULES.md)
- [`docs/JIXIA_TRACE_OBSERVABILITY_VISION.md`](docs/JIXIA_TRACE_OBSERVABILITY_VISION.md)

## Build

Prerequisites:

- CMake 3.20 or newer
- Ninja
- `riscv64-unknown-elf-gcc` and `riscv64-unknown-elf-g++`
- `qemu-system-riscv64`

Preferred debug build:

```bash
cmake --preset jixia-rv64-debug
cmake --build --preset jixia-rv64-debug
```

Equivalent explicit configuration:

```bash
cmake -S . -B build/clion-debug \
  -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64-unknown-elf.cmake

cmake --build build/clion-debug --target jixia.elf
```

Generated artifacts include:

```text
build/clion-debug/jixia.elf
build/clion-debug/jixia.bin
build/clion-debug/jixia.map
build/clion-debug/jixia.dis
build/clion-debug/jixia.readelf
```

For a configured build directory, `./scripts/run-qemu.sh <build-dir>` runs the firmware image.

## Repository layout

```text
boot/                    Boot0 contract (codename Pangu)
microkernel/             executable host firmware microkernel (Mozi)
  arch/riscv/            trap/ISA architecture code
  console/               minimal Kernel Print path
  core/                  architecture-independent kernel policy/tests
lib/                     freestanding shared utilities
platform/model/          PlatformGraph and ownership model (Nuwa)
hypervisor/              firmware-native partition runtime (ArchHV)
services/driver_domain/  Linux driver and boot service (Luban)
firmware_personality/    OS-facing personalities (Yuange)
ras/diagnosis/           RAS diagnosis (Bianque)
ras/recovery/            recovery actions (Taiyi)
debug/                   dynamic debug and introspection (Guigu)
virtualization/time/     virtual time and migration (Sunbin)
security/confidential/   confidential LPAR architecture
interfaces/simulator/    firmware-simulator boundary (Jingjie)

platform/qemu_virt/      current physical-platform backend
linker/                  linker scripts
scripts/                 build/run/test helpers
docs/                    architecture, design, source, and progress records
```

Placeholder module directories define ownership and planned interfaces; they do not claim completed implementations.

## Read first

Every new project conversation or coding session begins with:

1. [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md)
2. [`docs/JIXIA_PROGRESS.md`](docs/JIXIA_PROGRESS.md)
3. [`docs/JIXIA_SOLO_ROADMAP.md`](docs/JIXIA_SOLO_ROADMAP.md)
4. [`docs/JIXIA_ARCHITECTURE_V0.3.md`](docs/JIXIA_ARCHITECTURE_V0.3.md)
5. [`docs/JIXIA_PROJECT_SOURCES.md`](docs/JIXIA_PROJECT_SOURCES.md)
6. current code, current progress branch, and recent commits

Older `docs/ARCHFW_*` and `JIXIA_ARCHITECTURE_V0.2.md` files remain historical design records. Jixia is canonical; cultural component names are codenames, while code uses semantic English names and `jixia::*` namespaces.
