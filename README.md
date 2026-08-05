# 稷下 Jixia

**A RISC-V firmware-native logical partition, RAS, security, and full-system co-design project.**

> 稷下容百家，墨子立其规；女娲构其形，鲁班驭百器。

Jixia is a learning-driven server platform project. It studies a different systems path from the dominant `x86/Arm + Host Linux + KVM` model:

- the physical machine can launch multiple peer logical machines directly from firmware;
- partition identity, resource contracts, I/O ownership, RAS, attestation, and future confidential computing are designed as one platform model;
- complex device drivers can live in a Linux service domain instead of the microkernel or minimum hypervisor;
- the firmware and the full-system simulator grow together in a CECSIM-style workflow;
- native Linux/KVM remains a supported profile and a mainstream comparison baseline.

This is not an attempt to clone IBM PowerVM, EDK II, or KVM. It uses RISC-V to study why those systems put complexity in different places and what trade-offs follow.

## Canonical names

| Component | Role |
|---|---|
| **Jixia / 稷下** | Entire firmware-native platform |
| **Pangu / 盘古** | Future immutable Boot0 / first-instruction root |
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
  Pangu/Mozi -> native HS-mode Linux -> KVM guests

SINGLE_LPAR
  Pangu/Mozi -> ArchHV -> one VS-mode logical partition

MULTI_LPAR
  Pangu/Mozi -> ArchHV -> multiple peer logical partitions
```

A single LPAR is not automatically a normal KVM host. Native Linux must own HS-mode and the RISC-V H extension to run ordinary KVM guests; a Linux LPAR under ArchHV normally runs in VS-mode.

## Current state

Completed:

- `M00-00`: RV64 QEMU virt entry, hart filtering, `gp`, stack, BSS, UART.
- `M00-01`: minimal fatal M-mode trap path.
- canonical naming transition from ArchFW/kernel artifacts to Jixia/Mozi.

Next:

```text
M00-02  Complete TrapFrame
M00-03  Recoverable trap and mret
M00-04  Timer interrupt
M00-05  Per-hart state
M00-06  Privilege transition
```

## Build

Prerequisites:

- CMake 3.20 or newer
- `riscv64-unknown-elf-gcc` toolchain
- `qemu-system-riscv64`

```bash
cmake -S . -B build \
  -DCMAKE_TOOLCHAIN_FILE=cmake/riscv64-unknown-elf.cmake

cmake --build build
./scripts/run-qemu.sh
```

Generated artifacts include:

```text
build/jixia.elf
build/jixia.bin
build/jixia.map
build/jixia.dis
build/jixia.readelf
```

## Repository layout

```text
mozi/                   executable host firmware microkernel
pangu/                  future Boot0 contract
nuwa/                   PlatformGraph contract
archhv/                 firmware-native partition runtime contracts
services/luban/         Linux driver and boot service
personalities/yuange/   OS-facing firmware personalities
ras/bianque/            RAS diagnosis
recovery/taiyi/         recovery actions
debug/guigu/             dynamic debug and introspection
time/sunbin/             virtual time and migration
security/confidential/  confidential LPAR architecture
interfaces/jingjie/     firmware-simulator boundary

platform/qemu_virt/     current platform backend
linker/                 linker scripts
scripts/                build/run helpers
docs/                   architecture and source records
```

Placeholder component directories describe ownership and planned interfaces; they do not claim completed implementations.

## Read first

Every new project conversation or coding session should begin with:

1. [`PROJECT_CONTEXT.md`](PROJECT_CONTEXT.md)
2. [`docs/JIXIA_ARCHITECTURE_V0.2.md`](docs/JIXIA_ARCHITECTURE_V0.2.md)
3. [`docs/JIXIA_PROJECT_SOURCES.md`](docs/JIXIA_PROJECT_SOURCES.md)
4. current code and recent commits

Older `docs/ARCHFW_*` files are preserved as historical design records. Jixia/Mozi and the names above are canonical.
