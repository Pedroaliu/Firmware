# Jixia M00-07 Pre-DDR Memory Foundation

**Status:** DONE

**Branch:** `milestone/m00-07-memory-foundation`

**Reference model:** IBM Hostboot cache-contained firmware lifecycle, adapted to RISC-V/QEMU

**Accepted scope:** pre-DDR memory foundation plus a contained-to-mainstore mechanism prototype

**Accepted through:** `M00-07.04 mainstore transition mechanism prototype`

## 1. Milestone boundary

M00-07 answers one primary question:

> How can Jixia boot, allocate memory, construct page tables, and demand-page firmware from PNOR before normal DDR/mainstore is available?

The accepted path is:

```text
QEMU pflash / PNOR-equivalent image
        -> XIP Stage0
        -> FFS partition discovery
        -> resident JXBASE
        -> contained EarlyMemory domain
        -> 4 KiB PageManager
        -> Sv39 page tables from EarlyMemory
        -> JXEXT remains in pflash
        -> real instruction page fault
        -> FlashProvider fills an EarlyMemory page
        -> install RX PTE
        -> retry the faulting instruction
```

M00-07.04 additionally proves the software mechanism required for a future Hostboot-style exit-contained transition:

```text
DDR lifecycle prototype
        -> DDR ONLINE
        -> contained flush/castout point
        -> MAINSTORE semantic commit
        -> PageManager backing promotion
        -> remaining DDR registration
        -> allocator publication LAST
```

M00-07 intentionally stops here. It does **not** claim that the final Jixia boot flow already initializes DDR from this test path.

## 2. Architecture reference rule

Jixia firmware boot flow uses Hostboot as its primary reference.

```text
Hostboot
    -> IPL flow, InitService/istep, PNOR/VFS, memory initialization,
       exit-contained/mainstore, and RAS integration reference

seL4 and related microkernels
    -> protection, capability, address-space, service-isolation reference

NXP and similar firmware frameworks
    -> component manifest, dependency, versioning, package-boundary reference
```

Therefore M00-07.04's direct DDR sequence is a mechanism prototype only. A later milestone will first establish a Hostboot-style firmware service execution model and InitService/istep control flow; real host-driven DDR initialization and post-DDR paging will attach to that flow.

## 3. Primary invariant

The central M00-07 invariant is:

```text
firmware object/address identity
        !=
current backing/storage medium
```

For the QEMU semantic model:

```text
same firmware physical address
          |
          +-- CONTAINED -> EarlyMemory semantics
          |
          `-- MAINSTORE -> DDR semantics
```

No PEI-style global pointer fixup is required merely because the backing changes.

QEMU does not provide a POWER-style L3 backing-cache mode. M00-07 proves the software-visible lifecycle contract, not literal cache-line migration hardware.

A later Jingjie/real-platform backend may implement real SRAM/CAR/backing-cache castout and retirement while preserving the same address-identity contract.

## 4. PNOR image and FFS contract

The accepted image uses an OpenPOWER-compatible FFS v1 partition table rather than a private fixed Base/Extended header.

Current `pnor/qemu_virt.toml` model:

```text
0x00000000              BOOT0, fixed 4 KiB XIP Stage0
0x00001000              FFS TOC bootstrap location
0x00010000 and later    FFS-managed data area
                        JXBASE
                        optional JXEXT
...
0x02000000              end of 32 MiB pflash0
```

The important contract is partition identity, not a hard-coded JXBASE data offset:

```text
Stage0
    -> knows FFS TOC bootstrap location
    -> validates/reads FFS
    -> finds partition "JXBASE"
    -> copies only actual Base bytes
    -> preserves a0=hartid and a1=DTB
    -> jumps to Base
```

The resident FFS parser is reused by `FlashProvider` to locate `JXEXT` for demand paging.

Current partitions are readonly:

```text
BOOT0
JXBASE
JXEXT (optional, 4 KiB aligned/padded)
```

Read-side firmware paging is intentionally separate from future privileged persistent PNOR mutation.

## 5. EarlyMemory and PageManager

The software abstraction is `EarlyMemory`, not `L2` or `L3`.

```text
EarlyMemory
    current QEMU backend: semantic contained window

future possible backends:
    boot SRAM
    L2 cache-as-RAM
    L3 backing cache
    simulator-defined contained memory
