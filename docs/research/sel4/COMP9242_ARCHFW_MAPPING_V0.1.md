# COMP9242 / seL4 → ArchFW Design Mapping v0.1

## 1. Scope

This note converts the studied COMP9242 lectures and related L4/seL4 papers into concrete ArchFW design rules. It is not a general seL4 summary and does not imply that ArchFW will port or clone seL4.

ArchFW uses:

- Hostboot for firmware startup semantics;
- seL4/L4 for minimal kernel mechanisms and authority control;
- Microkit/LionsOS for static user-level component structure;
- multikernel research for future multi-die/agent scaling;
- NXP management firmware for hardware ownership and logical-machine policy.

---

## 2. Minimality

### Source lesson

L4/seL4 tolerates a concept in the kernel only when moving it outside would prevent required functionality. Hardware abstraction and general resource policy do not belong in a true microkernel.

### ArchFW decision

The M-mode kernel contains only:

- protected execution contexts;
- capability lookup and derivation primitives;
- address-space/PMP switching;
- protected procedure calls and replies;
- notifications;
- interrupt ownership and delivery;
- bounded scheduling mechanism;
- fault delivery;
- narrow machine-level operations.

The following stay in U-mode:

- Step Engine;
- Targeting/PlatformGraph;
- DDR, core/cache, PCIe/CXL services;
- HWP implementations;
- ACPI construction;
- RAS rule interpretation;
- logging/FFDC policy;
- module and product policy.

---

## 3. Protected procedure calls

### Source lesson

A seL4 Endpoint is best understood as a user-controlled protection-domain switch with argument/result transfer, not as a general data pipe. Atomic `Call` and `ReplyRecv` semantics avoid protocol races and unnecessary kernel transitions.

### ArchFW mapping

Use Endpoint/PPC for small synchronous operations:

```text
Step Engine -> Targeting query
Step Engine -> submit hardware transaction
Supervisor  -> service lifecycle operation
Service     -> privileged machine operation
```

Rules:

- request and result remain small;
- no bulk register dumps in message words;
- no waiting for long hardware training in the kernel call;
- long work returns a transaction ID;
- completion arrives through Notification and shared state;
- the server's client identity is carried by a badged capability or equivalent generated service handle.

### Reply authority

ArchFW v0.1 must preserve one-shot reply authority. Two implementation candidates remain:

1. explicit Reply Object, close to modern seL4 MCS;
2. one-shot reply capability embedded in the blocked caller TCB.

Whichever is chosen must guarantee that a server cannot forge a reply to an unrelated client.

---

## 4. Notifications

### Source lesson

Notification is an asynchronous synchronization object. Multiple signals are coalesced by OR-ing badges; it is not a queued message transport.

### ArchFW mapping

Notification is used for:

- interrupt delivery;
- timer expiry;
- mailbox doorbells;
- SBE/SCP/RAS-agent completion;
- Attention events;
- watchdog and service-death notification.

The receiver must drain the corresponding source queue or device status before rearming the source.

Badge allocation must be static and documented so a service can distinguish:

```text
IRQ bits
Agent-completion bits
Timer bits
Supervisor bits
```

---

## 5. Shared-memory queues and batching

### Source lesson

LionsOS uses isolated sequential components plus shared SPSC queues and notification-based wakeup. Batching naturally reduces protection-domain switches under load.

### ArchFW mapping

Use SPSC queues where ownership is unambiguous:

```text
Memory Service <-> SBE proxy
RAS source adapter -> RAS Engine
FFDC producer -> Log Service
AFRT -> management agent
```

Queue rules:

- one producer and one consumer;
- fixed-size descriptors or bounded payload regions;
- producer owns free entries;
- consumer owns published entries;
- release/acquire ordering is explicit;
- no locks in the common path;
- process all currently available entries before signalling the next component;
- notification means `work may be available`, never `exactly one item exists`.

---

## 6. Initial task and resource management

### Source lesson

seL4 gives initial authority and remaining machine resources to the initial task. After bootstrap, the kernel does not contain a general allocator or impose resource-management policy.

### ArchFW mapping

The kernel creates one Initial Platform Service and provides `ArchBootInfo`:

```text
memory ranges
object-pool bounds
initial capabilities
IRQ control
boot reason and epoch
platform/config image
agent-channel table
recovery manifest/evidence
payload locations
measurement state
```

The Initial Platform Service creates:

- Service Supervisor;
- Targeting Service;
- Step Engine;
- Memory/Core/PCIe services;
- RAS boot service;
- ACPI handoff builder;
- UEFI loader/transition service.

### v0.1 memory strategy

Do not copy the full Untyped/Retype model immediately. Use statically sized boot pools with explicit budgets:

```text
TCBs
Address spaces
Capability slots
Endpoints
Reply objects
Notifications
Frames/pages
Shared queues
```

Retain the important invariants:

- no unbounded kernel allocation after bootstrap;
- no global shared pool available to every service;
- service failure cannot consume all kernel objects;
- each object has a clear owner and teardown path.

---

## 7. Threads, events, and component structure

### Source lesson

Events reduce hidden concurrency and shared-state races; threads preserve natural control flow but make synchronization and deadlock part of every component. Microkit favors sequential event-driven protection domains.

### ArchFW decision

Default:

```text
one protection domain
one sequential service thread
one event loop
short handlers
```

Use a second thread only for:

- actual parallel hardware initialization;
- blocking legacy code that cannot be transformed safely;
- independent real-time deadlines;
- a measured performance bottleneck.

Step Engine is not one thread per substep. It is a deterministic state machine that issues operations and waits for explicit completion events.

Long operations store state in transaction objects, not hidden callback chains.

---

## 8. Kernel execution model and preemption

### Source lesson

seL4 uses an event-kernel style with explicit preemption points for long operations. This avoids a fully preemptible kernel and makes correctness/WCET reasoning tractable.

### ArchFW mapping

Kernel rules:

- enter on a fresh kernel stack or a clearly bounded per-hart stack;
- no blocking inside the kernel;
- no arbitrary callbacks from kernel to service code;
- interrupts disabled only for bounded sections;
- long kernel operations are incremental and restartable;
- explicit preemption points check pending urgent interrupts;
- user-level state is saved in the TCB;
- all failure exits leave kernel objects consistent.

Operations requiring incremental consistency include:

- capability-tree revoke;
- destruction of a protection domain;
- large PMP/address-space teardown;
- bulk object reset during recovery.

Hardware training and topology scanning are never kernel operations.

---

## 9. Scheduling

### Source lesson

seL4 MCS separates thread identity from the right to consume CPU time. Scheduling policy can be implemented at user level using priorities, budgets, periods, and timeout endpoints.

### ArchFW v0.1

Implement first:

- fixed priorities;
- strict highest-priority selection;
- round robin only within equal priority;
- timer-driven budget placeholder;
- no fairness goal during boot;
- watchdog/timeout notification.

Suggested priority classes:

```text
P0 emergency machine containment
P1 interrupt and fault delivery
P2 service supervisor
P3 active istep critical service
P4 normal hardware service
P5 logging and telemetry
P6 background validation
```

### Later MCS feature

Passive services may execute on a client's donated scheduling context:

```text
Step Engine Call -> Memory Service
Memory Service runs on caller budget
Reply returns budget
```

This should be added only after basic Endpoint/Reply semantics are correct and measured.

---

## 10. Capability model

### Source lesson

Capabilities combine an object reference with rights and support controlled delegation, attenuation, badging, and revocation. They avoid ambient authority and confused-deputy behavior.

### ArchFW object capabilities

Candidate capability classes:

```text
TaskCap
AddressSpaceCap
EndpointCap
NotificationCap
IrqControlCap
IrqHandlerCap
FrameCap
TargetCap
MmioCap
MachineOpCap
AgentChannelCap
BootStorageCap
RasEvidenceCap
```

Rights are object-specific. Examples:

```text
TargetCap: QUERY, INITIALIZE, DECONFIGURE, RESET
MmioCap: READ, WRITE, RMW, MAP
MachineOpCap: START_HART, STOP_HART, FENCE_DOMAIN, ENTER_PAYLOAD
RasEvidenceCap: READ_RECORD, MASK, CLEAR, REARM, SNAPSHOT
```

Do not expose `write_csr(number, value)` or unrestricted physical MMIO as generic machine operations.

### Capability generation

