# Jixia Solo Development Roadmap

## Status

This is the canonical execution plan for Jixia's current one-person development mode.

**Last updated:** 2026-08-18

**Latest completed milestone:** M00-08.01 Hostboot-shaped Task Executive

**Current milestone:** M00-08.02 Hostboot Scheduler Alignment

**Immediate next step:** close RV64/QEMU acceptance for machine-timer preemption, per-hart
sleep/wakeup queues, deadline-aware idle dispatch, and pre-release task-stack mappings.

Architecture checkpoint: `docs/JIXIA_BOOT_SERVICE_NATIVE_SBI_ARCHITECTURE_2026-08-17.md`.

Current implementation checkpoint: `docs/JIXIA_M00_08_HOSTBOOT_EXECUTIVE_FOUNDATION.md`.

Active scheduler checkpoint: `docs/JIXIA_M00_08_02_HOSTBOOT_SCHEDULER_ALIGNMENT.md`.

The goal is not maximum feature throughput. The goal is to understand, implement, test, and record each mechanism deeply enough that the architecture remains coherent and teachable.

## 1. Working model

```text
one implementation milestone or architecture research gate
    -> study the primary reference implementation
    -> define responsibility boundaries
    -> define invariants and failure modes
    -> implement minimum mechanism
    -> add machine-checkable acceptance
    -> retain old regressions
    -> update design/progress records
    -> integrate accepted checkpoint into main
    -> choose next milestone
```

At any time:

```text
NOW       exactly one primary milestone or research gate
NEXT      at most a few ordered items
BACKLOG   accepted later work
FROZEN    work blocked by architectural prerequisites
```

## 2. Reference discipline

For firmware lifecycle questions, use:

```text
Hostboot whole-system flow
    -> Jixia platform requirements
    -> seL4 protection/capability ideas
    -> NXP component/package ideas
    -> Linux/other implementation comparisons
```

Hostboot is the primary reference for:

```text
kernel/bootstrap flow
user/service startup
VFS/PNOR Resource Providers
InitService
isteps
HWP invocation
memory initialization
cache-contained operation
mainstore transition
RAS integration
```

seL4 is not the boot-flow template; it is a protection/mechanism reference. NXP is not the boot-flow template; it is a component-boundary/package reference.

OpenSBI is the primary RISC-V SBI/runtime reference implementation and compatibility oracle. Jixia implements the SBI standard ABI with Jixia-owned M-mode mechanisms rather than embedding the complete OpenSBI `lib/sbi` executive as the runtime owner.

## 3. Branch and integration policy

- `main` is the latest stable integrated checkpoint.
- `milestone/<id>-<topic>` is the current implementation branch.
- research/docs branches may capture architecture findings before or between implementation milestones.

Completion rule:

1. implementation complete;
2. acceptance scripts and regression chain green;
3. design record updated;
4. `docs/JIXIA_PROGRESS.md` updated;
5. `PROJECT_CONTEXT.md` updated when architecture direction/current state changes;
6. milestone integrated promptly into `main`, normally as one semantic squash checkpoint;
7. next implementation branch starts from current `main`.

Do not leave completed milestone branches unintegrated while `main` becomes stale.

## 4. Completed foundation

```text
DONE  M00-00  Minimal RV64 boot, stack, BSS, UART
DONE  M00-01  Minimal fatal M-mode trap
DONE  M00-02  Complete RV64 TrapFrame
DONE  M00-03  Recoverable trap and mret
DONE  M00-04  Machine timer interrupt
DONE  F00-01  Kernel Print foundation
DONE  M00-05  Per-hart state, private stacks, SMP foundation
DONE  M00-06  Privilege transition foundation
DONE  M00-07  Pre-DDR Memory Foundation
```

M00-07 established:

```text
pflash/PNOR-equivalent image
OpenPOWER-compatible FFS
Stage0 -> resident JXBASE
contained EarlyMemory
4 KiB PageManager
Sv39 pre-DDR page tables
JXEXT pageable from pflash
real pre-DDR instruction page fault
FlashProvider fill into EarlyMemory
fake DDR/mainstore mechanism prototype
stable-address backing transition
prepare-before-publish allocator gating
```

M00-07 does not claim a production post-DDR firmware flow. See `docs/JIXIA_M00_07_MEMORY_FOUNDATION.md`.

## 5. Research gate closed — Hostboot service startup

The research gate after M00-07 established enough of the Hostboot execution model to freeze the next milestone.

Confirmed architecture lessons include:

```text
kernel PageManager/heap/VMM exist before the first firmware task
    -> first init task bootstraps a resident root VFS/registry
    -> Base InitService starts resident prerequisites
    -> PNOR precedes Extended VFS/Resource Provider
    -> extended/pageable components are looked up through prebuilt metadata
    -> a page fault can block the faulting task while a userspace provider fills the page
    -> provider response installs translation state and resumes the original task
    -> Hostboot firmware tasks execute at lower privilege than the kernel
```

