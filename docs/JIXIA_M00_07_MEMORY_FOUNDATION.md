# Jixia M00-07 Memory Foundation

**Status:** ACTIVE
**Branch:** `milestone/m00-07-memory-foundation`
**Reference model:** Hostboot cache-contained -> mainstore lifecycle, adapted to RISC-V/QEMU first and later to Jingjie/SimSoc hardware models
**Accepted through:** `M00-07.03 pre-DDR flash-backed page fault`

## 1. Objective

M00-07 is not an allocator milestone. It establishes the first end-to-end firmware memory lifecycle:

```text
QEMU pflash / PNOR-equivalent
        -> tiny XIP Stage0
        -> resident Jixia Base image
        -> contained EarlyMemory domain
        -> VMM/PageManager usable before DDR
        -> flash-backed pre-DDR page fault
        -> explicit DDR discovery/training/layout/decode lifecycle
        -> DDR online
        -> contained -> mainstore transition
        -> flash-backed post-DDR page fault
        -> retire EarlyMemory
```

The first implementation deliberately keeps DDR training and hardware decode as stubs while making the lifecycle and state transitions real and machine-checkable.

## 2. Architectural invariant

The primary M00-07 invariant is:

```text
firmware object/address identity
        !=
current backing/storage medium
```

Jixia must not require UEFI/PEI-style global pointer fixups merely because the early-memory backing changes.

The desired model is:

```text
same firmware VA/PA identity
          |
          +-- pre-DDR  -> contained EarlyMemory backing
          |
          `-- post-DDR -> normal DDR backing
```

On a future SimSoc implementation with a real cache-contained mode, the strongest form is:

```text
same PA
  |
  +-- contained mode -> L2/L3/cache backing
  |
  `-- normal mode    -> DDR backing

transition:
DDR decode valid
-> stop host execution
-> route castout to memory
-> flush/purge contained cache
-> disable contained mode
-> resume host execution
```

The QEMU model is behavioral. It must preserve the same software contract without pretending QEMU provides POWER-style cache backing hardware.

## 3. QEMU virt facts used by M00-07

Current upstream QEMU `virt` provides two CFI pflash windows in a 64 MiB region beginning at `0x20000000`; each pflash is 32 MiB. DRAM begins at `0x80000000`.

Upstream source references:

```text
QEMU hw/riscv/virt.c
  VIRT_FLASH = 0x20000000, size 0x04000000
  VIRT_DRAM  = 0x80000000

QEMU docs/system/riscv/virt.rst
  pflash images must be exactly 32 MiB
  TCG may boot M-mode firmware from pflash0 with -bios none

QEMU hw/riscv/virt.c::virt_machine_done()
  pflash0 + -bios none + TCG changes reset jump target to VIRT_FLASH.base

QEMU hw/riscv/boot.c::riscv_setup_rom_reset_vec()
  reset ROM supplies:
      a0 = mhartid
      a1 = generated FDT load address
  then jumps to the selected start address
```

Therefore the first M00-07 boot path can execute Stage0 directly from pflash while preserving the existing Jixia `(a0=hartid, a1=dtb)` handoff contract.

## 4. Firmware image model

M00-07 introduces a small explicit flash image contract instead of treating the complete firmware as an opaque RAM payload.

Initial 32 MiB pflash0 layout:

```text
0x00000000  Stage0 XIP code
0x00001000  JixiaFlashHeader v1
0x00010000  Jixia Base image
0x00100000  Extended/pageable image
...         future additional sections
0x02000000  end of pflash0
```

`JixiaFlashHeader v1` is 64 bytes:

```text
0x00 u64 magic
0x08 u32 version
0x0c u32 header_size
0x10 u64 base_offset
0x18 u64 base_size
0x20 u64 base_load_address
0x28 u64 base_entry
0x30 u64 extended_offset
0x38 u64 extended_size
```

M00-07.03 begins using the Extended fields for a real pflash-resident pageable code page.

## 5. Stage0 boundary

M00-07.01 Stage0 is intentionally smaller than the future complete Pangu/Boot Engine design.

It owns only:

```text
reset entry from QEMU mask ROM
-> disable asynchronous M-mode interruption
-> validate minimal flash header
-> copy resident Base image from pflash to contained EarlyMemory address
-> preserve a0=hartid and a1=FDT
-> jump to Base entry
```

It does not own:

```text
DDR policy
memory grouping/interleave
NUMA policy
allocator policy
VMM policy
RAS policy
secure boot/recovery/A-B update yet
```

