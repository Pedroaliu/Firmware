# Jixia Solo Development Roadmap

## Status

This is the canonical execution plan for the current **one-person development mode**.

The project is developed by the repository owner with ChatGPT acting as a research, teaching, architecture, review, debugging, and implementation partner. The goal is not maximum parallel throughput. The goal is to understand, implement, test, and document each mechanism deeply enough that the project remains coherent and teachable.

Last updated: **2026-08-11**

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
- `milestone/<id>-<topic>`: current milestone implementation branch.
- `feature/<topic>`: optional short-lived branch for a contained supporting feature.

### Completion rule

When a milestone is completed:

1. implementation and tests are committed;
2. `docs/JIXIA_PROGRESS.md` is updated;
3. `PROJECT_CONTEXT.md` is updated when the active milestone, architecture direction, repository, or trust model changes;
4. the milestone entry includes evidence: test command, expected output, design record, and known limitations;
5. the completed milestone is merged promptly into `main`;
6. the next milestone branch starts from that integrated checkpoint.

A feature is not considered complete merely because code exists.

---

## 3. Current execution queue

### Completed M00 foundation

```text
DONE  M00-00  Minimal RV64 boot, stack, BSS, UART
DONE  M00-01  Minimal fatal M-mode trap
DONE  M00-02  Complete RV64 TrapFrame
DONE  M00-03  Recoverable trap and mret
DONE  M00-04  Machine timer interrupt
DONE  M00-05  Per-hart state, private stacks, SMP foundation
```

M00-05 establishes the per-hart substrate required by privilege transition:

```text
private per-hart stack
HartId != dense HartIndex
boot-hart-only global initialization
release/acquire rendezvous
HartLocal
mscratch -> HartLocal
per-hart mtvec
per-hart timer state/compare
actual population != capacity
```

Design record: `docs/JIXIA_M00_05_SMP_FOUNDATION.md`.

### NOW

```text
M00-06  Privilege transition foundation
```

Initial objective:

```text
M-mode Mozi
    -> configure mstatus.MPP = S
    -> configure mepc
    -> mret
    -> execute a controlled S-mode payload with satp = 0
    -> trap/ecall back to M-mode
    -> prove previous privilege and context
    -> return or terminate through a defined path
```

Required learning and implementation steps:

1. review RISC-V `mstatus.MPP`, `mepc`, `mret`, `mcause`, `mtval`, `mscratch`, and `ecall` semantics;
2. define the exact M->S->M state machine before writing the payload;
3. keep `satp = 0` for the first proof so privilege transition is isolated from paging;
4. define what M-mode may trust after lower-privilege execution begins;
5. design trusted per-hart trap/kernel stack entry using `mscratch -> HartLocal`;
6. preserve the interrupted S-mode `sp` in the saved context rather than using it as trusted kernel storage;
7. decide the minimal TrapFrame/entry changes required for previous-privilege execution;
8. add a controlled S-mode payload and an `ecall` or equivalent trap back to M-mode;
9. prove expected `mstatus.MPP`, `mepc`, `mcause`, register preservation, and stack ownership;
10. retain all M00-02 through M00-05 and Kernel Print regressions;
11. document delegation, paging, PMP, nested-trap, and security limitations explicitly.

The first M00-06 proof does **not** add:

```text
Sv39/page tables
physical allocator
scheduler/tasks
user mode
PMP service isolation
ArchHV/HS/VS
full syscall ABI
```

### NEXT

```text
M00-07  Early physical allocator
M00-08  Structured event and trace ABI
M00-09  Automated QEMU test harness
```

---

## 4. Phase plan

Calendar estimates assume approximately **8-12 hours per week** and are planning ranges, not deadlines.

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

Current state: M00-00 through M00-05 are complete; M00-06 is active.

Exit direction:

```text
multiple harts
    -> trusted trap state
    -> controlled privilege transition
    -> allocator
    -> structured event foundation
    -> repeatable automated acceptance
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
- CPU, hart, memory, UART, timer, interrupt, MMIO, ownership, health, and trust objects;
- trace ring and semantic event schema;
- filtering by hart, service, transaction, and BootEpoch;
- event breakpoints;
- fault-injection interface;
- state dump and state diff.

Exit demo:

```text
break on a semantic platform event
    -> display owner, target, state, and capability context
    -> inject a controlled fault
    -> observe diagnosis and recovery evidence
```

### Phase D — Rule-driven RAS, diagnosis, recovery, and AI-era reasoning

Estimated duration: **3-4 months**

Core direction:

- Power-style deterministic diagnostic rules remain the trusted spine;
- Structured Events and PlatformGraph provide common hardware semantics;
- Incident Graph correlates cascaded errors;
- HWP probes provide active diagnosis;
- Machine Health Journal and Case Memory preserve experience;
- AI may rank hypotheses and discover candidate rules;
- accepted recovery actions remain deterministic and policy-controlled;
- Jingjie provides replay, fault injection, and counterfactual validation.

Detailed records:

- `docs/JIXIA_RAS_ARCHITECTURE.md`
- `docs/JIXIA_RAS_REASONING_VISION.md`
- `docs/JIXIA_AI_RAS_ARCHITECTURE_SUMMARY.md`

Exit demo direction:

```text
inject a platform/service fault
    -> structured evidence
    -> topology/owner correlation
    -> diagnosis/hypothesis
    -> optional safe HWP probe
    -> deterministic containment/recovery
    -> health verification
    -> auditable incident case
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

Security research additionally studies seL4-style minimization and formal assurance of selected trusted properties. Formal proof claims must always state their hardware, compiler/binary, cryptographic, and specification assumptions.

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

---

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

Architecture documents may preserve long-term LPAR and confidential-computing design, but those features are not current implementation work.

---

## 6. Definition of Done

Every milestone must satisfy all applicable items:

```text
[ ] concept and invariants documented
[ ] implementation builds without warnings
[ ] normal-path test passes
[ ] failure-path test passes
[ ] result is machine-checkable, not only UART text
[ ] observability evidence exists at the level currently supported by the project
[ ] known limitations are recorded
[ ] design/source documentation is updated
[ ] progress ledger is updated
[ ] commit or integration point is retained as evidence
[ ] current milestone does not regress earlier milestones
```

A future milestone may require stronger observability than an earlier one. Before M00-08, machine-checkable UART markers plus acceptance scripts are acceptable evidence; after the Structured Event ABI exists, new milestones should use it where applicable.

---

## 7. Learning and collaboration contract

The repository owner writes, debugs, and understands the core mechanisms to build real systems skill.

ChatGPT supports the work by:

- reading specifications, papers, and source references before each milestone;
- teaching the relevant concepts and checking understanding;
- helping derive data structures, state machines, invariants, and tests;
- providing complete reference code when useful, especially for syntax-heavy scaffolding;
- reviewing code and logs;
- isolating failures through evidence rather than guesses;
- maintaining roadmap, progress ledger, design records, and project context.

Each milestone should leave durable artifacts:

```text
code
machine-checkable tests
design/invariant record
known-limitations record
```

---

## 8. Developer workflow

The normal local workflow is documented in `docs/JIXIA_DEVELOPER_WORKFLOW.md`.

Primary commands:

```bash
bash scripts/setup-dev-env.sh --check
bash scripts/jixia.sh build
bash scripts/jixia.sh run --smp 4
bash scripts/jixia.sh debug --smp 4
```

Generic developer commands do not replace milestone-specific acceptance gates.

---

## 9. Roadmap maintenance

Update this file when the execution queue, phase order, working model, feature gates, or Definition of Done materially changes.

Routine test results and detailed milestone-completion evidence belong in `docs/JIXIA_PROGRESS.md` and the milestone design record.
