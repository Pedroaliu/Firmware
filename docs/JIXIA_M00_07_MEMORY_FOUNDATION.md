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
    -> primary reference for IPL flow, InitService/istep, PNOR/VFS,
       memory initialization, exit-contained, and mainstore lifecycle

seL4 and related microkernels
    -> secondary reference for protection, capability, address-space,
       service isolation, and kernel/service mechanism design

NXP firmware frameworks
    -> secondary reference for component packaging, manifests,
       dependency declaration, and standardized component boundaries
```

Therefore M00-07.04's direct test-driven DDR sequence is a mechanism prototype only. A later milestone will first establish a Hostboot-style service execution model and InitService/istep control flow; real host-driven DDR initialization and post-DDR paging will be attached to that flow.

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

No PEI-style global pointer fixup is required merely because the memory backing changes.

QEMU does not provide a POWER-style L3 backing-cache mode. The implementation therefore proves the software-visible lifecycle contract, not literal cache-line migration hardware.

A future Jingjie/SimSoc or real-platform backend may implement:

```text
same PA
  |
  +-- contained mode -> boot SRAM / CAR / backing cache
  |
  `-- normal mode    -> DDR

DDR viable
-> quiesce host execution
-> route castout correctly
-> clean/cast out dirty contained state
-> disable contained mode
-> resume at the same firmware identities
```

## 4. PNOR image and FFS contract

M00-07 no longer uses a private fixed Base/Extended flash header. The accepted image uses an OpenPOWER-compatible FFS v1 partition table.

QEMU pflash0 is a 32 MiB image. The current manifest is `pnor/qemu_virt.toml`:

```text
0x00000000              BOOT0, fixed 4 KiB XIP Stage0
0x00001000              FFS TOC bootstrap location
0x00010000 and later    FFS-managed data area
                        JXBASE
                        optional JXEXT
...
0x02000000              end of pflash0
```

The important contract is identity, not a fixed JXBASE offset:

```text
Stage0
  knows the FFS TOC bootstrap location
        -> validates/reads FFS
        -> finds partition "JXBASE"
        -> copies only actual Base bytes
        -> preserves a0=hartid and a1=DTB
        -> jumps to Base entry
```

The resident firmware-side FFS parser is reused by `FlashProvider` to locate `JXEXT` for demand paging.

The partition model currently includes:

```text
BOOT0   readonly
JXBASE  readonly
JXEXT   readonly, optional, 4 KiB padded/aligned
```

This is intentionally compatible with the later architecture rule that pageable firmware reads are backing operations, while persistent PNOR mutation is a separate privileged transaction path.

## 5. Stage0 boundary

Stage0 is intentionally small. Its accepted responsibilities are:

```text
reset entry from QEMU pflash0
-> establish deterministic early execution state
-> inspect FFS
-> find JXBASE
-> copy resident Base to its linked address
-> preserve QEMU hart/DTB handoff
-> jump to Base
```

Stage0 does not own:

```text
DDR policy or training
memory grouping/interleave
NUMA policy
normal allocator policy
VMM policy
RAS diagnosis
InitService/istep orchestration
```

Those belong to later host firmware layers.

## 6. EarlyMemory and allocation model

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

as a semantic contained domain until the explicit lifecycle transition. The physical QEMU implementation being ordinary RAM does not make DDR allocator-visible to Jixia.

A linker-reserved 64 KiB, page-aligned bootstrap pool supplies the initial PageManager pages. Allocation/protection ownership uses 4 KiB pages.

Design rule:

> 4 KiB is the unit of ownership; larger pages are a later translation optimization.

The 8 MiB resident Sv39 mapping requires six 4 KiB page-table pages:

```text
1 x L2 root
1 x L1 table
4 x L0 tables
= 6 pages = 24 KiB
```

The 64 KiB bootstrap pool therefore has 10 pages remaining immediately after the resident mapping is built, which is observed by the M00-07.03 probe.

