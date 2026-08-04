# ArchFW Microkernel and Firmware Architecture v0.2

Status: architecture baseline for implementation

This document is the canonical integration point for the ArchFW branch. It combines:

- seL4-style minimal kernel mechanisms;
- IBM Hostboot-style istep, Targeting, HWP, HWAS and reconfiguration organization;
- NXP-style system-controller resource ownership, partitioning and mailbox RPC;
- an independent RAS management domain;
- EDK II and ACPI as the host operating-system handoff.

The design is not a port of seL4, Hostboot or NXP SCFW. It adopts mechanisms and architectural lessons while keeping an ArchFW-specific object model, boot transaction model and hardware-service model.

---

## 1. Frozen architecture statement

```text
Boot0 / immutable root
        |
        v
ArchFW microkernel
        |
        v
Initial Task / Root Orchestrator
        |
        +-- Targeting / PlatformGraph service
        +-- IStep engine
        +-- Service supervisor
        +-- Image and module service
        +-- Trace / FFDC service
        +-- RAS broker
        |
        +-- isolated hardware services
        |      +-- core/cache
        |      +-- memory
        |      +-- power/clock/reset
        |      +-- interrupt
        |      +-- PCIe/CXL
        |      +-- security
        |
        +-- remote agents
               +-- SBE/core agent
               +-- SCP/system-control agent
               +-- memory agent
               +-- RAS processor
               +-- BMC/management endpoint

Available Platform View
        |
        v
EDK II payload -> EFI memory map + ACPI -> Linux/hypervisor
```

The host boot environment is a small firmware OS. The kernel provides protection and communication mechanisms only. Platform policy, hardware initialization, diagnosis and operating-system description remain outside the kernel.

After Linux starts, boot-only services are terminated and their capabilities are revoked. The host retains only a minimal SBI/reset/timer/IPI/RAS handoff monitor. Long-lived platform management and full RAS diagnosis run on an independent SCP/RAS/BMC domain where available.

---

## 2. Design rules

1. **Mechanism stays in the kernel; policy stays in services.**
2. **A physical resource has one configuration writer at a time.**
3. **Capability answers “may this component access it?”; ownership answers “should this component control it now?”**
4. **Endpoint RPC, asynchronous notification and shared-memory transport are separate mechanisms.**
5. **The static platform model, runtime health state and OS-published topology are separate views.**
6. **An istep is a restartable transaction over targets, not a direct call into arbitrary platform code.**
7. **No volatile error evidence is cleared before an immutable snapshot exists.**
8. **Boot progress and reconfiguration are journaled by step, target, transaction and boot epoch.**
9. **The reporting component is not assumed to be the failing component.**
10. **The first QEMU implementation must preserve the production architecture even when hardware operations are simulated.**

---

## 3. Minimal microkernel object model

M00 implements exactly the following object classes.

| Object | Purpose | Minimum rights |
|---|---|---|
| `TCB` | user execution context, state and scheduler metadata | configure, read-state, write-state, suspend, resume |
| `CNode` | capability namespace | lookup, copy, mint, move, delete, revoke |
| `VSpaceRoot` | root of an address space and its page-table objects | map, unmap, protect |
| `Frame` | mappable RAM or device-memory frame | read, write, execute, map |
| `Endpoint` | synchronous request/reply IPC | send, receive, grant |
| `Reply` | one-use, unforgeable reply relationship | reply |
| `Notification` | asynchronous badge-bit event aggregation | signal, wait |
| `IRQHandler` | authority over one interrupt source | bind, acknowledge, mask, unmask |
| `UntypedRegion` | authority to derive kernel objects from a memory region | retype, revoke-descendants |

### 3.1 What is deliberately not a kernel object in M00

- A fault route is a TCB field naming an Endpoint, not a separate object.
- A timer is initially an IRQ source bound to a Notification, plus kernel scheduler accounting.
- Scheduling budget is stored in the TCB in M00; a first-class `SchedulingContext` is deferred.
- Target, istep, HWP, Agent, RAS case, ACPI table and log objects are user-space service objects.
- MMIO devices are represented by restricted device Frames and IRQHandler capabilities, not by kernel driver objects.

### 3.2 Object creation

The kernel does not perform general allocation after bootstrap.

