# Jixia Memory Management Research Notes

Status: research checkpoint, 2026-08-12

This document records the memory-management reasoning, Hostboot source-study progress, and the design constraints that should survive future conversation/context loss. It is deliberately **not** a final M00-07 design. The current rule is: get a correct Hostboot-inspired v0 running, learn from experiments, then refactor with better ideas from UEFI, seL4, Linux, U-Boot/coreboot, NXP firmware, and modern memory-debugging systems.

## 1. First-principles problem statement

Jixia does not merely need an allocator. The firmware must answer several independent questions:

1. What occupies the system physical address space?
   - system RAM / DDR
   - MMIO
   - firmware-reserved ranges
   - holes / unavailable ranges
   - future persistent/CXL/other resources
2. Which RAM ranges are currently usable by firmware allocators?
3. Who owns each allocation or mapping?
4. Which virtual-address ranges may each microkernel service access?
5. How are virtual pages backed, faulted in, protected, unmapped, and reclaimed?
6. Which memory survives boot handoff as runtime/reserved memory, and which memory becomes reclaimable/payload-owned?
7. Can memory corruption be detected at the **first illegal access**, rather than only when a later victim crashes?
8. Can the temporary boot-memory domain be retired cleanly after DDR becomes available?

These are separate architectural problems. Do not collapse them into a single `MemoryManager` or a single `type` enum.

## 2. System resource map is separate from the allocator

The machine-wide physical address-space database and the RAM allocator must remain conceptually separate.

### 2.1 Resource-map role

Reference idea: UEFI DXE GCD.

A Jixia resource database should eventually be able to represent and dynamically add/remove/query resources such as:

- SystemMemory
- MMIO
- Reserved
- holes / nonexistent address space
- attributes such as cacheability, ownership/source, NUMA/proximity domain, etc.

This database answers **what the address range is**, not whether a 4 KiB page inside RAM is currently allocated by a service.

### 2.2 Allocation role

Physical allocators consume only eligible SystemMemory ranges. Allocation state is an overlay on the resource map, not the definition of the resource map.

Conceptually:

```text
SystemResourceMap
  [System RAM................................]

Allocation state
  [kernel][stack][free....][page table][free]
```

Do not split the global resource database into `allocated/free/allocated/free` descriptors on every page allocation merely to represent allocator state.

### 2.3 Handoff/reporting is another view

UEFI GCD-like resource classification, EFI memory-map lifecycle types, Device Tree reporting, Hostboot/HDAT reserved-memory reporting, and future LPAR views are not the same schema.

Jixia should eventually maintain its own internal truth and **project** it into the requested handoff ABI rather than storing internal state as UEFI-specific types.

## 3. Boot memory is temporary: CAR/bootstrap -> DDR/main memory

Jixia should **not** remain cache-contained/CAR-based for normal execution.

Temporary memory is a bootstrap survival environment. DDR/main memory is the normal microkernel execution environment.

Reference paths:

- UEFI: temporary RAM/CAR -> permanent DRAM
- Hostboot: cache-contained -> `p10_exit_cache_contained` -> `mm_extend()` -> mainstore

### 3.1 Hostboot source evidence already studied

`src/usr/isteps/istep14/call_proc_exit_cache_contained.C`

- validates functional memory
- executes the exit-cache-contained HWP
- calls `mm_extend()` on the master
- after success explicitly operates from main storage

`src/kernel/basesegment.C`, `BaseSegment::_mmExtend()`

- extends the VMM from cache-contained memory into mainstore
- creates an extended block
- reserves memory for ShadowPTE metadata
- passes the remaining pages to `PageManager::addMemory()`

This is an important architectural lesson: entering DDR is a **memory-domain transition**, not simply changing one allocator pointer.

### 3.2 Likely Jixia lifecycle

The exact implementation remains open, but the architecture must permit:

```text
Bootstrap memory domain
  CAR / SRAM / temporary RAM
        |
        v
simple bootstrap allocator
        |
DDR discovery/training complete
        |
        v
main-memory page allocator
        |
move/rebuild long-lived state
page tables / stacks / kernel state / service state
        |
        v
switch to normal DDR-backed execution
        |
        v
retire/poison bootstrap memory
```

A bump/monotonic allocator is sufficient for very early bootstrap. The long-running DDR allocator may be completely different.

Long term, stable virtual addresses may allow a page backing to move from CAR to DDR without repairing arbitrary C++ pointers, but this must be proven experimentally rather than assumed.

## 4. Hostboot kernel memory-management model learned so far