## 7. Pre-DDR Sv39 paging proof

M00-07.03 uses S-mode only as a synthetic lower-privilege fault context. It is **not** the final Jixia user-service execution model.

The reason for entering S-mode is architectural: normal M-mode instruction fetch does not use S-mode `satp` translation, so a real Sv39 instruction page fault must be generated from a lower privilege context.

Accepted sequence:

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
    -> allocates a contained EarlyMemory page
    -> locates JXEXT through FFS
    -> reads JXEXT page from pflash
    -> installs Sv39 RX leaf PTE
    -> sfence.vma
    -> fence.i
    -> leaves saved mepc unchanged
    -> mret

hardware retries the same instruction fetch
    -> pageable code executes
    -> returns expected result
```

A page fault repairs the execution environment and retries the original instruction; it does not skip the faulting instruction by incrementing `mepc`.

## 8. DDR/mainstore mechanism prototype

M00-07.04 exposes the following explicit fake platform stages:

```text
DDR_OFFLINE
    -> DDR_DISCOVERED
    -> DDR_TRAINING
    -> DDR_TRAINED
    -> DDR_ADDRESS_MAP_READY
    -> DDR_DECODE_COMMITTED
    -> DDR_ONLINE
```

`build_topology()` remains an explicit platform operation even though it does not currently add another `DdrState` value.

`DDR_ONLINE` means the memory path is viable enough for the transition model. It does **not** publish DDR allocation.

The contained-to-mainstore mechanism is:

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

The publication rule is:

> Prepare everything first; publish availability last.

The accepted transition deliberately separates four facts:

```text
DDR hardware online
    !=
mainstore backing committed
    !=
PageManager metadata prepared
    !=
allocation published
```

This closes a real race/consistency hole found during M00-07.04 review: the old implementation enabled DDR allocation before PageManager had promoted the contained range and registered the remaining DDR range. A concurrent allocation could then have returned stale `CONTAINED` allocator metadata while `backing_for(PA)` already reported `DDR`.

The final allocator policy is:

```text
CONTAINED
    -> allocate contained pages

TRANSITIONING
    -> allocation forbidden

MAINSTORE, allocator gate closed
    -> allocation forbidden

MAINSTORE, allocator gate open
    -> allocate DDR only