```text
UntypedRegion
    --retype--> TCB / CNode / VSpace / Frame / Endpoint / Reply /
                Notification / IRQHandler
```

Retype validates:

- object type and size;
- alignment;
- destination CNode slot emptiness;
- region bounds;
- device-memory restrictions;
- absence of conflicting live descendants.

Pre-DRAM allocation uses fixed boot pools and a bump allocator. Once memory initialization is committed, DRAM is converted into normal and device `UntypedRegion` capabilities and managed by the Initial Task.

### 3.3 Capability space

M00 uses a one-level CNode to keep bootstrap and proof obligations small. The API preserves guarded lookup semantics so a two-level CSpace can be added without changing service interfaces.

Required operations:

```text
copy    preserve rights or reduce them
mint    reduce rights and attach a badge
move    transfer a capability slot
revoke  remove all descendants of a capability
remove  delete one capability reference
```

Capability derivation is recorded so that terminating a service can revoke its MMIO, IRQ, shared-memory and IPC authority as one subtree.

---

## 4. Kernel execution and scheduling model

### 4.1 Event-based kernel

The kernel uses one kernel stack per hart. A trap, syscall or interrupt enters a bounded kernel handler and returns to a selected user thread. The kernel never blocks on a user-level service.

This model is selected because it:

- keeps control flow structured;
- bounds kernel stack use;
- simplifies fault analysis and later verification;
- avoids retaining arbitrary in-kernel call chains across a context switch.

### 4.2 M00 scheduler

M00 is single-hart and fixed-priority:

```text
highest runnable priority wins
round-robin among equal priorities
reschedule on syscall block, reply, IRQ wakeup, timer tick or explicit yield
```

Secondary harts remain in a holding pen until the Root Orchestrator establishes the platform model, per-hart state and interrupt routes.

The kernel runs with interrupts disabled. Potentially long operations such as revoke or large unmap contain explicit preemption points. This follows the seL4 trade-off: controlled preemption points rather than a fully preemptible kernel.

### 4.3 Deferred scheduling features

Not in M00:

- SMP run queues;
- deadline or EDF policy;
- mixed-criticality scheduling-context donation;
- cross-hart load balancing;
- temporal partitioning and time protection.

The API reserves budget, period and deadline fields so these can be introduced after the single-hart semantics are stable.

---

## 5. IPC and fault model

### 5.1 Endpoint

Endpoint IPC is synchronous and provides backpressure.

```text
client Call(endpoint, request)
        |
        v
server ReplyRecv(endpoint)
```

A `Call` atomically sends a request, creates a reply relationship and blocks the caller. A `Reply` object is consumed by the response. The server cannot manufacture a reply to an unrelated caller.

Use Endpoint for:

- istep service operations;
- Targeting queries;
- image loading;
- power/clock/reset requests;
- Agent management commands;
- supervisor control.

### 5.2 Notification

Notification is asynchronous. Signals OR badge bits into a pending word and wake a waiter.

Use Notification for:

- IRQ delivery;
- completion doorbells;
- watchdog expiration;
- service death;
- RAS attention;
- timer wakeup.

Endpoint and Notification are not merged into a generic channel because request/reply ordering, blocking, event coalescing and acknowledgement semantics are different.

### 5.3 Bulk transfer

Large payloads use shared Frame capabilities plus Endpoint control messages. Ownership is explicit:

```text
producer owns buffer -> sends descriptor -> consumer owns buffer
consumer completes -> buffer capability/descriptor returns to pool
```

M00 does not rely on a type-safe-language ownership proof. Access is enforced by VSpace mappings, capability transfer and protocol state.

### 5.4 IRQ binding

```text
IRQControl authority
    --derive--> IRQHandler
    --bind--> Notification(badge)
```

The kernel delivers only a signal. The driver or Agent service reads device state and explicitly acknowledges/rearms the IRQ. Destroying the service revokes the IRQHandler and unbinds the Notification.

### 5.5 Fault IPC

Each TCB names a fault Endpoint. On a user-mode page fault, illegal instruction, capability fault or service exception:

1. the kernel freezes the faulting TCB;
2. it sends a structured fault message to the supervisor;
3. the message contains PC, cause, address, capability lookup information and thread identity;
4. the supervisor correlates it with service ID, current istep transaction, target, boot epoch and topology generation;
5. the supervisor resumes, restarts, terminates or escalates.