Detailed source research belongs in the Firmware Google Drive Hostboot/OpenPOWER knowledge area. The durable Jixia consequences are recorded in `docs/JIXIA_BOOT_SERVICE_NATIVE_SBI_ARCHITECTURE_2026-08-17.md`.

## 6. NOW — M00-08 Boot Service Execution Foundation

Primary objective:

> Turn the accepted M00-06/M00-07 privilege and VMM mechanisms into a real first-class boot-task execution substrate.

Production boot privilege model:

```text
M-mode
    Jixia microkernel
    bare / physical address domain

S-mode
    unused by boot services
    reserved for later OS/hypervisor use

U-mode
    disposable Jixia boot services
    Sv39 translated
```

M00-06/M00-07 S-mode contexts remain mechanism tests only.

### M00-08 increment ledger

```text
08.01  DONE    TaskContext + U dispatch + task/tracker lifecycle
               + ready queues + idle + create/yield/end/wait/detach
08.02  ACTIVE  mtime preemption + sleep/wakeup + deadline-aware idle
08.03  NEXT    message queue + blocking/wakeup IPC foundation
08.04  NEXT    safe user-copy/translation syscall boundary
08.05  NEXT    resident Root Component Registry / prebuilt component catalog
08.06  NEXT    init_main bootstrap -> registry -> InitService
08.07  NEXT    minimal Base InitService task list and lifecycle acceptance
```

M00-08.01 absorbed the originally separate minimal scheduler and task-syscall increments so the
accepted result is a real task lifecycle rather than an M-mode sequential probe.

### M00-08.01 accepted boundary

Prove at least:

```text
M-mode kernel remains in the bare/physical domain
TaskContext owns an explicit VSpace/satp identity
mret enters a real U-mode task
U-mode executes through Sv39
U-mode ECALL traps directly to the M-mode kernel
kernel recovers current task identity and arguments
return/termination is controlled by task state rather than a probe-specific path
```

The first implementation may use a simple/shared service page-table root, but task APIs must not hard-code a global `satp`; later per-service VSpaces must fit without redesigning task context.

M00-08 does not implement a general Linux-style runtime dynamic ELF linker. ELF remains a build-time input; runtime starts preprocessed firmware components through a component catalog.

### M00-08.02 acceptance direction

Prove at least:

```text
a CPU-bound U task that never yields is preempted by mtime
a ready witness task executes behind that CPU-bound task
the interrupted context resumes and exits normally
a sleeping task becomes BLOCK_SLEEP
idle uses the nearest wake deadline
timer expiry returns the sleeper to READY and resumes after ECALL
task creation no longer mutates the shared bootstrap root after hart release
all M00-02 through M00-08.01 regressions remain green
```

## 7. NEXT — provider-backed pageable components

After M00-08 establishes real tasks, scheduler, IPC, and InitService bootstrap, replace the M00-07 direct trap-to-FlashProvider acceptance path with the production provider model:

```text
U service page fault
    -> M VMM
    -> VmRegion / BackingObject
    -> allocate frame
    -> send Resource Provider request
    -> faulting task BLOCKED
    -> resident/pinned provider runs
    -> PNOR/backing store -> frame
    -> provider response
    -> install PTE
    -> wake task
    -> retry exact faulting instruction
```

This milestone will also formalize the extended component catalog/manifest boundary.

Preferred component flow:

```text
ELF at build time
    -> validate/relocate/layout
    -> generate component identity, entry, VmRegions, permissions, backing, dependencies
    -> later capability/hash/signature metadata
    -> PNOR + catalog

runtime
    ComponentId/name -> catalog -> VSpace/backing -> task
```

## 8. Memory continuation after InitService/provider paging exists

The later memory continuation should be natural:

```text
InitService
    -> memory isteps
    -> SPD/VPD/attributes/topology
    -> host-owned DDR configuration/training
    -> memory diagnostics
    -> grouping/interleave/address map
    -> decode viable
    -> exit contained
    -> kernel VMM/PageManager mainstore extension
    -> continue the same firmware/service execution
    -> post-DDR PNOR-backed page fault
    -> DDR-backed page allocation
```

Acceptance must then prove:

```text
pre-DDR Sv39 root survives
pre-DDR L1/L0 tables survive
existing VA mappings survive
existing live firmware object identities survive
no stale contained-only allocator ownership remains
new page faults allocate DDR
real contained backend is retired correctly on Jingjie/hardware
```

Do not re-create VMM/page tables after DDR merely to make the test pass; the point is continuity across the transition.

## 9. Boot-to-runtime lifecycle

Boot U-mode services are ephemeral firmware processes. They must not become permanent runtime dependencies.

Future `exit_boot_services()` semantics include:

```text
stop new boot task creation
finish/cancel boot graph
quiesce providers
complete/drain IPC
revoke boot-scoped capabilities
release task stacks/pages
tear down service VSpaces
invalidate stale translations
remove temporary physical permissions
scrub sensitive boot-only state
publish final platform handoff
enter persistent Jixia M-mode runtime
```

After this boundary, the normal system software world is:

```text
Native:
    M   Jixia Runtime
    S   Linux / OS
    U   applications

Virtualized:
    M   Jixia Runtime
    HS  hypervisor / host kernel
    VS  guest kernel
    VU  guest application
```

## 10. Native Jixia SBI/runtime direction

Jixia Runtime remains the persistent M-mode machine authority.

```text
Jixia M Runtime
    -> SBI standard ABI
    -> hart/timer/IPI/reset/platform runtime mechanisms
    -> machine-level RAS containment and notification
    -> security monitor / trust continuity
    -> confidential-computing hooks and state
    -> Management Complex interface
```

OpenSBI is used as:

```text
reference implementation
compatibility target
differential oracle
source of implementation ideas
```

Do not make the complete OpenSBI `lib/sbi` the authoritative Jixia runtime core because that library also owns trap, hart/HSM, domains, PMP/protection, IPI, timer, TLB, IRQ, PMU, heap/scratch, and related M-mode executive state.

Selective leaf reuse is permitted only when ownership remains explicit and non-overlapping.

## 11. Management Complex roadmap boundary

Preferred role:

```text
Boot prerequisite:
    root of trust
    minimum power/clock/PLL/reset
    release host

Runtime/OOB:
    RAS collection
    telemetry
    watchdog
    thermal/power monitoring
    BMC communication
    rule/health monitoring
    recovery/degrade coordination
```

Heavy DDR training, large HWP libraries, rich attribute databases, and complex boot orchestration remain host firmware responsibilities unless a later hardware dependency proves otherwise.

This keeps Management Complex SRAM and software footprint proportional to its always-on management role.

Runtime responsibility is intentionally three-layered:

```text
Management Complex
    host-independent/OOB monitoring and recovery coordination

Jixia M Runtime
    architectural machine authority, SBI, RAS/security machine actions

S/HS OS or hypervisor
    process/page/VM/workload recovery and system policy
```

## 12. Planned later foundations

Existing planned work remains valid, but ordering follows architectural dependencies:

```text
ACTIVE   M00-08 Boot Service Execution Foundation
NEXT     provider-backed pageable component foundation
NEXT     real InitService/ISTEP memory continuation
PLANNED  Structured event and trace ABI
PLANNED  Consolidated automated QEMU test harness
PLANNED  PlatformGraph runtime model
PLANNED  service isolation and restart
PLANNED  capability-secured device/resource ownership
PLANNED  native SBI runtime expansion
PLANNED  rule-driven RAS/PRD-style diagnosis
PLANNED  secure lifecycle and firmware update
PLANNED  ArchHV / LPAR work after prerequisites
PLANNED  confidential LPAR work after virtualization/security prerequisites
```

Do not preserve an old milestone number merely because it was once listed as NEXT; architectural dependency order wins.

## 13. Later phase direction

### Service operating substrate

Deliverables eventually include:

```text
service tasks
address-space ownership
minimal typed IPC
capability handles
W^X and guard pages
service crash containment
resource reclamation
restart where lifecycle requires it
```

Boot services are disposable at runtime handoff even if the underlying mechanisms later support restart during boot.

### PlatformGraph and semantic debug

Deliverables:

```text
physical topology and ownership graph
structured trace/event schema
semantic breakpoints
fault injection
state dump/diff
Jingjie synchronization
```

### RAS

Power-style deterministic diagnostic rules remain the trusted spine, extended with:

```text
Structured Event
PlatformGraph
Incident Graph
Machine Health Journal / Case Memory
safe HWP active probes
fleet rule mining
Jingjie replay/counterfactual validation
optional AI hypothesis ranking
```

Recovery decisions remain deterministic and auditable.

### Secure lifecycle / confidential computing / virtualization

These remain later gates after service ownership, memory, structured evidence, runtime SBI foundations, and platform modeling are sufficiently mature.

## 14. Frozen areas

Until prerequisites exist:

```text
FROZEN  production ArchHV/LPAR runtime
FROZEN  HS/VS and G-stage virtualization implementation
FROZEN  virtual interrupt/device architecture
FROZEN  confidential LPAR runtime
FROZEN  migration
FROZEN  simulator-dependent partition hardware experiments
```

Research may continue, but implementation does not bypass prerequisite gates.

## 15. Current rule of thumb

```text
Hostboot tells us how firmware boots.
Jixia defines the lifecycle, ownership, RAS, and security invariants.
seL4 helps us protect the pieces.
NXP helps us package the pieces.
OpenSBI tells us how a mature SBI implementation behaves, but does not own Jixia runtime.
Jingjie helps us prove the whole machine.
```
