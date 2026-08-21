# Jixia Project Context

> Persistent entry point for future chat sessions, contributors, and coding agents.
> Repository state and accepted design records are authoritative over conversational memory.

## 1. Canonical identity

- **Project/platform:** 稷下 / **Jixia**
- **Repository:** `Pedroaliu/Firmware`
- **Stable integration branch:** `main`
- **Latest completed milestone:** `M00-08.02 Hostboot Scheduler Alignment` — DONE (`de4df0e`, PR #26; CI run `32219284629`)
- **Current implementation milestone:** `M00-08.03 Message IPC Foundation` — ACTIVE; M00-08.03.01 (non-blocking Endpoint/Message IPC) and M00-08.03.02 (blocking recv + FIFO wakeup) DONE, M00-08.03.03 (call/reply + ReplyToken) at ABI-candidate stage — review pending
- **Project type:** RISC-V firmware-native server platform research project

Jixia studies firmware, logical partitions, RAS, confidential computing, management-plane design, and full-system simulation as one co-designed platform. It is a learning and architecture project, not a short path to cloning EDK II, KVM, PowerVM, OpenSBI, or any single existing firmware stack.

Canonical live status: `docs/JIXIA_PROGRESS.md`.

Canonical execution plan: `docs/JIXIA_SOLO_ROADMAP.md`.

Current boot/runtime architecture checkpoint: `docs/JIXIA_BOOT_SERVICE_NATIVE_SBI_ARCHITECTURE_2026-08-17.md`.

## 2. Architecture reference priority

Whole-system firmware boot flow is Hostboot-first.

```text
1. IBM Hostboot
   primary reference for:
   - IPL control flow
   - kernel/user firmware split
   - InitService and istep orchestration
   - HWP execution model
   - PNOR/VFS/resource providers
   - cache-contained operation
   - memory initialization
   - exit-contained/mainstore transition
   - RAS/PRD integration patterns

2. Jixia platform requirements
   determine where Jixia intentionally differs

3. seL4 and related microkernels
   secondary reference for:
   - capability security
   - address-space isolation
   - least privilege
   - kernel/service mechanism boundaries
   - fault containment

4. NXP and similar firmware frameworks
   secondary reference for:
   - component manifests
   - dependencies
   - versioning
   - standardized package/service boundaries

5. OpenSBI
   primary RISC-V SBI/runtime reference implementation and compatibility oracle
   not the authoritative Jixia runtime owner

6. Linux/other operating systems
   implementation and comparison reference where applicable
```

Do not invent a generic microkernel boot flow first and retrofit firmware behavior later. For boot, memory, PNOR, istep/HWP, runtime transition, and RAS lifecycle questions, inspect Hostboot first.

For SBI behavior and compatibility, inspect the SBI specification and OpenSBI first, but preserve Jixia ownership of machine-level runtime state.

## 3. Host versus Management Complex boundary

The Management Complex is not intended to become a second Hostboot.

Preferred responsibility split:

```text
Boot Engine / minimum prerequisite logic
    -> make the host safely executable
    -> root of trust / secure-load prerequisites
    -> reset release
    -> minimum power and PLL/clock prerequisites
    -> minimum fabric/pervasive setup needed to release host

Host Jixia firmware
    -> make the platform operational
    -> microkernel and boot services
    -> InitService / istep orchestration
    -> heavy HWP libraries
    -> processor/fabric initialization
    -> SPD/VPD/attribute processing
    -> DDR configuration/training/diagnostics
    -> memory grouping/interleave/address map
    -> PCIe/CXL and later platform initialization

Management Complex
    -> keep the platform manageable even when host is unhealthy
    -> always-on runtime and out-of-band control
    -> RAS event aggregation and monitoring
    -> watchdog and recovery coordination
    -> telemetry
    -> power/thermal supervision
    -> BMC/OOB communication
    -> predictive RAS/rule execution and health monitoring

Jixia M Runtime
    -> persistent host-side machine authority after boot services are destroyed
    -> SBI
    -> machine-level RAS containment/notification
    -> security/trust continuity
    -> confidential-computing hooks/state
    -> Management Complex coordination
```

Heavy boot algorithms remain on host cores where resident Base + contained memory + PNOR demand paging can support code larger than the early-memory capacity. Avoid requiring a large Management Complex SRAM merely to execute host initialization libraries.

## 4. Development model

The project is intentionally single-threaded:

```text
NOW       one primary implementation milestone or one architecture research gate
NEXT      at most a few ordered items
BACKLOG   accepted later work
FROZEN    work blocked by missing prerequisites
```

Milestone completion requires:

```text
architecture/invariants
-> implementation
-> machine-checkable acceptance
-> regression preservation
-> design/progress records
-> integration into main
```

Development branches may contain fine-grained implementation/debug commits. Accepted milestones are integrated into `main` as semantic checkpoints, normally by squash merge.

## 5. Naming policy

Jixia is the project and product brand. Chinese cultural names are implementation codenames, not public source-code vocabulary.

| Codename | Responsibility | Semantic code area |
|---|---|---|
| Pangu / 盘古 | immutable Boot0 | `boot/`, `jixia::boot` |
| Mozi / 墨子 | host firmware microkernel | `microkernel/`, `jixia::microkernel` |
| Nuwa / 女娲 | PlatformGraph/topology | `platform/model/`, `jixia::platform` |
| ArchHV | firmware-native type-1 hypervisor | `hypervisor/` |
| Yixing / 弈星 | scheduling/placement | hypervisor scheduler |
| Shouyue / 守约 | resource contracts | hypervisor contracts |
| Dunshan / 盾山 | isolation/IOMMU/DMA | isolation layer |
| Luban / 鲁班 | Linux driver/boot domain | `services/driver_domain/` |
| Yuange / 元歌 | firmware personalities | `firmware_personality/` |
| Bianque / 扁鹊 | RAS diagnosis | `ras/diagnosis/` |
| Taiyi / 太乙 | recovery | `ras/recovery/` |
| Sunbin / 孙膑 | virtual time/migration | `virtualization/time/` |
| Guigu / 鬼谷 | dynamic debug/introspection | `debug/` |
| Jingjie / 镜界 | full-system simulator | `interfaces/simulator/` |

Source directories, interfaces, types, functions, schemas, and C++ namespaces use clear English technical names.

## 6. Privilege and lifecycle architecture

### Boot Realm

Production Jixia boot-service placement is now frozen as:

```text
M-mode
    Jixia microkernel
    bare / physical address domain
    trap, task, scheduler, VMM, PageManager, IPC, capability enforcement

S-mode
    intentionally unused by Jixia boot services
    reserved for later OS/hypervisor use

U-mode
    disposable firmware boot services
    Sv39-translated service/task address spaces
    InitService, PNOR/VFS providers, MemoryInit, FabricInit, PCIe/CXL init, etc.
```

M00-06/M00-07 S-mode code remains acceptance/probe machinery only.

A task must carry an explicit VSpace/address-space identity. The first implementation may share one simple root, but APIs must not assume one global `satp`.

Normal M-mode kernel execution remains physical/bare. Kernel code must not directly dereference U virtual pointers as physical pointers; future user-memory access uses explicit copy/translation helpers.

### Boot-service teardown

Boot U-mode services are ephemeral firmware processes. They cannot be permanent runtime dependencies.

Future `exit_boot_services()` tears down boot tasks, VSpaces, stacks, IPC endpoints, and boot-scoped capabilities before the system software world takes ownership.

### Runtime Realm

After boot-service teardown:

```text
Native:
    M   Jixia Runtime
    S   Linux / OS kernel
    U   applications

Virtualized:
    M   Jixia Runtime
    HS  host hypervisor / host kernel
    VS  guest kernel
    VU  guest applications
```

## 7. Native SBI/runtime policy

Jixia Runtime implements the RISC-V SBI standard ABI with Jixia-owned mechanisms.

OpenSBI is used as:

```text
reference implementation
compatibility target
differential oracle
implementation study source
```

Do not make the complete OpenSBI `lib/sbi` the authoritative Jixia runtime core. It contains far more than ecall decoding: trap handling, hart/HSM state, domains, PMP/protection, IPI, timer, TLB, interrupt/PMU integration, heap/scratch, and other M-mode executive mechanisms.

The architectural reason for a native runtime is not that real silicon never integrates OpenSBI; some platforms do. The reason is that Jixia must retain one authoritative owner for machine-level RAS, security, confidential-computing, trap/delegation, hart, and protection state.

Selective leaf code reuse is allowed only when ownership remains explicit and non-overlapping.

## 8. Component and loader direction

ELF remains the compiler/build-time interchange format.

Preferred firmware flow:

```text
ELF component
    -> Jixia image builder
    -> validation / relocation / layout
    -> component catalog/manifest
       identity
       entry
       virtual ranges
       permissions
       backing object
       dependencies
       later capabilities/hash/signature metadata
    -> PNOR image
```

Pre-DDR runtime starts preprocessed components through the catalog rather than implementing a Linux-style general dynamic ELF loader.

The resident root component registry/provider bootstrap must not recursively depend on the pageable component world it creates.

## 9. Page-fault/provider direction

M00-07 proved direct kernel-side PNOR materialization. The target service model is:

```text
U service page fault
    -> M VMM
    -> locate VmRegion/backing
    -> allocate physical page
    -> send Resource Provider request
    -> block faulting task
    -> schedule resident/pinned provider
    -> provider fills page from PNOR/backing store
    -> response to kernel
    -> install PTE
    -> wake original task
    -> retry exact instruction
```

The page fault repairs backing and resumes execution. It does not drive boot-phase state machines such as DDR initialization.

Read-side pageable backing and persistent PNOR mutation remain separate authority paths.

## 10. Accepted implementation through M00-07

### M00-00 through M00-04

- RV64 QEMU virt reset entry, stacks, BSS, UART.
- minimal fatal M trap.
- complete integer TrapFrame and common save/restore path.
- recoverable 32-bit `EBREAK` and 16-bit `C.EBREAK`.
- recoverable machine timer interrupt.
- Kernel Print foundation.

### M00-05 — SMP foundation

Accepted:

```text
private per-hart stacks
HartId != dense HartIndex
boot-hart-owned global initialization
release/acquire publication
HartLocal
mscratch -> HartLocal
bounded FDT population discovery
per-hart timer state/compare
1/2/4-hart acceptance
controlled over-capacity rejection
```

### M00-06 — privilege transition foundation

Accepted:

```text
trusted per-hart M trap stack
M-origin and lower-origin M traps use trusted trap storage
interrupted lower-privilege sp preserved only as a value
controlled M->S transition
controlled S->M->S ECALL round trip
hostile S sp proof
missing HartLocal anchor fails closed
```

M00-06 does not define the production service privilege model; the production model is M-mode kernel + U-mode boot services.

### M00-07 — Pre-DDR Memory Foundation

Accepted:

```text
32 MiB pflash/PNOR-equivalent image
OpenPOWER-compatible FFS v1 partition table
XIP Stage0
JXBASE discovery by FFS partition identity
resident Base transfer
explicit contained EarlyMemory state
4 KiB PageManager bootstrap pool
Sv39 page-table construction from EarlyMemory
resident FFS parser and FlashProvider
JXEXT left pageable in pflash
real pre-DDR instruction page fault
pflash -> EarlyMemory fill
RX PTE install and exact instruction retry
fake DDR lifecycle/mainstore mechanism prototype
stable firmware address/content across backing transition
PageManager contained->DDR metadata promotion
prepare-before-publish allocator gating
no mainstore fallback to contained allocation
```

Design record: `docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`.

Primary full-regression evidence: GitHub Actions run `32005255564`.

M00-07 intentionally does not finish a production DDR boot flow. Its DDR/mainstore code is a mechanism prototype used to establish invariants for the later Hostboot-style flow.

## 11. Current implementation — M00-08

M00-08 Boot Service Execution Foundation remains the active major milestone. M00-08.01 is accepted
at `e930a24`; M00-08.02 is DONE at `de4df0e` (CI run `32219284629`); M00-08.03 is ACTIVE with its
first increment M00-08.03.01 DONE at `9c617ec` (PR #29, CI run `32437093429`), its
second increment M00-08.03.02 (blocking recv + FIFO wakeup) DONE at squash
`ba27c4c1a520` (PR #30, CI run `32460452557`), and its third increment
M00-08.03.03 (call/reply + ReplyToken) at ABI-candidate stage.

Actual increment ledger:

```text
08.01    DONE    TaskContext + U dispatch + task/tracker lifecycle
                 + ready queues + idle + create/yield/end/wait/detach
08.02    DONE    mtime preemption + sleep/wakeup + deadline-aware idle
08.03    ACTIVE  message queue + blocking/wakeup IPC foundation
  .03.01 DONE    non-blocking Endpoint/Message IPC: static 16-slot endpoint
                 table, per-endpoint spinlock + depth-16 FIFO, typed
                 index+generation handles (bit 63 clear, generation
                 1..0x7fffffff, ceiling retires the slot), boot-hart-first
                 EndpointManager construction, full 4-word + sender-TaskId
                 register ABI, syscalls 6/7/8/11 (9/10/12 reserved ->
                 -ENOSYS), local acceptance PASS
                 (squash 9c617ec, PR #29; CI 32437093429 SUCCESS)
  .03.02 DONE    blocking recv + multi-receiver FIFO wakeup: syscall 10
                 ipc_recv (9/12 stayed -ENOSYS), per-task MessageWaitNode,
                 per-endpoint waiting FIFO, atomic block/wake/destroy under
                 ep.lock, -EIDRM destroy wake; smp1 deterministic + smp2
                 cross-hart acceptance PASS x3
                 (squash ba27c4c1a520, PR #30; CI 32460452557 SUCCESS)
  .03.03 CAND    call/reply + ReplyToken ABI candidate (syscalls 9/12,
                 self-locating 64-bit token, a6 token out on recv/try_recv,
                 per-endpoint transaction table, fail-closed task-exit
                 boundary); docs only — review pending
  .03.04 NEXT    full task-exit IPC cleanup (split from .03.03 by design)
08.04    NEXT    safe user-copy/translation syscall boundary
08.05    NEXT    resident Root Component Registry
08.06    NEXT    init_main -> registry -> InitService
08.07    NEXT    minimal Base InitService lifecycle
```

Accepted M00-08.03.01 ABI record: `docs/JIXIA_M00_08_03_01_IPC_NONBLOCKING_ABI.md`
(covers the research candidate's non-blocking leans; blocking IPC, ReplyToken,
and task-exit cleanup remain open for later M00-08.03 increments).

Accepted M00-08.03.02 ABI record:
`docs/JIXIA_M00_08_03_02_IPC_BLOCKING_RECV_ABI.md` (blocking recv syscall 10,
multi-receiver FIFO wakeup, -EIDRM destroy wake, atomic block/wake protocols;
squash `ba27c4c1a520`, PR #30, CI run `32460452557`).

M00-08.03.03 ABI candidate (CANDIDATE / FROZEN FOR REVIEW, design only):
`docs/JIXIA_M00_08_03_03_IPC_CALL_REPLY_ABI.md` (call/reply syscalls 9/12,
self-locating 64-bit ReplyToken, a6 token out on recv/try_recv with a5 keeping
the sender TaskId, per-endpoint transaction table, fail-closed task-exit
boundary; timeouts, cancel, capabilities, registry routing, and the full
task-exit IPC cleanup — split to M00-08.03.04 — remain out of scope).

M00-08.02 translated Hostboot's decrementer and delay-list policy to RISC-V `mtime`: save the
interrupted U task, release expired sleepers, select local/global/idle work, restore the selected
task, and arm the next task or sleep deadline. Timer arming is executive mechanism in all M00-08
builds; only the acceptance workload and markers carry the M00-08.02 probe gate.

Current service boundary:

```text
supported now:
    statically resident U-mode task entry
    explicit bootstrap AddressSpace
    create/preempt/block/wake/wait/end lifecycle

not yet a real usr service:
    no named component registry/task_exec
    non-blocking message IPC only (blocking/wakeup paths pending)
    no safe user-copy
    no protected per-service VSpace
```

## 12. Next provider-backed component milestone

After real tasks/scheduler/IPC exist, introduce the production page-fault Resource Provider model and extended component catalog. This replaces direct trap-to-FlashProvider materialization.

Only after this provider model and InitService exist should DDR initialization re-enter the implementation roadmap.

## 13. Deferred memory continuation

Later, under the real Hostboot-style boot flow:

```text
InitService / memory isteps
    -> host-driven DDR discovery/configuration/training/diagnostics
    -> address map / decode viable
    -> exit contained
    -> mainstore/VMM extension
    -> continue the same firmware/service execution
    -> natural post-DDR PNOR-backed page fault
    -> allocate DDR backing
    -> prove pre-DDR page tables and mappings survived the transition
```

Real cache-contained retirement and dirty-line castout semantics belong to Jingjie/real hardware validation rather than the QEMU semantic model alone.

## 14. PNOR persistence direction

M00-07 paging establishes read-side backing. Persistent mutation follows a separate rule:

> Read is a pageable backing operation; write is a privileged persistent transaction.

Ordinary CPU stores must never implicitly write firmware storage. Immutable firmware partitions are RO/RX; future updates/VPD/GUARD/config persistence use explicit scoped services and capabilities.

## 15. RAS direction

Power-style deterministic RAS diagnosis remains the trusted spine. Jixia extends it with structured evidence, topology/ownership correlation, active HWP probes, case memory, replay, and optional AI-assisted hypothesis/rule discovery while keeping accepted recovery policy deterministic and auditable.

Runtime authority is intentionally layered:

```text
Management Complex
    host-independent collection/monitoring/OOB recovery coordination

Jixia M Runtime
    machine architectural containment, SBI/event delivery, security state

S/HS system software
    process/page/VM/workload recovery
```

## 16. Read-first order for future sessions

1. `PROJECT_CONTEXT.md`
2. `docs/JIXIA_PROGRESS.md`
3. `docs/JIXIA_SOLO_ROADMAP.md`
4. `docs/JIXIA_BOOT_SERVICE_NATIVE_SBI_ARCHITECTURE_2026-08-17.md`
5. `docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`
6. relevant architecture/RAS records
7. current branch, recent commits, and current code

Repository state wins over remembered chat state whenever they differ.