The first pflash acceptance is deliberately single-hart. SMP release policy belongs to the later Boot Foundation/management-complex work and must not be smuggled into the memory milestone.

## 6. EarlyMemory model

Do not name the software abstraction `L2` or `L3`.

```text
EarlyMemory
  backend = SIMULATED_CONTAINED on QEMU v0

future backends:
  BOOT_SRAM
  L2_CAR
  L3_BACKING_CACHE
  simulator-defined contained memory
```

For QEMU v0, the resident Base is copied to the same firmware address range already used by Jixia at `0x80000000`. QEMU physically implements that region as RAM, but Jixia must treat it as contained/early backing until the explicit DDR lifecycle reaches `DDR_ONLINE` and the contained-mode transition completes.

This deliberately models the POWER invariant rather than the physical cache mechanism.

A linker-reserved, page-aligned 64 KiB bootstrap pool is allocator-owned contained memory. It is separate from resident code/data/stacks and supplies the first real Sv39 page tables and pflash-backed page-in buffer.

## 7. DDR state machine

The first implementation exposes explicit stages even when their internal work is stubbed:

```text
DDR_OFFLINE
    -> DDR_DISCOVERED
    -> DDR_TRAINING
    -> DDR_TRAINED
    -> DDR_ADDRESS_MAP_READY
    -> DDR_DECODE_COMMITTED
    -> DDR_ONLINE
```

`DDR_ONLINE` means the hardware/main-memory path is operational enough to receive the contained-state castout. It does **not** immediately make normal DDR pages allocator-visible. Mainstore becomes allocator-visible only after the contained -> mainstore transition commits.

The QEMU platform keeps named stages:

```text
ddr::discover()
ddr::start_training()
ddr::finish_training()
ddr::build_topology()
ddr::build_address_map()
ddr::program_decode()
ddr::online()
```

Later Hostboot MSS-style discovery, effective configuration, grouping/interleave, PA layout, and BAR programming replace these stubs without replacing the lifecycle.

## 8. Contained -> mainstore state machine

The first software contract is:

```text
MEM_CONTAINED
    -> DDR_ONLINE
    -> MEM_TRANSITIONING
    -> contained_flush/castout abstraction
    -> MEM_MAINSTORE
    -> promote existing allocator-owned contained ranges to DDR backing
       without changing base/next/end addresses
    -> add remaining normal DDR range
```

On QEMU, `contained_flush/castout` is a semantic transition because the machine does not expose a POWER-style backing-cache mode. On Jingjie/SimSoc it may later become real dirty-line writeback into DDR at the same PA.

The PageManager transition is deliberately a backing-label promotion, not an object move:

```text
before:
  ManagedRange { base, next, end, backing=CONTAINED }

after:
  ManagedRange { same base, same next, same end, backing=DDR }
```

This is the software-visible proof of the POWER-style stable-address invariant.

## 9. Pageable image contract

The resident Base contains everything necessary to service the first page fault before DDR:

```text
trap/exception entry
minimal Sv39 page-table mechanism
minimal range-based physical PageManager
resident pflash provider
critical diagnostics
memory lifecycle state
```

Extended firmware remains in pflash and is not eagerly copied merely because it exists.

Accepted pre-DDR path:

```text
S-mode JALR to unmapped VA 0x40000000
-> instruction page fault to M-mode
-> trusted M TrapFrame
-> PageManager allocates contained EarlyMemory page
-> pflash provider copies Extended page
-> Sv39 RX mapping installed
-> sfence.vma + fence.i
-> saved mepc is NOT advanced
-> mret retries original instruction fetch
-> pageable code executes and returns magic
-> controlled S ECALL reports completion
```

Required post-DDR proof for M00-07.05:

```text
access another nonresident firmware VA
-> page fault
-> allocate DDR-backed page
-> flash_read(page)
-> install mapping
-> resume faulting instruction
```

## 10. M00-07 acceptance sequence

### M00-07.01 — pflash Stage0 -> resident Base — DONE

```text
[x] 32 MiB pflash image is generated with explicit header
[x] QEMU runs with pflash0 + -bios none
[x] Stage0 executes XIP from 0x20000000
[x] Stage0 validates header and copies Base to 0x80000000
[x] existing a0/a1 handoff survives
[x] Base enters the existing Mozi bootstrap
[x] old M00 foundation regressions remain intact on the normal path
```

Accepted evidence:

```text
GitHub Actions run 31665208312
pflash_size=33554432
stage0_size=255
base_load=0x80000000
base_entry=0x80000000
M00_07_PFLASH_STAGE0: PASS
M00_07_BASE_TRANSFER: PASS
M00-07.01 pflash Stage0 -> resident Base: PASS
```