The kernel does not know that a DDR-training service fault requires an istep rollback. It only preserves isolation and reports the event.

---

## 6. Initial Task and service supervision

The kernel constructs an Initial Task with:

- a TCB;
- a root CNode;
- a VSpace containing its image, stack, IPC buffer and BootInfo;
- capabilities for untyped RAM and device regions;
- IRQ control authority;
- the kernel log/console bootstrap endpoint;
- the capability to start secondary harts later.

The Initial Task becomes the Root Orchestrator. It creates isolated services and delegates only the capabilities each service needs.

### 6.1 Supervisor contract

Every service has a manifest:

```text
ServiceId
image hash and version
entry point
memory budget
priority
fault endpoint
owned Endpoint/Notification objects
required MMIO/IRQ capabilities
owned and observed Target queries
restart policy
boot-phase lifetime
```

The supervisor maintains these states:

```text
CREATED -> STARTING -> READY -> RUNNING
                      |           |
                      v           v
                    FAILED <- STOPPING -> STOPPED
                      |
                      +-> RESTARTING / QUARANTINED / FATAL
```

A service restart creates a new service generation. Completions from an old generation are rejected.

---

## 7. IStep engine

ArchFW keeps Hostboot's `major.minor` operational language but replaces direct function-pointer dispatch with typed, restartable transactions.

### 7.1 Step descriptor

```cpp
struct StepDescriptor {
    StepId id;                    // major.minor
    StringId name;
    BootPhase phase;

    TargetQueryId target_query;
    Span<StepId> dependencies;

    ServiceId service;
    OperationId operation;

    CapabilitySetId required_caps;
    OwnershipRole required_owner;

    TimeoutPolicy timeout;
    RetryPolicy retry;
    FailurePolicy failure;
    AttentionPolicy attention;

    ConcurrencyClass concurrency;
    CheckpointClass checkpoint;
    StepId reentry_floor;

    FfdcSchemaId ffdc_schema;
};
```

### 7.2 Step execution

```text
SELECT TARGETS
    -> VALIDATE DEPENDENCIES
    -> ACQUIRE OWNERSHIP LEASES
    -> DISPATCH SERVICE/AGENT TRANSACTIONS
    -> WAIT FOR COMPLETIONS OR TIMEOUT
    -> COLLECT FFDC AND OBSERVED STATE
    -> VALIDATE RESULT
    -> COMMIT TARGET STATE DELTA
    -> JOURNAL CHECKPOINT
```

A step is successful only after its state delta and journal record are durable enough for the current boot phase.

### 7.3 Transaction identity

Every operation carries:

```text
BootEpoch
TopologyGeneration
ServiceGeneration
TransactionId
TargetId
TargetGeneration
StepId
IdempotencyKey
```

This prevents a reset Agent, restarted service or earlier boot from completing a current transaction.

### 7.4 Parallelism

A descriptor declares a concurrency class:

- `SERIAL_SYSTEM`: one globally;
- `SERIAL_DOMAIN`: one per power/coherence/RAS domain;
- `PARALLEL_TARGETS`: independent targets may execute concurrently;
- `BARRIER`: all prior work must commit before proceeding.

The engine does not infer independence from target type alone. The PlatformGraph dependency edges and ownership declarations determine safe parallelism.

### 7.5 Reconfiguration loop

When a step or RAS action deconfigures hardware:

```text
capture evidence
    -> propose Target State Delta
    -> apply health/deconfiguration overlay atomically
    -> revoke capabilities for unavailable resources
    -> stop or restart affected services
    -> recompute minimum boot topology
    -> find earliest invalidated checkpoint
    -> increment TopologyGeneration
    -> re-enter from computed checkpoint
```

Re-entry is computed from the dependency graph; arbitrary `goto istep` is forbidden. Retry and reconfiguration counts are bounded and journaled.

---

## 8. Targeting and PlatformGraph

The CUE platform description is a source language, not a runtime parser dependency.

```text
CUE source
   -> fwcfg compiler
      -> immutable Platform IR
      -> generated Target IDs and typed attributes
      -> Agent manifests
      -> simulator model
      -> ACPI-builder inputs
```

### 8.1 Three views

#### Static PlatformGraph

