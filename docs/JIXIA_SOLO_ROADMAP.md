# Jixia Solo Development Roadmap

## Status

This is the canonical execution plan for the current **one-person development mode**.

The project is developed by the repository owner with ChatGPT acting as a research, teaching, architecture, review, and debugging partner. The goal is not maximum parallel throughput. The goal is to understand, implement, test, and document each mechanism deeply enough that the project remains coherent and teachable.

Last updated: **2026-08-05**

## 1. Working model

Jixia follows a single-threaded development rule:

```text
one active milestone
    -> concept and invariants
    -> interface and state design
    -> implementation
    -> normal-path test
    -> failure-path test
    -> trace and observability
    -> documentation and review
    -> progress record
    -> next milestone
```

At any time the project has:

- **NOW**: exactly one primary milestone;
- **NEXT**: at most three ordered milestones;
- **BACKLOG**: all other features;
- **FROZEN**: features that require missing architectural prerequisites.

No new major subsystem begins before the current milestone satisfies its Definition of Done.

## 2. Branch and recording policy

### Branch roles

- `main`: last integrated and stable project checkpoint.
- `roadmap/solo-development`: living integration and progress branch for the solo-development sequence.
- `feature/<milestone>-<topic>`: optional short-lived branch for a risky or substantial implementation.

### Completion rule

When a milestone is completed:

1. implementation and tests are committed;
2. `docs/JIXIA_PROGRESS.md` is updated in the same branch or pull request;
3. `PROJECT_CONTEXT.md` is updated when the active milestone, architecture direction, repository, or trust model changes;
4. the milestone entry includes evidence: commit, test command, expected output, and known limitations;
5. the next milestone becomes NOW only after the completed milestone is recorded.

A feature is not considered complete merely because code exists.

## 3. Current execution queue

### NOW

```text
M00-02  Complete RV64 TrapFrame
```

Required learning and implementation steps:

1. identify architectural state already captured by RISC-V hardware on trap entry;
2. define the RV64 TrapFrame fields, alignment, and stack layout;
3. define shared assembly/C++ offsets without duplicated magic numbers;
4. save the complete required context in assembly;
5. inspect and classify the trap in `jixia::microkernel::trap`;
6. restore the context and prepare for later `mret` support;
7. test register preservation with known values;
8. document nesting, stack-failure, and non-recoverable limitations.

### NEXT

```text
M00-03  Recoverable trap and mret
M00-04  Timer interrupt
M00-05  Per-hart state and stacks
```

### LATER IN M00

```text
M00-06  Privilege transition foundation
M00-07  Early physical allocator
M00-08  Structured event and trace ABI
M00-09  Automated QEMU test harness
```

## 4. Phase plan

The calendar estimates assume approximately **8-12 hours per week** and are planning ranges, not deadlines.

### Phase A — Recoverable microkernel foundation

Estimated duration: **2-3 months**

Deliverables:

- complete TrapFrame;
- recoverable exception path;
- timer interrupt;
- per-hart state and stacks;
- basic privilege transition;
- early allocator;
- structured event/trace foundation;
- automated QEMU tests.

Exit demo:

```text
load known values into registers
    -> trigger exception
    -> inspect TrapFrame
    -> modify only the recovery state
    -> return
    -> prove preserved register state
```

### Phase B — Minimal firmware service operating system

Estimated duration: **3-4 months**

Deliverables:

- service tasks;
- memory-region ownership;
- PMP-backed service isolation;
- capability handles;
- minimal typed IPC;
- guarded stacks and W^X policy;
- service crash detection;
- resource reclamation and restart.

Exit demo:

```text
Service A communicates with Service B
    -> Service A performs an illegal access
    -> Memory Guard contains the fault
    -> only Service A is terminated and restarted
    -> Service B and the microkernel continue
```

### Phase C — PlatformGraph and semantic dynamic debug

Estimated duration: **3-4 months**

Deliverables:

- QEMU virt PlatformGraph schema and runtime model;
- CPU, hart, memory, UART, timer, interrupt, MMIO, ownership, and health objects;
- trace ring and semantic event schema;
- filtering by hart, service, transaction, and BootEpoch;
- event breakpoints;
- fault-injection interface;
- state dump and state diff.