```

QEMU v0 treats:

```text
contained = [0x80000000, 0x80800000)
```

as a semantic contained domain. QEMU physically implementing that range as RAM does not make DDR allocator-visible to Jixia.

A linker-reserved 64 KiB page-aligned bootstrap pool supplies 4 KiB PageManager pages.

Design rule:

> 4 KiB is the unit of ownership; larger pages are later translation optimizations.

The resident 8 MiB Sv39 mapping needs six 4 KiB page-table pages:

```text
1 x L2 root
1 x L1 table
4 x L0 tables
= 6 pages = 24 KiB
```

The 64 KiB pool therefore reports 10 pages remaining immediately after resident mappings are created, matching the M00-07.03 observation.

## 6. Pre-DDR Sv39 paging proof

M00-07.03 uses S-mode only as a synthetic lower-privilege fault context. It is **not** the final Jixia user-service execution model.

A real Sv39 instruction page fault is generated as follows:

```text
resident M-mode firmware
    -> builds Sv39 root/tables from contained PageManager
    -> installs resident mappings
    -> enters synthetic S-mode probe

S-mode JALR 0x40000000
    -> unmapped instruction fetch
    -> instruction page fault to M-mode

M-mode pager
    -> validates expected fault
    -> allocates contained EarlyMemory page
    -> finds JXEXT through FFS
    -> reads page from pflash
    -> installs Sv39 RX leaf PTE
    -> sfence.vma
    -> fence.i
    -> leaves saved mepc unchanged
    -> mret

hardware retries the same fetch
    -> pageable code executes
    -> returns expected result
```

A page fault repairs the execution environment and retries the original instruction; it does not skip the instruction by incrementing `mepc`.

## 7. DDR/mainstore mechanism prototype

M00-07.04 exposes explicit fake platform stages:

```text
DDR_OFFLINE
    -> DDR_DISCOVERED
    -> DDR_TRAINING
    -> DDR_TRAINED
    -> DDR_ADDRESS_MAP_READY
    -> DDR_DECODE_COMMITTED
    -> DDR_ONLINE
```

`build_topology()` remains an explicit operation even though it has no separate `DdrState` value.

`DDR_ONLINE` does **not** publish DDR allocation.

The transition mechanism is:

```text
CONTAINED + DDR ONLINE
    -> TRANSITIONING
    -> contained flush/castout hook
    -> MAINSTORE semantic commit
    -> PageManager promotes existing contained range metadata to DDR
       without moving base/next/end
    -> register remaining DDR range
    -> allocation is still forbidden
    -> enable_mainstore_allocation()
    -> normal DDR allocation becomes visible
```

Publication rule:

> Prepare everything first; publish availability last.

M00-07 distinguishes four independent facts:

```text
DDR hardware online
    !=
mainstore backing committed
    !=
PageManager metadata prepared
    !=