### M00-07.02 — contained EarlyMemory state — DONE

```text
[x] explicit memory-domain state exists
[x] Base begins in MEM_CONTAINED
[x] DDR lifecycle begins in DDR_OFFLINE
[x] the first 8 MiB firmware window reports contained backing semantics
[x] addresses immediately outside the contained window are unavailable pre-DDR
[x] normal DDR allocation remains disabled before DDR_ONLINE/mainstore transition
```

Accepted evidence:

```text
GitHub Actions run 31665697206
format/build: PASS
M00-02..M00-06 regressions: PASS
M00-07.01 pflash Stage0 -> resident Base: PASS
M00-07.02 explicit contained EarlyMemory state: PASS
```

The 8 MiB value is a QEMU-v0 semantic contained capacity, not a claim that QEMU provides an 8 MiB L2/L3 cache. The software contract remains `EarlyMemory`/contained backing.

### M00-07.03 — pre-DDR flash-backed page fault — DONE

```text
[x] resident pflash provider exists and consumes the same JixiaFlashHeader v1
[x] minimal range-based PageManager owns an explicit contained bootstrap pool
[x] minimal Sv39 page-table walker allocates its own tables from PageManager
[x] one executable Extended page remains only in pflash
[x] S-mode performs a real instruction fetch from unmapped VA 0x40000000
[x] M-mode receives a real instruction-page-fault TrapFrame
[x] fault handling verifies DDR is still OFFLINE and allocator-invisible
[x] PageManager supplies a contained EarlyMemory page
[x] pflash provider fills the page and Sv39 installs an RX mapping
[x] mret retries the same saved mepc rather than skipping the faulting fetch
[x] pageable code executes and returns the expected magic value
```

Accepted evidence:

```text
GitHub Actions run 31666809917
M00_07_PRE_DDR_PAGING_ARMED: PASS
M00_07_PRE_DDR_PAGE_FAULT: PASS
M00_07_PRE_DDR_FLASH_READ: PASS
M00_07_PRE_DDR_BACKING_EARLY: PASS
M00_07_PRE_DDR_PAGING_RESUME: PASS
M00-07.03 pre-DDR flash-backed paging: PASS
```

This is the first Jixia proof that a pageable firmware component can remain in flash and be faulted into temporary/contained memory before DDR is available.

### M00-07.04 — fake DDR lifecycle and mainstore transition — ACTIVE

```text
[ ] fake platform walks discovery -> training -> topology -> PA map -> decode -> online
[ ] DDR remains allocator-invisible even after hardware state reaches ONLINE
[ ] mainstore transition has an explicit contained flush/castout commit point
[ ] a live pre-DDR object keeps the same pointer/PA and contents across transition
[ ] firmware-visible backing changes CONTAINED -> DDR only when transition commits
[ ] PageManager promotes existing contained ranges without moving them
[ ] remaining mainstore is added only after transition
[ ] post-transition allocations are DDR-backed
```

### M00-07.05 — post-DDR paging and EarlyMemory retirement

```text
[ ] second Extended page faults from pflash into DDR
[ ] pre-DDR page-table and allocator state survives the backing transition
[ ] contained-only ownership is retired explicitly
[ ] final invariant checker reports no stale EarlyMemory ownership
[ ] complete M00-07 acceptance is machine-checkable
```

## 11. Final M00-07 target log

```text
[M00-07] boot source = pflash
[FLASH] header valid
[M00-07] resident Base transfer PASS

[MEM] state = CONTAINED
[PF] pre-DDR flash-backed fault
[PM] backing = EARLY

[DDR] discovered
[DDR] training
[DDR] trained
[DDR] address map ready
[DDR] decode committed
[DDR] online

[MEM] transition CONTAINED -> MAINSTORE
[MEM] stable firmware identity PASS

[PF] post-DDR flash-backed fault
[PM] backing = DDR

[MEM] EarlyMemory retired
[M00-07] PASS
```

## 12. Non-goals

M00-07 does not attempt to finish:

```text
real DDR PHY training
real DIMM SPD/VPD stack
final MSS grouping/interleave policy
NUMA allocator policy
buddy/slab design optimization
CXL memory hotplug
LPAR memory ownership
full RAS retirement/quarantine
secure boot signing/A-B recovery
real L2/L3 CAR hardware in QEMU
```

Those mechanisms must fit into the lifecycle established here rather than forcing the lifecycle to be redesigned later.
