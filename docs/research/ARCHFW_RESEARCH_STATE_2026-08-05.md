# ArchFW Research State — 2026-08-05

> Purpose: persist the current research state, architectural decisions, rejected directions, and next work items so the project can continue across conversations without reconstructing the reasoning from scratch.
>
> This file is a decision snapshot, not a replacement for the detailed architecture specifications under `docs/`.

## 1. Project goal

ArchFW is a cloud/server firmware architecture for heterogeneous RISC-V systems. It borrows proven ideas rather than cloning one existing project:

- IBM Hostboot: firmware-OS model, istep orchestration, Targeting, HWP separation, reconfiguration loop, HBRT, PRDF-style RAS;
- seL4/L4: minimal M-mode kernel, capabilities, protected procedure calls, notifications, fault endpoints, user-level policy;
- seL4 Microkit/LionsOS: static component graph, sequential event-driven protection domains, SPSC queues, batching;
- NXP System Manager/SCFW: resource ownership, logical machines, per-agent channels, power/clock/reset authority;
- OpenSBI: standard RISC-V machine-mode runtime and SBI services;
- EDK II: supervisor-mode UEFI implementation;
- ACPI/APEI: server hardware description and standard OS-visible RAS reporting;
- OpenPOWER PRDF/HBRT/opal-prd: common rule engine deployable in host runtime or service processor.

The first executable target is QEMU RISC-V `virt`, ending in an ACPI-based Linux boot.

---

## 2. Decisions that must not regress

### 2.1 Startup model

ArchFW startup is **not** a flat BSP with a `PlatformOps` function table.

The startup language is:

```text
Major IStep
    -> ordered Substeps
    -> service/agent transaction
    -> result + FFDC
    -> attention/RAS handling
    -> continue, degrade, retry, reconfigure, or stop
```

The first implementation should remain close to Hostboot's deterministic `major step + ordered substep + reconfiguration entry point` model. A free-form generic DAG may be added later, but must not replace deterministic IPL semantics in v1.

### 2.2 Platform model

All hardware initialization is driven by Targeting/PlatformGraph, not hard-coded platform functions.

The graph must represent more than physical hierarchy:

- target type and identity;
- containment and parent/child relations;
- ownership and execution agent;
- power, clock, reset, and coherency domains;
- address spaces and translation paths;
- MMIO and interrupt ownership;
- lifecycle and health state;
- physical, available, and published topology;
- RAS reporter, source, victim, and propagation relations.

### 2.3 Privilege split

Boot phase:

```text
M-mode: ArchFW-MK kernel
U-mode: ArchFW boot services
S/HS-mode: EDK II UEFI
S/HS-mode: Linux or hypervisor
U/VS/VU: host and guest userspace
```

`ArchFW is M-mode` means the minimal kernel and machine runtime own M-mode. It does **not** mean DDR training, PCIe initialization, Targeting, istep, ACPI construction, or the full RAS rule engine belong inside the kernel.

### 2.4 OS contract

The product server contract is:

```text
Boot interface: UEFI
Hardware description: ACPI only
Memory discovery: EFI Memory Map
Low-level machine services: SBI
RAS reporting: APEI/HEST/GHES/CPER/BERT/ERST as applicable
```

Device Tree is not the product-level OS hardware-description contract. A minimal synthetic FDT used internally by an EFI stub does not change this rule.

### 2.5 Runtime transition

The complete boot microkernel exists only during the boot-control phase.

After isteps finish:

```text
stop Step Engine
quiesce hardware services
freeze/persist runtime topology summary
revoke boot-only capabilities
terminate boot U-mode services
lock runtime PMP and interrupt ownership
activate AFRT/OpenSBI personality
enter UEFI
```

At runtime, the application harts retain a small M-mode environment:

- OpenSBI core services;
- machine trap and Hart lifecycle;
- system reset, IPI, RFENCE, TIME where required;
- bounded RAS capture and forwarding;
- secure update and emergency containment interfaces;
- small runtime state and event transport.

The runtime must not preserve a hidden general-purpose firmware scheduler that steals unbounded time from customer harts.

### 2.6 Common RAS engine

A dedicated RAS processor is a deployment choice, not a prerequisite for ArchFW RAS.

The same Common RAS Engine and rule source must support:

- RAS-processor primary;
- host-runtime primary;
- capable external service-processor primary;
- boot-time host fallback;
- offline analysis and replay.