allocation published
```

This closes a real consistency hole found during review: the previous implementation published DDR allocation before PageManager had promoted the contained range and registered the remaining DDR range. A concurrent allocation could have returned stale `CONTAINED` metadata while `backing_for(PA)` already reported `DDR`.

Final allocator policy:

```text
CONTAINED                 -> contained allocation allowed
TRANSITIONING              -> allocation forbidden
MAINSTORE, gate closed     -> allocation forbidden
MAINSTORE, gate open       -> DDR allocation only
```

There is no hidden mainstore-to-contained fallback.

## 8. Accepted submilestones

### M00-07.01 — pflash Stage0 -> resident Base — DONE

Proves:

```text
[x] exact 32 MiB pflash image
[x] QEMU pflash0 + -bios none reset path
[x] XIP Stage0
[x] FFS TOC parsing
[x] JXBASE discovery by partition name
[x] Base copy to 0x80000000
[x] a0/a1 handoff preservation
[x] transfer into existing Mozi bootstrap
```

Test:

```bash
bash scripts/test-m00-07-01-pflash-stage0.sh
```

### M00-07.02 — explicit contained EarlyMemory — DONE

Test:

```bash
bash scripts/test-m00-07-02-contained-memory.sh
```

Core evidence:

```text
state       : CONTAINED
contained   : [0x80000000, 0x80800000)
ddr         : OFFLINE
ddr alloc   : disabled
M00_07_CONTAINED_MEMORY: PASS
```

### M00-07.03 — pre-DDR pflash-backed page fault — DONE

Test:

```bash
bash scripts/test-m00-07-03-pre-ddr-paging.sh
```

Core evidence:

```text
M00_07_PRE_DDR_PAGING_ARMED: PASS
M00_07_PRE_DDR_PAGE_FAULT: PASS
M00_07_PRE_DDR_FLASH_READ: PASS
M00_07_PRE_DDR_BACKING_EARLY: PASS
M00_07_PRE_DDR_PAGING_RESUME: PASS
M00-07.03 pre-DDR flash-backed paging: PASS
```

### M00-07.04 — fake DDR/mainstore transition mechanism — DONE

Test:

```bash
bash scripts/test-m00-07-04-mainstore-transition.sh
```

Core evidence:

```text
M00_07_DDR_ALLOCATOR_GATED: PASS
M00_07_DDR_DISCOVERED: PASS
M00_07_DDR_TRAINING: PASS
M00_07_DDR_TRAINED: PASS
M00_07_DDR_TOPOLOGY_READY: PASS
M00_07_DDR_ADDRESS_MAP_READY: PASS
M00_07_DDR_DECODE_COMMITTED: PASS
M00_07_DDR_ONLINE: PASS
M00_07_CONTAINED_FLUSH: PASS
M00_07_STABLE_ADDRESS: PASS
M00_07_MAINSTORE_ALLOCATOR_GATED: PASS
M00_07_MAINSTORE_TRANSITION: PASS
M00_07_MAINSTORE_EXTEND: PASS
M00-07.04 fake DDR lifecycle and mainstore transition: PASS
```

## 9. Closure evidence

GitHub Actions run `32005255564` passed the complete RV64 QEMU chain through M00-07.04, including formatting, build, M00-02 through M00-06 regressions, and all four M00-07 acceptance scripts.

## 10. Why there is no `EARLY_RETIRED` memory domain

An `early_retired` state was considered and removed before closure.

`CONTAINED`, `TRANSITIONING`, and `MAINSTORE` answer which memory domain is active. "Early memory retired" answers whether an old backend still has special ownership or hardware side effects.

In QEMU there is no additional operation after backing promotion and allocator publication. A label-only state adds no behavior.

If future hardware requires SRAM removal, CAR invalidation, cache-mode disable, capability revocation, or power gating, model that as a separate early-memory lifecycle with real side effects.

## 11. Deferred work: not M00-07.05

These are deliberately deferred rather than treated as unfinished M00-07 work:

```text
Hostboot kernel/user/VFS/InitService startup study
    -> define Jixia service execution model
    -> establish task/scheduler/address-space/IPC prerequisites
    -> execute Hostboot-style InitService/istep flow
    -> run host-owned DDR initialization/training/configuration
    -> establish exact exit-contained/MM_EXTEND-like ordering
    -> prove pre-DDR Sv39/page-table state survives that real transition
    -> perform natural post-DDR PNOR-backed faults into DDR
    -> validate real contained-cache retirement on Jingjie/hardware
```

The future post-DDR page fault should occur naturally while a real firmware service continues after DDR initialization, not because a synthetic S-mode test asks M-mode to initialize DDR through ECALL.

## 12. Management Complex boundary

M00-07 closure clarified the intended split:

```text
Boot Engine / minimum prerequisite management
    -> make host executable
    -> RoT, reset, minimum power/PLL/clock prerequisites

Host Jixia firmware
    -> make platform operational
    -> heavy HWP/istep work
    -> memory discovery/config/training/diagnostics
    -> address-map and mainstore orchestration

Management Complex
    -> keep platform manageable
    -> always-on/OOB runtime management
    -> RAS collection/monitoring
    -> telemetry/watchdog/recovery
    -> power/thermal supervision
    -> BMC communication
```

This avoids building a second Hostboot inside the Management Complex or paying for a large SRAM merely to host heavy initialization libraries that the host can demand-page from PNOR.

## 13. Definition of Done

M00-07 is complete when these four scripts pass together with the existing regression chain:

```bash
bash scripts/test-m00-07-01-pflash-stage0.sh
bash scripts/test-m00-07-02-contained-memory.sh
bash scripts/test-m00-07-03-pre-ddr-paging.sh
bash scripts/test-m00-07-04-mainstore-transition.sh
```

The durable result is the pre-DDR substrate needed to build Hostboot-style firmware services and later real DDR/mainstore flow without redesigning the boot image, allocator, VMM, or PNOR paging foundations.
