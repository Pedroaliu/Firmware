# Jixia Solo Development Roadmap

## Status

This is the canonical execution plan for Jixia's current one-person development mode.

**Last updated:** 2026-08-17

**Latest completed milestone:** M00-07 Pre-DDR Memory Foundation

**Immediate next step:** Hostboot execution-flow research gate before freezing the next implementation milestone

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

## 3. Branch and integration policy

- `main` is the latest stable integrated checkpoint.
- `milestone/<id>-<topic>` is the current implementation branch.
- research branches may capture architecture findings before a milestone is frozen.

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

## 5. NOW — Hostboot service/InitService research gate

Before creating the next implementation milestone, trace Hostboot from Base/kernel entry to real firmware services and memory isteps.

Required study path:

```text
Hostboot Base entry
    -> kernel initialization
    -> scheduler/task foundation
    -> VMM
    -> VFS
    -> PNOR Resource Provider
    -> first user/service task
    -> InitService
    -> module/service load
    -> istep dispatch
    -> HWP invocation
    -> memory isteps
    -> proc_exit_cache_contained
    -> MM_EXTEND_REAL_MEMORY / VMM extension
```

Research output must answer:

1. what runs in Hostboot kernel versus user space before DDR;
2. when the first user/service task starts;
3. how pageable HBI/service code is loaded and resumed;
4. how a Resource Provider participates in a page fault without making the pager recursively depend on pageable critical-path code;
5. how InitService sequences hardware work and HWP libraries;
6. which memory stages belong to host firmware and which minimum prerequisites belong to Boot Engine/Management Complex;
7. the exact ordering of DDR viability, BAR/decode setup, exit-contained, and VMM/mainstore extension;
8. which Hostboot mechanisms should be retained conceptually and which protection boundaries should be strengthened using seL4-style capabilities/address spaces;
9. what RISC-V M/S/U placement best fits the resulting Jixia model.

Do not implement a fake user service merely to advance the roadmap. Freeze the next milestone only after these questions are answered.

## 6. Likely next implementation direction — not yet numbered

The next implementation milestone is expected to establish a minimal Hostboot-style firmware service execution substrate, but its exact scope and milestone number remain deliberately open until the research gate closes.

Candidate mechanisms:

```text
task/thread object
minimal scheduler
service address space
message/IPC foundation
service lifecycle
VFS/module-load boundary
initial InitService
minimum capability/object ownership
```

The first service demo should be architecture-driven, not a generic OS demo.

Likely target:

```text
resident Base/kernel
    -> create initial firmware service execution context
    -> load/start a small pageable service/module
    -> service communicates with kernel through defined mechanism
    -> InitService can sequence a small synthetic istep list
```

Only after this exists should DDR initialization be moved into the real host boot flow.

## 7. Memory continuation after InitService exists

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

## 8. Management Complex roadmap boundary

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

## 9. Planned later foundations

Existing planned work remains valid, but ordering may move behind the Hostboot-style service substrate:

```text
PLANNED  Structured event and trace ABI
PLANNED  Consolidated automated QEMU test harness
PLANNED  PlatformGraph runtime model
PLANNED  service isolation and restart
PLANNED  capability-secured device/resource ownership
PLANNED  rule-driven RAS/PRD-style diagnosis
PLANNED  secure lifecycle and firmware update
PLANNED  ArchHV / LPAR work after prerequisites
PLANNED  confidential LPAR work after virtualization/security prerequisites
```

Do not preserve an old milestone number merely because it was once listed as NEXT; architectural dependency order wins.

## 10. Later phase direction

### Service operating system

Deliverables eventually include:

```text
service tasks
address-space ownership
minimal typed IPC
capability handles
W^X and guard pages
service crash containment
resource reclamation
restart
```

The service architecture should improve on Hostboot's historical protection limits without abandoning its firmware-oriented boot flow.

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

These remain later gates after service ownership, memory, structured evidence, and platform modeling are sufficiently mature.

## 11. Frozen areas

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

## 12. Current rule of thumb

```text
Hostboot tells us how firmware boots.
Jixia tells us which invariants and server requirements matter.
seL4 helps us protect the pieces.
NXP helps us package the pieces.
Jingjie helps us prove the whole machine.
```