Immutable hardware facts:

- physical hierarchy;
- MMIO ranges and IRQ sources;
- power, clock and reset dependencies;
- coherent and non-coherent fabric paths;
- PCIe/CXL topology;
- memory-controller/channel/rank topology;
- Agent attachment;
- security and RAS containment domains;
- platform capabilities.

#### Runtime State Overlay

Boot- and run-time state:

```text
PRESENT
FUNCTIONAL
INITIALIZED
POWERED
DEGRADED
QUARANTINED
DECONFIGURED
OWNER
OWNER_EPOCH
LAST_ERROR
LAST_SUCCESSFUL_STEP
```

#### Published OS View

Only resources that can be safely handed to the OS:

- enabled harts;
- usable memory ranges;
- NUMA/cache topology;
- enabled PCIe/CXL roots and devices;
- interrupt and IOMMU relationships;
- firmware and RAS interfaces.

EFI memory maps and ACPI tables are generated from this final view. ACPI generation must not rescan physical hardware independently.

### 8.2 Relationship types

```text
CONTAINS
AFFINITY
POWER_DEPENDS_ON
CLOCK_DEPENDS_ON
RESET_DEPENDS_ON
IRQ_ROUTED_TO
DMA_BEHIND
COHERENT_WITH
MEMORY_BACKED_BY
CONTROLLED_BY
DIAGNOSED_BY
FAULT_CONTAINED_BY
PUBLISHED_AS
```

Persistent data and Agent messages use stable `TargetId` values and a generation; they never store native pointers.

---

## 9. Hardware service and Agent ownership

### 9.1 Four ownership roles

Every physical target has explicit roles:

| Role | Meaning |
|---|---|
| Lifecycle Owner | power, clock, reset and lifecycle transition authority |
| Configuration Owner | register programming and HWP execution authority |
| Diagnosis Owner | evidence collection, root-cause diagnosis and action planning |
| Workload Owner | OS, hypervisor or accelerator runtime using the resource |

Example:

```text
DDR channel
  lifecycle owner      = SCP
  configuration owner  = Memory Service / memory Agent
  diagnosis owner      = RAS Processor
  workload owner       = Host OS
```

A target has one lifecycle owner and one configuration writer at a time. Observers receive read-only evidence or telemetry capabilities.

### 9.2 Capability versus ownership

- A capability is kernel-enforced authority over an object, MMIO frame, IRQ or Endpoint.
- An ownership lease is a PlatformGraph policy record naming who may perform a lifecycle/configuration operation in a boot epoch.

Both checks are required for destructive operations.

### 9.3 NXP-derived resource-domain model

ArchFW borrows two distinct NXP ideas without conflating them:

1. System Controller Firmware style partitioning: resources, pads and memory regions are assigned to a partition; power/clock/reset control is centralized and exposed through RPC.
2. Management Complex style resource containers: a domain owns discoverable objects and invokes object-specific commands through a portal.

ArchFW generalizes these as:

```text
ResourceDomain
  member TargetIds
  MMIO windows
  IRQ set
  memory windows
  DMA/IOMMU domain
  lifecycle owner
  configuration owner
  diagnosis owner
  command endpoint
```

It does not copy NXP object names or assume that every platform has a hardware Management Complex.

---

## 10. Agent protocol

The semantic envelope is independent of mailbox, shared-memory ring, MCTP/PLDM or simulator transport.

```cpp
struct AgentEnvelope {
    uint16_t protocol_version;
    uint16_t service_id;
    uint16_t opcode;
    uint16_t flags;

    uint64_t transaction_id;
    uint64_t boot_epoch;
    uint64_t topology_generation;
    uint64_t target_id;
    uint32_t target_generation;
    uint32_t payload_length;
    uint32_t status;
    uint32_t reserved;
};
```

Transaction lifecycle:

```text
REQUESTED -> ACCEPTED -> IN_PROGRESS -> COMPLETED
       |          |             |
       +----------+-------------+-> FAILED / CANCELLED / STALE
```

Required statuses include:

```text
OK
RETRY_LATER
NOT_OWNER
NO_POWER
BUSY
BAD_VERSION
BAD_TARGET_GENERATION
STALE_BOOT_EPOCH
TIMEOUT
FAILED
```

Rules:

- no shared compiler-dependent C structs on the wire;
- all mutating operations are idempotent or explicitly non-replayable;
- responses echo transaction and epoch identity;
- Agent reset increments its generation and triggers reconciliation;
- bulk FFDC uses descriptors to shared immutable buffers;
- unsolicited events use a separate event queue and Notification/doorbell.

---

## 11. HWP model

A Hardware Procedure is a typed algorithm for one IP family. It runs inside the configuration-owning service or Agent.

An HWP may:

- read and write registers for its delegated targets;
- execute bounded polling and calibration loops;
- collect structured FFDC;
- report observed state and a local suggested action.

An HWP may not:

- change global boot flow;
- modify arbitrary PlatformGraph state;
- permanently GARD/deconfigure a target;
- publish ACPI;
- reset unrelated services;
- bypass lifecycle ownership.

The caller determines when and where the HWP runs. The RAS domain determines platform root cause and permanent action. The IStep engine determines retry, rollback or re-entry.

---

## 12. RAS split

The canonical path is:

```text
hardware containment
    -> local source adapter / immutable snapshot
    -> independent RAS Processor
    -> root-cause and action manifest
    -> HostFW RAS Broker
    -> Target overlay and istep reconfiguration
    -> OS/hypervisor recovery
```

### 12.1 Responsibility boundary

- **Local adapter:** minimum capture, mask/ack enough to stop storms, preserve source context.
- **RAS Processor:** topology-aware root cause, first-error correlation, thresholds, FRU/callout, persistent evidence, repair and reset plan.
- **HostFW RAS Broker:** authenticate/validate the manifest, update boot topology, revoke capabilities and trigger reconfiguration.
- **OS/hypervisor:** page, CPU, device, process and VM recovery.
- **BMC/fleet:** service history, inventory, repair workflow and fleet correlation.

HostFW does not implement a second full PRDF-like rule engine when an independent RAS Processor is present. A QEMU-only fallback diagnosis service may exist for M04 testing but is not the production ownership model.

### 12.2 Evidence lifecycle

```text
DETECTED -> QUIESCED -> SNAPSHOTTED -> QUEUED -> DIAGNOSED
         -> ACTION_PLANNED -> PUBLISHED -> ACKNOWLEDGED
         -> CLEARED -> REARMED
```

The source reporter, suspected source and victim are separate fields. Global stop is used only when containment cannot prove continued coherent execution safe.

---

## 13. Boot state machine

```text
RESET
  -> BOOT0
  -> KERNEL_BOOTSTRAP
  -> INITIAL_TASK
  -> PLATFORM_MODEL_READY
  -> MANAGEMENT_AGENTS_READY
  -> PRE_DRAM_ISTEPS
  -> DRAM_READY_TRANSITION
  -> PROTECTED_HOSTFW
  -> AVAILABLE_TOPOLOGY_FINALIZED
  -> UEFI_PAYLOAD
  -> LINUX_OR_HYPERVISOR
  -> RESIDENT_MINIMAL_MONITOR
```

### 13.1 BOOT0

Only:

- select boot hart;
- establish a temporary stack;
- initialize minimum UART;
- verify kernel, root-task and Platform IR images;
- establish minimum PMP/ePMP policy where available;
- jump to the kernel.

### 13.2 KERNEL_BOOTSTRAP

- establish trap vector and kernel VSpace;
- initialize interrupt controller and timer;
- create root CNode, root VSpace, Initial TCB and BootInfo;
- create idle thread;
- convert remaining boot memory to UntypedRegion capabilities;
- enter the Initial Task.

### 13.3 PRE_DRAM

Typical steps:

```text
agent discovery and protocol negotiation
power/clock/reset baseline
core/cache BIST
memory-controller discovery
DDR PHY initialization
DDR training
ECC enablement
minimum memory test
```

### 13.4 DRAM transition

Real hardware may initially execute from SRAM or contained cache. The transition is explicit and restartable:

```text
quiesce boot services
commit DRAM_READY
create DRAM UntypedRegions
rebuild service address spaces in DRAM
restart services with new generations
revoke temporary SRAM/cache mappings
```

QEMU simulates the hardware training but still performs the same logical commit and service-generation transition.

### 13.5 UEFI and Linux handoff

ArchFW provides EDK II with:

- final memory map and reserved regions;
- Published OS View;
- ACPI tables or ACPI-builder inputs;
- boot device and console descriptors;
- RAS shared regions and persistent journal handles.

The M03 acceptance target is an ACPI-described Linux boot. QEMU's generated FDT may be used as a bring-up oracle, but it is not the authoritative ArchFW platform contract and is not part of the final OS-visible interface.

---

## 14. QEMU `virt` implementation path

### 14.1 Machine baseline

- `qemu-system-riscv64 -machine virt`;
- one boot hart for M00/M01;
- `-bios none` so Boot0 starts in M-mode;
- pflash for immutable and mutable firmware regions;
- UART for early console;
- CLINT/ACLINT timer and software interrupts;
- PLIC first, AIA later;
- PCIe ECAM and virtio devices for M03;
- fw_cfg only as an optional transport, not as platform truth.

### 14.2 Image layout

```text
pflash0
  Boot0
  ArchFW kernel
  Initial Task
  immutable Platform IR
  service images and manifests
  EDK II payload

pflash1
  UEFI variables
  boot journal
  simulated RAS manifest/evidence
```

### 14.3 First services

```text
console
interrupt/timer
supervisor
Targeting
IStep engine
trace/FFDC
core/cache simulator
memory simulator
power/clock/reset simulator
PCIe service
RAS broker
ACPI/topology exporter
```

Simulated hardware still uses Endpoint calls, ownership checks and transaction identity. Direct test-only function calls into fake hardware are forbidden outside unit tests.

### 14.4 Fault injection

QEMU tests must inject:

- service page fault;
- IRQ storm before acknowledge;
- Agent timeout and reset;
- stale completion from an older boot epoch;
- DDR target failure causing reconfiguration;
- core BIST failure causing capability revocation;
- checkstop followed by next-boot deconfiguration.

---

## 15. Milestones M00-M04

### M00 — microkernel bootstrap

Deliver:

- Boot0 and early UART;
- event-based kernel and trap path;
- TCB, CNode, VSpaceRoot, Frame, Endpoint, Reply, Notification, IRQHandler and UntypedRegion;
- fixed-priority single-hart scheduler;
- Initial Task and BootInfo;
- fault IPC and supervisor restart demo.

Exit criteria:

- two isolated services complete synchronous RPC;
- a timer/IRQ arrives through Notification and requires explicit ACK;
- an illegal access faults only the service and the supervisor restarts it;
- revoked MMIO/IRQ capabilities become unusable.

### M01 — PlatformGraph and IStep engine

Deliver:

- generated QEMU Platform IR;
- Targeting service with static graph and runtime overlay;
- StepDescriptor and major.minor numbering;
- dependency, timeout, retry, FFDC and checkpoint journal;
- target queries and typed state deltas.

Exit criteria:

- deterministic boot trace through the pre-DRAM logical sequence;
- replay rejects an already committed non-replayable step;
- target query and dependency failures are diagnosed precisely;
- restart resumes from the last valid checkpoint.

### M02 — hardware services, Agents and reconfiguration

Deliver:

- memory, core/cache, PCR and PCIe services;
- Agent envelope and transport simulator;
- ownership leases;
- logical DRAM transition;
- service-generation restart;
- capability revocation on deconfiguration;
- dependency-based reconfiguration loop.

Exit criteria:

- non-owner destructive requests fail;
- stale Agent completion is rejected;
- injected core/cache failure removes the target and re-enters at the calculated checkpoint;
- minimum boot topology is enforced.

### M03 — EDK II, ACPI and Linux

Deliver:

- EDK II payload loading;
- EFI memory-map handoff;
- ACPI generation from Published OS View;
- Linux EFI boot;
- virtio or PCIe device enumeration.

Exit criteria:

- disabled targets are absent from ACPI;
- reserved firmware regions are not exposed as usable RAM;
- Linux enumerates CPUs, memory and PCIe from ACPI;
- the ArchFW platform model, not QEMU FDT, is the source of published topology.

### M04 — independent RAS and recovery

Deliver:

- simulated independent RAS Processor;
- first-error/evidence journal;
- authenticated action manifest;
- boot-time manifest replay;
- persistent target health;
- checkstop/reset and next-boot deconfiguration;
- OS-visible RAS record path.