Capabilities should be generated from the static PlatformGraph and component manifest. The graph is the policy source; the kernel CSpace is the enforcement representation.

---

## 11. Fault delivery and service supervision

### Source lesson

seL4 delivers faults to a user-level fault endpoint. The kernel reports mechanism-level facts; user-level software decides recovery policy.

### ArchFW mapping

Fault flow:

```text
service exception
    -> kernel FaultEndpoint
    -> Service Supervisor
    -> capture software FFDC
    -> revoke service hardware capabilities
    -> notify Step Engine
    -> restart, retry, reconfigure, or fail boot
```

Keep these classes separate:

```text
software-service fault
    Service Supervisor

hardware-operation failure
    Step Engine + RAS policy

platform hardware error
    local containment + Common RAS Engine
```

The kernel must not interpret a DDR training failure as a hardware FRU diagnosis.

---

## 12. SMP, NUMA, and multikernel direction

### Source lesson

Shared kernel data and cache-line movement become scalability problems before raw lock contention does. Modern many-core systems should minimize sharing, communicate explicitly, and allocate/schedule for locality.

### ArchFW v0.1

- one boot hart executes the M-mode kernel and boot services;
- all other application harts begin parked;
- Core Service awakens, tests, and parks/releases them through isteps;
- no SMP kernel required for the first Linux boot;
- no globally writable Targeting database shared by every hart.

### Future scale-out

Use per-domain agents:

```text
system coordinator
socket/die initialization agent
local target shard
explicit transaction messages
immutable published snapshots
```

This is closer to a multikernel than an SMP firmware kernel. Replicate read-mostly topology and keep mutable ownership local.

---

## 13. Heterogeneous de-facto OS

### Source lesson

The real operating system of a modern SoC includes every privileged component that can initiate memory operations or configure translation and protection: CPU kernels, hypervisors, DMA devices, IOMMUs, management processors, firmware, and interconnect controllers.

### ArchFW PlatformGraph extension

Model:

- execution initiators;
- memory/address spaces;
- translation stages;
- configuration authorities;
- interrupt routes;
- DMA reachability;
- sideband management paths;
- security and RAS apertures.

This graph must answer:

```text
Can Agent A access memory region X?
Through which translation and interconnect path?
Who can change that path?
Can a reset or RAS action widen access?
Which component remains alive after host checkstop?
```

The same graph feeds capability generation, IOMMU setup, security review, and RAS diagnosis.

---

## 14. Performance methodology

### Required microbenchmarks

- M/U trap round trip;
- Endpoint Call/Reply;
- Notification signal/wait;
- IRQ to U-mode handler latency;
- address-space/PMP switch;
- shared SPSC enqueue/dequeue;
- capability lookup;
- fault delivery;
- reconfiguration-loop overhead.

### Required macrobenchmarks

- complete QEMU reset-to-UEFI time;
- reset-to-Linux-userspace time;
- istep trace length and time distribution;
- boot with injected recoverable fault;
- boot with deconfigured hart/memory/PCIe target;
- runtime RAS event delivery without host-wide stall.

### Reporting rules

- raw values: arithmetic mean plus variation;
- normalized ratios: geometric mean;
- always show tail/worst-observed latency;
- separate throughput from processing cost;
- test the model with measurements rather than claiming architectural overhead from call counts alone.

---

## 15. Explicitly rejected shortcuts

- `PlatformOps` as the architecture's primary abstraction;
- placing hardware abstraction and product policy in M-mode kernel;
- a general POSIX-like boot userspace;
- one thread per istep/substep;
- synchronous SBI calls that wait for hardware or RAS analysis;
- keeping the full boot microkernel active on customer harts at runtime;
- requiring a customer daemon or proprietary driver for basic RAS correctness;
- assuming BMC presence means FSP-class diagnosis capability;
- making Device Tree the server OS hardware contract;
- implementing a shared SMP firmware kernel before the single-boot-hart design works.

---

## 16. Practical takeaway

> The correct ArchFW synthesis is not `Hostboot rewritten on seL4`. It is a Hostboot-style deterministic firmware OS whose privileged mechanism layer follows seL4 minimality, whose user-level component graph follows Microkit/LionsOS, whose multi-die expansion follows explicit-message multikernel ideas, and whose hardware authority is generated from a whole-system PlatformGraph.