A normal BMC is not automatically equivalent to an IBM-style FSP. It may lack sideband access, internal error visibility, reset independence, and the ability to execute topology-aware rules.

---

## 3. Boot microkernel model

### 3.1 Kernel responsibility

The M-mode kernel provides mechanisms only:

- trap entry and return;
- context switching and fixed-priority scheduling;
- address-space/PMP management;
- capability spaces;
- Endpoint protected procedure calls;
- Reply objects or equivalent atomic call/reply semantics;
- Notification objects;
- IRQ control, IRQ-handler objects, and binding to notifications;
- fault endpoints;
- bounded machine operations;
- object pools and initial-task bootstrap;
- transition to the runtime SEE personality.

The kernel does not contain:

- hardware abstraction policy;
- istep ordering;
- Targeting policy;
- HWP implementations;
- DDR/PCIe/cache initialization policy;
- ACPI generation;
- complete RAS diagnosis;
- BMC protocol stacks.

### 3.2 First user-level task

The kernel creates one `Initial Platform Service` and supplies an `ArchBootInfo` structure containing:

- free and reserved memory ranges;
- initial capability slots;
- IRQ control authority;
- boot reason and boot epoch;
- immutable platform/configuration image;
- agent-channel descriptors;
- boot recovery manifest or persisted evidence;
- payload locations and secure-boot measurements.

The Initial Platform Service creates and supervises all remaining boot services.

### 3.3 Initial kernel object set

The first practical object set is:

```text
TaskControlBlock
AddressSpace
CapabilitySpace
Endpoint
ReplyObject
Notification
IrqHandler / IrqBinding
MemoryRegion or Frame
FaultEndpoint
SchedulingContext placeholder
```

The first version may use build-time object pools rather than a full seL4 Untyped/Retype implementation. The important retained principle is that the kernel does not perform unbounded allocation after bootstrap and each protection domain has a bounded resource budget.

### 3.4 Endpoint, Notification, and shared-memory roles

```text
Endpoint / PPC
    short synchronous control operations
    request/reply
    small arguments and results

Notification
    IRQ
    mailbox doorbell
    timer
    attention
    watchdog
    asynchronous completion

Shared SPSC queue
    FFDC
    register batches
    training data
    agent messages
    large results
```

Do not use synchronous kernel entry to wait for long hardware work. Long operations return a transaction ID and complete asynchronously.

### 3.5 Execution model

The preferred boot-service model is static and event driven:

- one sequential thread per protection domain by default;
- short handlers run to completion;
- no implicit shared-state concurrency inside a service;
- true parallelism only where it brings measurable startup benefit;
- batch all pending queue work before signalling the next component;
- never busy-poll in the normal path;
- use threads only for actual CPU parallelism or unavoidable blocking structure.

---

## 4. Hostboot semantics retained

### 4.1 Istep descriptor

An ArchFW substep descriptor should include:

- major and minor step ID;
- name and firmware progress code;
- IPL mode applicability;
- target query/scope;
- required service and operation;
- service/module dependencies;
- required capabilities;
- timeout and retry budget;
- attention policy;
- failure policy;
- reconfiguration resume point;
- FFDC schema;
- secure-measurement identity/version.

### 4.2 HWP separation

Keep four layers separate:

```text
HWP / hardware procedure
    how to operate one IP block

Service or Agent
    owns access, serialization, and transport

IStep
    when and on which targets to invoke it

Policy/RAS
    what a failure means and what action follows
```

### 4.3 Reconfiguration loop

Reconfiguration is a first-class control-flow mechanism, not a reboot-shaped `goto`.

A decision includes:

- reason and evidence ID;
- targets deconfigured or restored;
- topology generation;
- resume major/minor step;
- retry count and maximum budget;
- action owner and diagnosis owner;
- whether ACPI-visible topology changed.

The QEMU proof case should inject a cache-BIST failure, deconfigure one hart, re-enter the appropriate step, and boot Linux with the reduced hart count in ACPI.

---

## 5. Agent and ownership model

Each target must distinguish:

```text
Lifecycle owner
Register owner
Power/clock/reset owner
RAS diagnosis owner
Execution agent
Logical machine / security domain
```

An istep never gains ambient authority to all hardware. It invokes a service through a capability. The service either performs a local HWP or routes the operation to SBE/SCP/RAS/BMC via a versioned transaction protocol.

Required protocol concepts:

- agent identity;
- boot epoch;
- transaction ID;
- accepted/progress/completion;
- timeout and retry-later;
- ownership epoch;
- capability and protocol version discovery;
- idempotency and duplicate suppression;
- immutable FFDC/evidence references.

---

## 6. RAS architecture snapshot

### 6.1 Global shape

```text
Local hardware recovery
    -> immutable source snapshot
    -> domain RAS controller or host M-mode capture
    -> Common RAS Engine
    -> action planner
    -> AFRT / Linux / hypervisor / BMC execution
    -> persistent evidence and next-boot manifest
```

The RAS processor should have a global view of **error evidence**, not unrestricted access to host data, guest memory, keys, or arbitrary execution state.

### 6.2 RAS access aperture

Each IP exposes a capability-scoped RAS window containing only:

- FIR/RERI/error records;
- first-error and WOF state;
- syndrome and error address;
- requester/transaction/ASI identifiers;
- retry and recovery result;
- counters and threshold state;
- frozen local trace;
- controlled mask, clear, rearm, fence, reset, and deconfigure operations.

Core-private state that cannot be read sideband must be copied into an immutable hardware snapshot window before the core is parked or reset.

### 6.3 Deployment profiles

#### `RSP_PRIMARY`

Dedicated always-on RAS RISC-V core runs the full Common RAS Engine. Host retains boot/runtime fallback and supplies execution-context information.

#### `HOST_PRIMARY`

No FSP-class processor exists. A reserved firmware hart is preferred; otherwise AFRT invokes a signed HBRT-like runtime capsule with strict time budgets and asynchronous slicing.

#### `EXTERNAL_SP_PRIMARY`

A BMC/service processor may be primary only if it has secure sideband access, persistent operation across host reset, sufficient internal error visibility, rule execution capacity, and controlled repair/deconfiguration authority.

At any moment exactly one diagnosis owner may finalize an event, clear evidence, persist GARD/deconfiguration, and issue the final callout.

### 6.4 Boot versus runtime policies

Common code:

- RAS rule IR/interpreter;
- topology schema;
- source adapters;
- causal analysis;
- threshold/callout/action-intent formats;
- replay tests.

Boot policy:

- analyze previous checkstop/reset;
- apply persistent deconfiguration;
- rebuild available topology;
- choose reconfiguration entry point;
- decide whether boot can continue.

Runtime policy:

- bound host latency;
- isolate page/CPU/device/domain;
- publish CPER/GHES;
- defer complex repair to the management domain;
- persist next-boot actions.

---

## 7. QEMU implementation path

### M00 — Machine kernel alive

- RISC-V M-mode reset entry;
- early console;
- trap and timer;
- one kernel stack per hart or event-kernel design decision;
- TCB/context switch;
- enter U-mode Initial Platform Service;
- minimal capability lookup;
- Endpoint and Notification smoke tests.

### M01 — Static component system

- build-time component manifest;
- address spaces/PMP regions;
- initial capability distribution;
- fault endpoint and supervisor;
- SPSC shared queues;
- IRQ-to-notification binding.

### M02 — Targeting and istep engine

- static PlatformGraph image;
- major-step/substep registry;
- service dependency lifecycle;
- FFDC and progress log;
- timeout/retry/failure policies;
- reconfiguration loop.

Initial QEMU substeps:

```text
early console
platform discovery
start agents
clock/reset stub
cache BIST stub
DDR initialization stub
interrupt-controller initialization
PCIe root-complex stub
topology finalization
```

### M03 — Runtime transition and UEFI

- ArchFW boot-to-runtime personality transition;
- integrated OpenSBI core;
- S/HS-mode EDK II payload;
- EFI Memory Map;
- ACPI-only platform description;
- Linux EFI boot.

### M04 — RAS closed loop

- QEMU custom RAS MMIO source;
- FIR/RERI-like records and first-error journal;
- injected BIST/runtime errors;
- Common RAS Engine;
- deconfiguration and istep re-entry;
- ACPI topology reflects degraded resources;
- GHES/CPER/BERT path.

---

## 8. Performance and verification rules

- Kernel hot paths must be O(1) or explicitly preemptible at safe points.
- No long hardware waits or rule scans in M-mode trap/ecall context.
- No function pointers or unbounded loops in verification-sensitive kernel paths unless justified.
- Use a fresh kernel stack on trap if the event-kernel design is retained.
- Measure IPC, notification, address-space switch, interrupt latency, istep overhead, and end-to-end boot time.
- Microbenchmarks diagnose mechanisms; macrobenchmarks assess the complete architecture.
- Report raw metrics with arithmetic means and normalized ratios with geometric means.
- Record mean, variance, tail latency, and worst-observed path; do not call throughput loss directly 'overhead' without a processing-cost model.
- Compare against a simple direct-call baseline and OpenSBI/EDK II reference boot.