```

There is no hidden mainstore-to-contained fallback.

## 9. Why there is no `EARLY_RETIRED` memory domain

M00-07 initially considered an `early_retired` state. It was removed before closure.

`CONTAINED`, `TRANSITIONING`, and `MAINSTORE` answer:

> Which firmware-visible memory domain is active?

"Early memory retired" answers a different question:

> Does an old early-memory backend still have special ownership or hardware side effects?

In the current QEMU stable-address model there is no additional action after PageManager backing promotion and allocator publication. Adding another `MemoryDomain` value would only record a fact without changing behavior.

If a future real platform requires SRAM removal, CAR invalidation, backing-cache disable, capability revocation, or power gating, that lifecycle should be modeled separately, for example as an `EarlyMemoryState` with real side effects.

## 10. Accepted submilestones

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

Primary test:

```bash
bash scripts/test-m00-07-01-pflash-stage0.sh
```

### M00-07.02 — explicit contained EarlyMemory — DONE

Proves:

```text
[x] Base begins in CONTAINED
[x] contained range is explicit
[x] DDR begins OFFLINE
[x] DDR allocation begins disabled
[x] addresses outside the contained ownership range are unavailable to this memory lifecycle
```

Primary test:

```bash
bash scripts/test-m00-07-02-contained-memory.sh
```

Expected core evidence:

```text
state       : CONTAINED
contained   : [0x80000000, 0x80800000)
ddr         : OFFLINE
ddr alloc   : disabled
M00_07_CONTAINED_MEMORY: PASS
```

### M00-07.03 — pre-DDR pflash-backed page fault — DONE

Proves:

```text
[x] resident FFS/FlashProvider path
[x] 4 KiB PageManager bootstrap pool
[x] real Sv39 page tables from EarlyMemory
[x] JXEXT remains in pflash until fault
[x] real S-mode instruction page fault
[x] trusted M-mode TrapFrame and pager handling
[x] EarlyMemory page allocation
[x] pflash -> EarlyMemory fill
[x] RX PTE install and instruction-cache synchronization
[x] retry of original mepc
[x] pageable code successfully resumes
```

Primary test:

```bash
bash scripts/test-m00-07-03-pre-ddr-paging.sh
```

Expected core evidence:

```text
M00_07_PRE_DDR_PAGING_ARMED: PASS
M00_07_PRE_DDR_PAGE_FAULT: PASS
M00_07_PRE_DDR_FLASH_READ: PASS
M00_07_PRE_DDR_BACKING_EARLY: PASS
M00_07_PRE_DDR_PAGING_RESUME: PASS
M00-07.03 pre-DDR flash-backed paging: PASS
```

### M00-07.04 — fake DDR/mainstore transition mechanism — DONE

Proves:

```text
[x] named DDR discovery/training/topology/map/decode/online stages
[x] DDR stays allocator-invisible when hardware state first reaches ONLINE
[x] explicit contained flush/castout point
[x] stable live object address and contents across semantic transition
[x] existing PageManager range is promoted without moving addresses
[x] remaining DDR is registered only after mainstore semantic commit
[x] prepared-but-unpublished allocator window rejects allocation
[x] explicit allocator publication happens last
[x] post-publication allocation is DDR-backed
```

Primary test:

```bash
bash scripts/test-m00-07-04-mainstore-transition.sh
```

Expected core evidence:

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

## 11. Deferred work: not M00-07.05

The following work is intentionally deferred rather than treated as an unfinished M00-07.05:

```text
Hostboot kernel/user/VFS/InitService startup study
    -> determine Jixia service execution model
    -> establish real task/scheduler/address-space/IPC prerequisites
    -> execute Hostboot-style InitService/istep flow
    -> run host-owned DDR initialization/training/configuration
    -> establish exact exit-contained/MM_EXTEND-like ordering
    -> prove pre-DDR Sv39/page-table state survives that real transition
    -> perform a natural post-DDR PNOR-backed fault into DDR
    -> validate real contained-cache retirement on Jingjie/hardware
```

The post-DDR page fault should occur naturally while a real firmware service continues executing after DDR initialization, not because a synthetic page-fault test asks M-mode to initialize DDR through ECALL.

## 12. Management Complex boundary discovered during M00-07 closure

The future Management Complex is not a second Hostboot.

A useful responsibility split is:

```text
Boot Engine / minimal management prerequisite
    -> make the host safely executable
    -> minimum power/PLL/reset/security prerequisites

Host Jixia firmware
    -> make the platform operational
    -> heavy HWP/istep execution
    -> memory discovery/configuration/training/diagnostics
    -> address-map and mainstore orchestration

Management Complex
    -> keep the platform manageable
    -> always-on/OOB runtime management
    -> RAS event aggregation and monitoring
    -> watchdog/recovery coordination
    -> telemetry, thermal/power supervision, BMC communication
```

This avoids paying for a large Management Complex SRAM and software environment merely to host memory-training code that the host can demand-page from PNOR while still in contained mode.

## 13. Definition of Done

M00-07 is complete when all four acceptance scripts are green together with the existing M00-02 through M00-06 regression chain:

```bash
bash scripts/test-m00-07-01-pflash-stage0.sh
bash scripts/test-m00-07-02-contained-memory.sh
bash scripts/test-m00-07-03-pre-ddr-paging.sh
bash scripts/test-m00-07-04-mainstore-transition.sh
```

The milestone does not require a production DDR PHY implementation, final user-service model, or real cache-contained hardware.

The durable result of M00-07 is the pre-DDR substrate needed to build those later layers without redesigning the boot image, allocator, VMM, or PNOR paging foundations.
