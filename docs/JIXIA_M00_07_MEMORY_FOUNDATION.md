# Jixia M00-07 Memory Foundation

**Status:** ACTIVE
**Branch:** `milestone/m00-07-memory-foundation`
**Reference model:** Hostboot cache-contained -> mainstore lifecycle, adapted to RISC-V/QEMU first and later to Jingjie/SimSoc hardware models
**Accepted through:** `M00-07.02 explicit contained EarlyMemory state`

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
...         future Extended/pageable image
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

The first slice sets `extended_size = 0`; the field is reserved now so later pageable content does not require replacing the image contract.

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

Only after `DDR_ONLINE` may normal System RAM be added to the allocator/resource view.

The API shape should remain decomposed:

```text
ddr_discover()
ddr_train()
ddr_build_topology()
ddr_build_address_map()
ddr_program_decode()
ddr_online()
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
    -> extend normal memory allocator/VMM
    -> retire contained-only ownership
    -> MEM_EARLY_RETIRED
```

On QEMU, `contained_flush/castout` is a semantic transition because the machine does not expose a POWER-style backing-cache mode. On Jingjie/SimSoc it may later become real dirty-line writeback into DDR at the same PA.

## 9. Pageable image contract

The resident Base must eventually contain everything necessary to service a page fault before DDR:

```text
trap/exception entry
VMM/page-table mechanism
minimal physical-page allocator
flash provider/pager
minimal module/VFS metadata needed by the provider
critical diagnostics
DDR lifecycle framework
```

Extended firmware remains in pflash and is not eagerly copied merely because it exists.

Required pre-DDR proof:

```text
access nonresident firmware VA
-> page fault
-> allocate EarlyMemory page
-> flash_read(page)
-> install mapping
-> resume faulting instruction
```

Required post-DDR proof:

```text
access another nonresident firmware VA
-> page fault
-> allocate DDR page
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
[x] normal DDR allocation remains disabled before DDR_ONLINE
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

### M00-07.03 — pre-DDR flash-backed page fault — ACTIVE

```text
[ ] resident pager/provider exists
[ ] one Extended page is deliberately absent
[ ] fault fills an EarlyMemory page from pflash and resumes
```

### M00-07.04 — fake DDR lifecycle and mainstore transition

```text
[ ] explicit DDR state machine reaches ONLINE
[ ] address/decode stages are visible even while stubbed
[ ] contained -> mainstore transition preserves live firmware identity
[ ] remaining DDR memory is added only after transition
```

### M00-07.05 — post-DDR paging and EarlyMemory retirement

```text
[ ] second Extended page faults from pflash into DDR
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