Exit demo:

```text
break on MEMORY_GRANT
    -> display owner, region, and capability
    -> inject IPC timeout
    -> observe diagnosis and recovery evidence
```

### Phase D — Rule-driven RAS, diagnosis, and recovery

Estimated duration: **3-4 months**

Initial fault classes:

- service crash;
- memory violation;
- IPC timeout;
- timer or device timeout.

Deliverables:

- RAS event schema;
- severity and error classes;
- topology and owner correlation;
- structured FFDC;
- typed policy/rule evaluation;
- containment actions;
- service restart and memory isolation;
- simulated page retirement and hart offline;
- degraded PlatformGraph;
- recovery verification.

Exit demo:

```text
inject a service hang
    -> watchdog detection
    -> diagnosis and ownership correlation
    -> capability and memory reclamation
    -> service restart
    -> health verification
    -> auditable recovery record
```

### Phase E — Secure lifecycle, service update, and LinuxBoot preparation

Estimated duration: **4-6 months**

Deliverables:

- image and service manifests;
- measurement log;
- signed service bundles;
- anti-rollback versioning;
- service quiesce;
- state export/import;
- transactional update and rollback;
- minimal Linux image;
- immutable root filesystem;
- containerized or appliance-style boot services;
- boot-candidate discovery and measurement tools.

LinuxBoot remains a preparatory appliance in this phase. It does not become a Service LPAR until the simulator and partition substrate are ready.

### Phase F — Jingjie simulator foundation

Start gate: the following contracts must be stable enough to share:

- TrapFrame;
- event schema;
- fault schema;
- PlatformGraph v0;
- memory ownership model;
- BootEpoch;
- service lifecycle.

Initial simulator sequence:

```text
S0  EventQueue, memory, UART, timer, MMIO
S1  RV64 functional core
S2  privilege architecture and MMU
S3  interrupt and IOMMU models
S4  firmware/simulator semantic-event closure
```

## 5. Frozen feature gates

The following features are deliberately frozen during the firmware-first phases:

- ArchHV implementation;
- HS/VS execution;
- G-stage translation;
- vCPU scheduling;
- virtual interrupts and virtual I/O;
- LPAR resource contracts;
- Service LPAR;
- confidential LPAR runtime;
- secure migration.

LPAR implementation begins only after Jingjie provides:

```text
[ ] M/S/HS/VS privilege modes
[ ] Sv39
[ ] hgatp and G-stage translation
[ ] virtual timer
[ ] virtual interrupt model
[ ] LPID-aware trace
[ ] G-stage and IOMMU fault injection
[ ] shared PlatformGraph/Event/Fault schemas
```

Architecture documents may continue to preserve the long-term LPAR and confidential-computing design, but these features are not in the current implementation queue.

## 6. Definition of Done

Every milestone must satisfy all applicable items:

```text
[ ] concept and invariants documented
[ ] implementation builds without warnings
[ ] normal-path test passes
[ ] failure-path test passes
[ ] result is machine-checkable, not only UART text
[ ] structured trace/event evidence exists
[ ] known limitations are recorded
[ ] design/source documentation is updated
[ ] progress ledger is updated
[ ] commit or PR is linked as evidence
[ ] current milestone does not regress earlier milestones
```

## 7. Learning and collaboration contract

The repository owner writes and debugs the core implementation to build real systems skill.

ChatGPT supports the work by:

- reading specifications, papers, and source references before each milestone;
- teaching the relevant concepts and asking understanding-check questions;
- helping derive data structures, state machines, invariants, and tests;
- reviewing code and logs;
- helping isolate failures through evidence rather than guesses;
- maintaining the roadmap, progress ledger, source manifest, and project context;
- avoiding large ready-made implementations unless explicitly requested or useful for low-learning-value boilerplate.

Each milestone should leave four durable artifacts:

```text
code
 tests
 design note
 learning note
```

## 8. Roadmap maintenance

Update this file only when the phase order, working model, feature gate, or Definition of Done changes.

Routine completion updates belong in `docs/JIXIA_PROGRESS.md`.