Hostboot has been useful because it is itself a custom microkernel and separates physical allocation, virtual address ownership, software page state, hardware translation state, and user-space backing providers.

The main path studied is:

```text
exception
  -> VmmManager
  -> SegmentManager
  -> BaseSegment / StackSegment / DeviceSegment
  -> Block
  -> ShadowPTE
       |             |
       v             v
   PageManager   Resource Provider
       \             /
        -> PageTableManager -> POWER HPT
```

### 4.1 `PageManager`

Role: physical backing-page allocator.

Important lessons already extracted:

- allocator operates on one or more explicit managed ranges
- allocation/free and split/coalesce are allocator concerns, not system topology concerns
- multiple ranges matter because coalescing must never cross holes/reserved regions
- Hostboot maintains a critical/kernel reserve pool
- memory-pressure behavior is layered on top of the allocator
- allocator state exposes debugging counters/pointers

Hostboot is a strong first implementation reference, but its exact algorithm does not need to become permanent Jixia policy.

### 4.2 `ShadowPTE`

`src/include/kernel/spte.H`

The in-kernel software page-state record contains, among other things:

- physical page number
- present
- read/write/execute permissions
- write tracking
- dirty state
- allocate-from-zero
- 3-bit aging/LRU state

Important historical limitation: the current Hostboot structure uses a 20-bit PFN and therefore reflects its original physical-memory constraints. The **idea** is useful; the exact format is historical baggage.

### 4.3 `PageTableManager` and POWER HPT

`src/kernel/ptmgr.C`, `src/include/kernel/ptmgr.H`

The POWER HPT is a hardware-visible translation working set, not the complete authoritative software page database.

Important observations:

- 256 KiB HPT
- 16-byte PTEs
- groups of 8 entries (PTEGs)
- VA hashes to a PTEG
- when a group is full, a hardware translation entry can be stolen/replaced
- HPT eviction does **not** imply freeing the backing physical page
- HPT has a 2-bit replacement/aging mechanism distinct from the ShadowPTE 3-bit page-aging state
- hardware R/reference information is harvested and propagated back into the higher-level VMM
- modifying hardware translation state requires strict synchronization/TLB invalidation

For RISC-V/Sv39, the hardware mechanism will be completely different, but the architectural separation between authoritative software state and hardware translation state remains valuable.

### 4.4 `VmmManager`

`src/kernel/vmmmgr.C`

Role: global VMM coordinator and synchronization boundary.

Studied behavior:

- initializes Base/Stack/Device segments
- initializes hardware page-table machinery
- page faults enter `VmmManager::pteMiss()`
- VMM lock protects the fault/mapping path
- routes the fault to `SegmentManager`
- exposes virtual-to-physical lookup, mapping, permissions, page removal, extension, device map/unmap, etc.

### 4.5 `SegmentManager`

`src/kernel/segmentmgr.C`

Role: virtual-address-space router.

It determines which Segment owns a faulting VA and delegates handling. It does not itself allocate backing RAM.

### 4.6 `BaseSegment`

`src/kernel/basesegment.C`

Important lessons:

- contains the initial base block
- initializes firmware text/data permissions
- creates additional VA blocks via `mmAllocBlock()`
- chains Blocks rather than embedding every region into one global page structure
- performs the cache-contained -> mainstore VMM extension in `_mmExtend()`
- adds only eligible remaining mainstore pages to `PageManager`

### 4.7 `StackSegment`

`src/kernel/stacksegment.C`

Very useful design example:

- each task gets a virtual stack region
- pages are marked allocate-from-zero
- backing physical pages are allocated on page fault, not all at stack creation
- stack placement is deliberately distributed to reduce hashed-page-table collisions
- guard/protection space is intentionally reserved around stacks

The exact HPT-collision concern is POWER-specific, but lazy physical backing and deliberate guard-space design are broadly useful.

### 4.8 `DeviceSegment`

`src/kernel/devicesegment.C`

Important lesson: MMIO mapping is VMM work, not RAM allocation.

A device VA fault maps an existing real device range into the hardware page table with appropriate cache-inhibited/guarded attributes. `PageManager` is not involved.

### 4.9 `Block`

`src/kernel/block.C`

Role: concrete VA-region state and page-fault worker.

Already studied behavior includes:

- containment/range routing
- one ShadowPTE per page in a Block
- permission checks
- lazy physical page allocation
- zero-fill anonymous-style backing
- page-table entry installation
- resource-provider read/write messaging
- page removal/release
- reference/LRU tracking
- real backing-page eviction under memory pressure