Exit scenario:

```text
inject uncorrectable core/cache failure
  -> local containment and evidence snapshot
  -> RAS Processor diagnoses and persists manifest
  -> reset
  -> ArchFW validates manifest
  -> target is deconfigured and capabilities revoked
  -> affected isteps replay from computed checkpoint
  -> ACPI publishes reduced topology
  -> Linux boots successfully
```

---

## 16. Explicitly rejected designs

1. A large kernel containing Targeting, istep, drivers, ACPI and RAS policy.
2. A flat `PlatformOps` function table as the main portability layer.
3. One generic `Channel` object that merges RPC, IRQ and asynchronous events.
4. Multiple services with writable mappings to the same physical control registers.
5. Capability checks without lifecycle/configuration ownership checks.
6. A runtime CUE parser in early firmware.
7. Direct function-pointer isteps with hidden side effects.
8. Arbitrary reconfiguration jumps rather than dependency-derived checkpoints.
9. Clearing error registers before immutable evidence capture.
10. HostFW and the independent RAS Processor each running a full duplicate diagnosis engine.
11. SMI/SMM terminology for RISC-V error handling.
12. Complex UEFI Runtime or hidden M-mode services continuing normal hardware management after Linux boot.
13. Full SMP, MCS scheduling, time protection and formal verification in M00.
14. Copying NXP SCFW's small fixed RPC packet or DPAA2 object taxonomy verbatim.
15. Using QEMU FDT as the production platform model.

---

## 17. Source and code basis

### seL4 and microkernel mechanisms

- seL4 kernel source and reference manual: boot construction, TCBs, CSpace, VSpace, Endpoint, Reply, Notification, IRQ binding, Untyped/retype and fault IPC.
- Gernot Heiser, COMP9242 2025 lectures: microkernel minimality, seL4 mechanisms, execution models, SMP, security, timing and system construction.
- Jochen Liedtke, *Improving IPC by Kernel Design* and *On Kernel Construction*: IPC-driven design, minimal mechanisms and address-space construction.
- Wulf et al., *HYDRA*: generalized protected objects and separation of mechanism from policy.
- Saltzer and Schroeder, *The Protection of Information in Computer Systems*: least privilege, complete mediation, fail-safe defaults and least common mechanism.
- Singularity message communication: explicit ownership transfer and protocol state as a design reference; ArchFW does not assume a safe language.
- Blackham et al., seL4 WCET work: event-based kernel, bounded paths and explicit preemption points.

### IBM Hostboot

- open-power/hostboot `release-fw1120` source: istep dispatcher tables, module/task organization, Targeting service and associations, FAPI/HWP invocation, HWAS deconfiguration, PRDF/attention handling, minimum-hardware checks and reconfiguration loops.
- ArchFW retains the operational strengths of these subsystems but does not copy Hostboot's kernel, XML targeting source, module ABI or direct step-function execution model.

### NXP system-control and management designs

- i.MX System Controller Firmware documentation and Linux SCU client: resource partitions, power/clock/reset service ownership, Message Unit transport, versioned service/function RPC and explicit error statuses.
- DPAA2 Management Complex documentation and Linux fsl-mc model: resource containers, discoverable managed objects and command portals.
- Scope boundary: SCFW and Management Complex are related NXP management patterns, not one universal implementation. Platform-specific details require validation against the selected NXP SoC generation.

### Existing ArchFW branch

This document supersedes architectural contradictions in earlier v0.1 notes while preserving their useful work on CUE configuration, Agent identity, desired-state reconciliation, persistent state, RISC-V RAS and power/FIR routing.

---

## 18. Inference boundary

The following are architecture decisions made by ArchFW, not claims that an upstream project implements them exactly:

- the nine-object M00 kernel subset;
- capability plus ownership-lease dual authorization;
- transactional StepDescriptor;
- three-view PlatformGraph;
- boot-only service teardown after Linux starts;
- the M00-M04 partition;
- QEMU's simulated PCR/memory/core services;
- the exact Agent envelope and state machine;
- the split between independent RAS diagnosis and HostFW manifest execution.

Hardware-specific HWP behavior, actual DDR training, PMP/ePMP policy, Agent interconnect, power/clock/reset graph, RAS register layout and final RISC-V ACPI compatibility remain platform validation work.