---

## 9. Source material already studied

The raw PDFs are intentionally not stored in this repository. The following local source set has informed the current design:

### COMP9242 2025 lectures

- `01a-intro.pdf` — microkernel minimality, PPC, notifications;
- `01b-sel4.pdf` — capabilities, CSpace, Untyped/Retype, threads, IRQ/fault handling;
- `02a-threadsevents.pdf`, `02b-threadsevents.pdf` — event, coroutine, continuation, and thread execution models;
- `03a-hw.pdf`, `04b-smp.pdf` — cache/TLB/device behavior, memory ordering, multicore locking;
- `03b-vms.pdf` — virtualisation and privilege boundaries;
- `05a-rts.pdf` — real-time scheduling and criticality;
- `07a-perf.pdf`, `07b-sec.pdf` — performance method and security/capability principles;
- `08a-multiproc-1.pdf`, `09a-multiproc-2.pdf` — scalability, reduced/no sharing, multikernel and heterogeneous de-facto OS;
- `08b-uk.pdf` — L4/seL4 API and implementation evolution;
- `10a-sel4.pdf`, `10b-local.pdf` — verification, Microkit, LionsOS, performance and system structure.

### Papers

- Liedtke, *Improving IPC by Kernel Design*;
- Liedtke, *On Kernel Construction*;
- Härtig et al., *The Performance of µ-Kernel-Based Systems*;
- Saltzer and Schroeder, *The Protection of Information in Computer Systems*;
- Blackham et al., *Timing Analysis of a Protected Operating System Kernel*;
- Ge et al., *Time Protection: The Missing OS Abstraction*;
- Singularity message/channel contracts;
- TinyOS event-driven architecture;
- AEGIS secure and recoverable bootstrap;
- Mach/Ultrix memory-system comparison;
- virtual-cache consistency papers;
- benchmark-statistics methodology.

### Firmware/source trees

- OpenPOWER Hostboot `release-fw1120`;
- OpenPOWER HBRT/PRDF/opal-prd paths;
- OpenSBI ecall/runtime structure;
- NXP System Manager/SCFW resource and agent model;
- current `Pedroaliu/Firmware` architecture documents.

---

## 10. Open questions

The following items are not yet frozen:

1. Exact RISC-V protection mechanism for U-mode boot services: PMP-only, M-mode-managed S-mode page tables, or a hybrid.
2. Whether v1 implements a separate Reply Object or embeds one-shot reply authority in the caller TCB.
3. How much of seL4 MCS scheduling-context donation is needed for the first working boot.
4. Exact static component-description format and code generator.
5. How EDK II consumes the ArchFW handoff: Universal Payload, custom PEI entry, or a smaller initial integration.
6. Which ACPI tables QEMU M03 must generate directly versus initially borrow from QEMU/EDK II.
7. Common RAS rule language/IR format and plugin boundary.
8. Reserved firmware-hart policy for `HOST_PRIMARY` runtime RAS.
9. How capability revocation maps to MMIO/PMP teardown without excessive boot latency.
10. SMP strategy after v1: single boot hart, per-die agents, or a true SMP kernel profile.

---

## 11. Immediate next work

1. Freeze `ArchFW-MK v0.1` object model and syscall surface.
2. Define the static component manifest and generated capability graph.
3. Specify `ArchBootInfo`, the Initial Platform Service, and boot memory pools.
4. Specify Endpoint/Reply/Notification and IRQ semantics in enough detail to implement QEMU M00.
5. Define the first Hostboot-style major steps and substeps.
6. Define the boot-to-AFRT/OpenSBI transition state machine.
7. Create the Common RAS Engine deployment and ownership specification.
8. Start QEMU implementation only after items 1–4 are reviewed.

---

## 12. One-sentence architecture

> ArchFW is a Hostboot-style deterministic firmware operating system that uses a minimal seL4-inspired M-mode kernel and static U-mode services during boot, then contracts into an OpenSBI-based M-mode runtime, while a common topology-aware RAS engine can execute on a dedicated RAS core, a capable service processor, or the host runtime without changing its rule source.