### 4.10 Resource Providers: microkernel pager/backend idea

`src/usr/pnor/pnorrp.C` studied as a concrete example.

PnorRP:

- creates a message queue
- registers a virtual-memory Block with that queue
- runs a user-space daemon waiting for memory messages
- on page fault, kernel VMM can allocate a physical page and ask the provider to fill it
- PNOR-specific details such as flash access/ECC remain outside the kernel VMM

This is one of Hostboot's most valuable ideas for Jixia: kernel provides mechanism; user-space service/pager provides complex backing policy.

## 5. Hostboot platform-memory discovery and handoff study status

This portion is **not yet considered fully complete**. Current overall Hostboot memory study is roughly 75-80% through the intended scope.

### 5.1 Confirmed so far

`src/usr/isteps/mem_utils.C`

Hostboot does not ask PageManager to discover system RAM. It queries functional processor targets and reads attributes such as:

- `ATTR_PROC_MEM_BASES`
- `ATTR_PROC_MEM_SIZES`
- mirrored/SMF-related ranges

This supports the separation:

```text
platform/Targeting memory knowledge
    !=
physical page allocator
```

### 5.2 HDAT Main Store work already inspected

`src/usr/hdat/hdatmsvpd.H` and related source paths.

The Main Store VPD structures carry platform/system-description data such as:

- maximum configured mainstore address
- total configured mainstore size
- memory areas and RAM/DIMM information
- installed/functional/shared state
- UE ranges
- Hostboot reserved-memory ranges

The Hostboot reserved-memory descriptor includes range type/id, start/end, label, and permissions.

This is system/handoff description, not allocator state.

### 5.3 Still to finish

The remaining source chain to study before declaring the Hostboot memory research complete is:

```text
MSS grouping / hardware memory setup
 -> Targeting attributes
 -> exact construction of system memory topology/ranges
 -> HDAT Main Store / reserved memory
 -> runtime memory ownership/lifetime
 -> Skiboot hdata parser
 -> final Device Tree memory/reserved-memory/NUMA view
 -> Linux payload
```

Do not fill gaps in this chain from memory or assumptions; follow the actual Hostboot/Skiboot source when needed.

## 6. Jixia microkernel/service isolation requirements

Jixia is not a single-address-space UEFI implementation. Service isolation means virtual memory is architectural, not optional decoration.

Long-term concerns include:

- per-service address spaces
- page-table ownership
- VA/PA ownership
- R/W/X permissions
- shared IPC pages
- service stacks/heaps
- MMIO authorization
- DMA/IOMMU ownership
- controlled pager/resource-provider behavior

The exact mechanism can evolve, but the first implementation must not make later isolation impossible.

Hostboot's `VmmManager -> Segment -> Block -> page state` structure is a useful first reference. seL4 VSpace/capability ideas and Linux MM can later be used to improve isolation and scalability.

## 7. Memory debugging and observability are first-class requirements

Do not implement the allocator/VMM first and add debugging only after corruption appears.

The target principle is:

> Stop at the first illegal access, not at the eventual victim crash.

Useful capabilities to grow incrementally:

### Early/basic

- range-overlap and overflow validation
- allocation IDs
- owner / purpose / source provenance
- PA reverse lookup
- allocation journal
- poison patterns
- redzones where practical
- allocator/VMM invariant checker

### With page tables

- guard pages
- unmap/no-access after free where practical
- stack guards
- mapping consistency checks
- stale/bootstrap-memory access detection

### Later allocator/runtime

- double-free detection
- quarantine before reuse
- ownership transitions
- mapping leak detection
- RAS quarantine/poison state

### Jingjie simulator

Maintain shadow state for physical memory and check CPU/DMA accesses against states such as:

- unavailable
- free
- allocated
- redzone
- freed
- MMIO
- firmware-reserved
- device-owned/DMA-owned
- service/LPAR ownership
- quarantined
- retired bootstrap memory

The simulator should be able to stop on the exact faulting PC/access rather than waiting for downstream corruption.

## 8. Fast boot and memory lifecycle

Prior Eagle Stream firmware work exposed several practical boot-time issues:

- limited CAR code/data capacity
- rearranging FDF/layout improved locality modestly
- larger gains likely come from latency hiding rather than only layout tuning
- DDR training can dominate boot time
- inter-socket fabric/KTI-style training can also dominate

Jixia should eventually treat boot as a dependency/resource graph rather than a strictly serial list of init calls.

Long-term direction:

- start long-latency hardware training and do independent work while it runs
- overlap DDR/fabric training, verification, decompression, topology work, etc. when safe
- use AP/service workers rather than keeping all secondary cores idle
- explicit dependency barriers only where truly required
- stage/prefetch future firmware data, using real DMA where available or worker cores where useful
- trace each boot task's start/end/CPU/dependencies/wait reason
- optimize the **critical path**, not individual function runtimes in isolation

Two useful design principles:

1. Software should not be idle while independent boot work exists.
2. Boot time should approach the hardware critical path rather than the sum of all hardware latencies.

Do not implement aggressive minimum-viable-boot/hot-add ideas in the first version; preserve resource-map flexibility so they can be explored later.

## 9. Reference-selection philosophy

Do not make Jixia a clone of one project.

Choose references according to the problem:

- system address-resource management: UEFI GCD, plus lessons from coreboot/U-Boot/Hostboot platform description
- physical allocation: Hostboot PageManager, Linux memblock/buddy, U-Boot LMB, seL4 Untyped ownership ideas
- VMM/page faults: Hostboot VMM, Linux MM, seL4 VSpace/fault model
- service isolation: seL4 + Hostboot Resource Provider concepts
- runtime/reserved/handoff: UEFI Runtime Memory, Hostboot HDAT/reserved memory, Skiboot Device Tree
- debugging: ASan/KASAN/KFENCE/guard pages/poison/quarantine plus Jingjie shadow-memory instrumentation

For every borrowed mechanism ask:

1. What problem was it solving?
2. Which invariant does it preserve?
3. Which part is general architecture?
4. Which part is hardware/project/history-specific baggage?
5. Does Jixia actually have the same constraint?

## 10. Implementation policy for M00-07 and later

Current policy agreed on 2026-08-12:

- enough research has been done to begin a first implementation when M00-07 starts
- do **not** over-design the final memory subsystem before experiments
- a Hostboot-inspired first version is acceptable
- direct reuse/reference is acceptable when licensing/source attribution is preserved
- prefer understanding and re-implementing the state machine/invariants in RISC-V rather than mechanically copying POWER-specific mechanisms
- if v0 exposes bad abstractions, refactor; system software is expected to evolve
- advanced NUMA/CXL/LPAR/hotplug/RAS features should not be guessed into v0 unless required by an immediate milestone

The desired progression is:

```text
v0: smallest correct mechanism
 -> experiments expose invariants/problems
 -> v1: strengthen correctness/isolation/debuggability
 -> real SMP/service workloads expose scalability limits
 -> v2+: NUMA/RAS/runtime/hotplug/advanced ownership as justified
```

## 11. Source-study checkpoint

Hostboot files already studied or inspected in this research pass:

- `src/kernel/README.md`
- `src/kernel/pagemgr.C` / corresponding declarations previously provided/studied
- `src/kernel/vmmmgr.C`
- `src/kernel/segmentmgr.C`
- `src/kernel/basesegment.C`
- `src/kernel/stacksegment.C`
- `src/kernel/devicesegment.C`
- `src/kernel/block.C`
- `src/kernel/ptmgr.C`
- `src/include/kernel/ptmgr.H`
- `src/include/kernel/spte.H`
- `src/kernel/exception.C`
- `src/usr/pnor/pnorrp.C`
- `src/usr/isteps/mem_utils.C`
- `src/usr/isteps/istep14/call_proc_exit_cache_contained.C`
- `src/import/chips/p10/procedures/hwp/nest/p10_exit_cache_contained.C` (partially inspected for the exit-cache-contained hardware flow)
- `src/usr/hdat/hdatmsvpd.H` and related HDAT Main Store paths (partial)
- Hostboot runtime reserved-memory population code (partial)
- Skiboot architecture/hdata-to-device-tree overview (partial)

Next research target when this topic resumes:

1. complete MSS/Targeting memory-range provenance
2. complete HDAT main-store and Hostboot reserved/runtime-memory construction
3. trace the exact Skiboot `hdata -> Device Tree -> Linux` path
4. then compare the resulting full Hostboot lifecycle against EDK II GCD / DXE Memory Services / EFI MemoryMap / Runtime Memory / ExitBootServices
5. only then refine the Jixia resource-map/handoff abstractions beyond the first runnable version

## 12. One-sentence checkpoint

Jixia should start from a simple Hostboot-inspired microkernel memory implementation, keep system-resource description separate from RAM allocation, explicitly transition from temporary bootstrap memory to DDR, use page tables for real service isolation, build debugging/guarding in from the beginning, and evolve the design through experiments rather than trying to make M00-07 final on day one.
