# ArchFW Hybrid RAS Architecture v0.1

## 1. Status and scope

This document defines the initial ArchFW RAS architecture derived from two reference models:

1. POWER Hostboot PRDF and the FIR -> Attention -> PRDF path.
2. x86/AMD-style Platform-First Error Handling using SMCA, ACPI APEI, HEST, GHES, CPER, BERT, MCE, LVT, SCI, NMI, PCIe AER, and CXL error records.

The design intentionally combines their strongest properties:

- preserve POWER-style topology-aware root-cause diagnosis and service resolution;
- use x86-standard ACPI APEI/CPER interfaces for OS and hypervisor delivery;
- keep interrupt, exception, and SMI paths minimal;
- continue normal workload execution on unaffected CPUs whenever hardware containment permits;
- stop only the smallest unsafe containment domain;
- support an optional out-of-band RAS agent, similar in role to an ARM SCP or an always-on management controller, for predictive and postmortem work.

This is an architecture proposal. It records design decisions, not a claim that current POWER or x86 firmware already implements every component exactly this way.

---

## 2. Core design statement

ArchFW RAS shall use:

```text
POWER-style internal diagnosis
    +
x86-standard external reporting
    +
containment-aware execution
    +
optional out-of-band recovery
```

The target system is:

```text
Hardware detector and containment
        -> minimal source-specific fast path
        -> normalized evidence event
        -> dedicated RAS executor
        -> topology/rule/plugin diagnosis
        -> action planner
        -> ACPI APEI/CPER and native exception delivery
        -> OS, hypervisor, BMC, and service tooling
```

The immediate interrupt path is a doorbell and evidence-preservation path. It is not the full diagnostic engine.

---

## 3. Reference model strengths

### 3.1 What ArchFW preserves from POWER PRDF

POWER PRDF contributes the internal diagnostic model:

- explicit hardware topology and diagnostic domains;
- hierarchical error propagation from local unit to chiplet and global summaries;
- separation between hardware error action and software diagnostic resolution;
- prioritized domain analysis;
- root-cause versus secondary-error discrimination;
- declarative rules for common register and bit relationships;
- plugins for complex algorithms;
- callout, threshold, GARD, deconfiguration, masking, reset, and repair policy;
- evidence-before-ACK ownership semantics;
- full analysis outside the immediate interrupt handler.

Its most important lesson is:

> The unit that reports an error is not necessarily the unit that caused it.

### 3.2 What ArchFW preserves from x86 Platform RAS

The x86/AMD-style reference contributes the hardware and OS cooperation model:

- SMCA bank identity using CPU topology, HardwareID, type, instance, status, address, syndrome, and miscellaneous registers;
- strong memory localization from normalized address to system address, channel, subchannel, DIMM, rank, bank, row, column, and sometimes DRAM device;
- native CPU execution-context delivery through MCE, corrected/deferred LVT, and related mechanisms;
- standard runtime reporting through HEST, GHES, and CPER;
- boot-time reporting through BERT;
- device reporting through PCIe AER, DPC, and CXL records;
- explicit firmware/OS ownership handoff such as MCA cloak, uncloak, consumption, and recloak;
- per-source hardware thresholds and per-rank or per-chip-select ECC counters.

Its most important lesson is:

> Internal evidence and recovery policy must be translated into stable standard interfaces understood by the OS and hypervisor.

### 3.3 The combined target

The desired result is not a PRDF clone and not a large SMM PlatformRas handler.

```text
AMD/x86 strength:
    precise reporter, address, FRU, execution context, standard OS delivery

POWER strength:
    causal graph, primary/secondary analysis, unified service resolution

ArchFW target:
    both
```

---

## 4. Non-goals

ArchFW shall not:

- perform full topology scans, address translation, CPER construction, PPR planning, or service callout logic in an SMI or machine-check fast path;
- assume that `reporter == root cause`;
- broadcast-stop all CPUs for every corrected or locally contained error;
- clear hardware evidence before it is durably snapshotted;
- use CPER as the internal diagnostic object model;
- make the out-of-band agent responsible for precise CPU exception recovery;
- require one permanently reserved global CPU if a per-socket or dynamically elected executor is more fault-tolerant.

---

## 5. Layered architecture

```text
+--------------------------------------------------------------+
| OS / Hypervisor / Service Consumers                          |
| MCE, LVT, SCI, NMI, HEST, GHES, CPER, BERT, ERST, Redfish    |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| Delivery and Recovery Adapters                               |
| page offline, process kill, VM inject, device reset, reboot  |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| Action Planner                                               |
| callout, threshold, GARD, deconfig, PPR, scrub, mask, reset   |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| POWER-style Diagnosis Engine                                 |
| topology graph, domains, rule graph, plugins, causal ranking  |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| Normalized Error and Evidence Model                           |
| reporter, suspected source, victim, address, syndrome, scope  |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| Event Fabric and Durable Snapshot Store                       |
| per-domain queues, crash-safe buffers, correlation, ownership |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| Minimal Fast-Path Source Adapters                             |
| FIR, SMCA, AER, CXL, DDR, IOMMU/SMMU, thermal, clock, power   |
+------------------------------^-------------------------------+
                               |
+------------------------------+-------------------------------+
| Hardware Detection, Retry, Poison, Isolation, Stop, Checkstop |
+--------------------------------------------------------------+

             +-----------------------------------+
             | Optional Out-of-Band RAS Agent    |
             | sideband access, trend analysis,  |
             | postmortem, repair, BMC reporting |
             +-----------------------------------+
```

---

## 6. Execution model: do not turn SMI into the diagnostic runtime

### 6.1 Minimal exception/interrupt/SMI fast path

The fast path may run on the source CPU, a firmware interrupt CPU, or a platform-designated handler. It shall do only work required to preserve correctness and evidence:

1. identify the reporting source and containment domain;
2. mask, cloak, freeze, or otherwise quiesce repeated notification;
3. capture volatile architectural and device evidence into a preallocated buffer;
4. assign a monotonic event ID and timestamp;
5. execute immediate hardware containment that cannot wait;
6. enqueue the event to the RAS executor;
7. either resume, park, isolate, or stop only the required scope.

The fast path shall not:

- walk the complete system topology;
- scan every CPU, PCIe endpoint, or CXL capability unless the hardware gives no smaller source indication;
- perform complex syndrome decoding;
- build final CPER records;
- query service databases;
- invoke BMC networking;
- run long repair procedures.

On x86, SMI delivery is implementation-dependent and many SMM designs rendezvous or serialize other CPUs. Even when the hardware does not architecturally broadcast every SMI, long SMM work can still perturb the whole machine. ArchFW therefore treats SMI as a secure capture bridge, not as the normal analysis engine.

### 6.2 Source-CPU capture requirement

Some evidence is local to the reporting CPU, for example per-core machine-check registers and exception state. A dedicated RAS core cannot always read another core's private MSRs directly.

Therefore:

```text
source CPU fast path
    -> capture local architectural state
    -> write immutable snapshot
    -> hand snapshot to RAS executor
```

The source CPU may then resume if the event is recoverable and restartable, or remain parked if its state is uncertain.

### 6.3 Dedicated RAS executor

Full diagnosis runs on a normal firmware/runtime execution context, not in SMI.

A practical deployment may use:

- one elected RAS worker thread for the whole system;
- one worker per socket or coherence domain;
- one reserved SMT thread instead of a complete physical core;
- a dynamically elected healthy CPU with failover;
- an out-of-band agent for classes that do not require host execution context.

The recommended initial implementation is:

```text
one coordinator
    +
one worker per socket or major containment domain
```

This is more fault-tolerant and scalable than a single global core. It still preserves the user's key requirement: unaffected business cores continue normal work while one bounded executor performs diagnosis.

### 6.4 Executor failover

The RAS executor itself is part of the failure model.

It must support:

- health heartbeat;
- lease-based ownership of event queues;
- failover to another healthy CPU or out-of-band agent;
- no single writable in-memory structure whose loss destroys all evidence;
- a watchdog that escalates if diagnosis does not complete within policy time.

---

## 7. Containment-aware stop policy

The system shall not decide stop scope only from severity labels such as corrected, uncorrected, or fatal. It must model whether integrity is still scoped.

Every event must carry at least:

```text
containment_scope
execution_context_valid
memory_data_integrity
coherency_integrity
address_scope_known
poison_contained
restartability
```

### 7.1 Recommended containment classes

| Class | Typical scope | Immediate action | Other CPUs |
|---|---|---|---|
| C0 | corrected/local telemetry | snapshot and queue | continue |
| C1 | thread/core-local recoverable | park or recover source thread/core | continue |
| C2 | channel/device/cluster contained | isolate affected unit/domain | continue outside domain |
| C3 | socket/die shared-resource uncertainty | quiesce affected socket/die | other sockets may continue if protocol permits |
| C4 | system coherency/common-fabric integrity lost | broadcast stop/checkstop all host CPUs | stop |
| C5 | host execution no longer trustworthy | hardware checkstop; out-of-band postmortem | host stopped |

### 7.2 Common bus and fabric errors

A shared-bus or common-fabric error should broadcast-stop all cores only when the hardware cannot prove that ordering, coherency, routing, or data integrity remains contained.

Examples that normally justify system-wide stop or checkstop include:

- coherence protocol state corruption with unknown ownership;
- directory or snoop-filter corruption whose affected lines are unknown;
- fabric routing-table corruption;
- uncontained poison or lost poison attribution;
- ambiguous system address decode across domains;
- retry/recovery failure on a non-redundant coherent link;
- global timebase/clock corruption affecting architectural ordering;
- common power or reset corruption;
- evidence that multiple domains may observe inconsistent memory state.

Examples that should not automatically stop every core include:

- corrected link CRC with successful retry;
- failure of one redundant route that hardware safely removes;
- a PCIe endpoint failure contained by DPC;
- a single memory channel disabled with address range isolation;
- a core-local cache recovery with no escaped bad data.

The rule is:

> Stop the smallest domain that makes continued execution provably safe. If safety cannot be proven, escalate to the next larger domain, up to system checkstop.

---

## 8. Normalized internal error model

CPER is an output format, not the internal truth model. ArchFW needs a richer canonical event representation.

```cpp
struct RasEvent
{
    EventId id;
    Timestamp timestamp;
    CorrelationId correlation;

    Target reporter;          // who detected or reported the error
    Target suspectedSource;   // current root-cause candidate
    Target victim;            // execution or device context affected

    ErrorClass errorClass;
    Severity severity;
    ContainmentScope containment;

    bool synchronous;
    bool preciseIp;
    bool restartable;
    bool dataPoisoned;
    bool poisonContained;
    bool coherencyLost;

    AddressEvidence address;
    SyndromeEvidence syndrome;
    TopologyPath propagationPath;
    EvidenceSet rawEvidence;

    LocalizationLevel localization;
    Confidence confidence;
    LifecycleState state;
    Owner owner;
};
```

### 8.1 Reporter, source, and victim are separate

Example:

```text
reporter        = UMC1
suspectedSource = DIMM3, later changed to DataFabric0
victim          = CPU17 load / physical page / VM42
```

This prevents the common error of replacing the part that first noticed corruption rather than the part that produced it.

### 8.2 Localization levels

ArchFW shall express result quality explicitly:

```text
EXACT
    exact physical address and FRU, possibly DRAM device

RANGE
    bounded address or topology range

CANDIDATE_SET
    one of several ranks/devices/links

REPORTER_ONLY
    detector known, source not established

INFERRED
    root cause inferred through propagation rules

UNKNOWN
```

Each result also carries confidence, for example HIGH, MEDIUM, LOW, or INDETERMINATE.

### 8.3 Preserve raw evidence

Raw register snapshots must remain available even after semantic decoding. Future firmware, service tools, or offline analysis may understand evidence that the current decoder does not.

---

## 9. POWER-style diagnosis engine

### 9.1 Topology and propagation graph

The topology graph includes ownership and connectivity:

```text
CPU thread -> core -> cluster -> socket -> coherent fabric
memory address -> controller -> channel -> subchannel -> DIMM -> rank
PCIe function -> switch -> root port -> IOMMU -> IO die -> fabric
CXL component -> link -> host bridge -> memory/fabric domain
```

It also includes causal propagation edges:

```text
source FIR/event A
    may produce secondary FIR/event B and C
```

### 9.2 Domain prioritization

The engine analyzes likely upstream causes before downstream reporters. For example, a fabric or PHY error may be checked before memory-controller and endpoint errors that it can induce.

### 9.3 Rule DSL and plugins

Declarative rules should express:

- register and bit relationships;
- source class and attention type;
- topology parent/child traversal;
- precedence and exclusion;
- reset and mask ownership;
- capture groups;
- common threshold and callout templates.

Typed plugins should implement:

- memory address translation;
- ECC syndrome and DRAM device decoding;
- temporal event correlation;
- causal ranking;
- PPR and sparing planning;
- device-specific recovery procedures;
- virtualization-aware action selection.

### 9.4 Explainable result

The diagnosis result should include a causal chain:

```text
PAU/Fabric first-error bit
    -> OMI link secondary error
    -> MCC poison reception
    -> UMC/OCMB victim report
    -> DIMM not called out as primary
```

This allows field service and developers to understand why the action planner selected one FRU over another.

---

## 10. Error ownership and lifecycle

POWER IPOLL mask/ACK and x86 MCA cloak/uncloak are instances of the same abstract transaction.

ArchFW shall implement:

```text
DETECTED
    -> QUIESCED
    -> SNAPSHOTTED
    -> QUEUED
    -> DIAGNOSED
    -> ACTION_PLANNED
    -> PUBLISHED
    -> ACKNOWLEDGED
    -> CLEARED
    -> REARMED
```

Ownership transitions may be:

```text
hardware
    -> fast-path adapter
    -> diagnosis service
    -> OS/hypervisor or out-of-band agent
    -> hardware rearmed
```

Rules:

1. notification may be masked before complete diagnosis;
2. evidence may not be cleared before a durable snapshot exists;
3. publishing CPER does not itself mean the OS has consumed it;
4. rearm occurs only after the responsible consumer acknowledges ownership;
5. new events arriving during analysis must receive a new sequence number or remain distinguishable from the in-flight event;
6. timeout and consumer death must have explicit escalation policy.

---

## 11. Northbound reporting: x86-standard ACPI APEI

The internal diagnosis engine produces a rich result. Delivery adapters translate it into standard interfaces.

### 11.1 Runtime

Use:

- HEST to declare error sources;
- GHES/GHESv2 error status blocks;
- CPER sections for processor, memory, PCIe, CXL, firmware, and OEM extensions;
- MCE for execution-context machine errors;
- corrected/deferred LVT where appropriate;
- SCI for recoverable platform notifications;
- NMI or fatal machine check only when required;
- native PCIe AER/DPC coordination where OS ownership is selected.

### 11.2 Boot and post-reset

Use:

- BERT for errors captured before OS runtime or after fatal reset;
- persistent event storage, potentially ERST or an implementation-defined journal;
- a correlation identifier connecting BERT, BMC logs, and prior runtime events.

### 11.3 Error injection

Use EINJ-compatible interfaces for test where possible, while retaining internal fault injection in RVSoC-Sim for propagation and containment validation.

### 11.4 Do not lose internal semantics

CPER may not represent every internal causal relationship. ArchFW should include:

- standard CPER sections for OS portability;
- optional OEM section containing event ID, causal chain, confidence, and internal topology identifiers;
- persistent full evidence referenced by the CPER event ID.

---

## 12. Optional out-of-band RAS agent

### 12.1 Role

An out-of-band RAS agent may run on an independent management core, a dedicated RISC-V microcontroller, BMC-side processor, or SCP-like always-on controller.

It is best suited for:

- corrected ECC trend monitoring;
- patrol scrub scheduling;
- rank/device threshold management;
- PPR and spare-row repair planning;
- long-running recovery procedures;
- persistent evidence journaling;
- postmortem analysis after host checkstop;
- health telemetry and Redfish/BMC reporting;
- watchdog and host-recovery orchestration;
- diagnosis fallback when host runtime services are unavailable.

### 12.2 What remains in-band

The out-of-band agent must not replace:

- precise CPU machine-check entry;
- source-thread register capture;
- OS page offlining;
- process or VM recovery;
- immediate coherent-fabric containment;
- actions that must complete before architectural execution resumes.

### 12.3 Hardware requirements

For the agent to remain useful during severe host failure, it should have:

- independent clock and reset domain;
- preferably independent power or always-on power;
- sideband access to relevant error registers;
- access to crash-safe SRAM or persistent journal;
- mailbox/doorbell communication with the host;
- selective reset, scrub, PPR, and isolation controls;
- no dependency on the same failed common fabric for all evidence access.

A management agent attached only through the failed coherent bus is not truly out-of-band.

### 12.4 Host-agent protocol

Use a versioned shared-memory or mailbox protocol:

```text
sequence number
message version
CRC or integrity tag
event ID
owner and lifecycle state
snapshot location
requested action
completion status
```

The protocol must be replay-safe, timeout-aware, and able to distinguish a restarted host from the previous boot instance.

### 12.5 Security

The agent is highly privileged and must use:

- signed firmware;
- least-privilege register access;
- authenticated host-agent messages where practical;
- memory protection preventing host corruption of trusted agent state;
- bounded DMA or no DMA for control messages;
- auditable recovery actions.

---

## 13. Example flows

### 13.1 Corrected DRAM ECC event

```text
DRAM CE
    -> hardware corrects data
    -> UMC/SMCA and per-rank counter record evidence
    -> minimal local notification
    -> source adapter snapshots status/address/syndrome/counter
    -> event queued to socket RAS worker
    -> worker translates address and decodes syndrome
    -> diagnosis correlates fabric, controller, and DIMM evidence
    -> action planner updates threshold
    -> CPER memory section published if policy requires
    -> out-of-band agent updates long-term trend
    -> counter and notification rearmed
```

Unaffected CPUs continue normal execution.

### 13.2 Uncorrectable memory load

```text
CPU load consumes UE
    -> source CPU enters MCE and captures precise context if available
    -> memory/fabric hardware contains poison
    -> source thread is parked or recoverable state is recorded
    -> event snapshot queued
    -> RAS worker diagnoses DIMM versus upstream fabric source
    -> CPER/GHES enriches native MCE
    -> OS offlines page, kills process, or injects error into guest
    -> long-term callout/PPR/GARD policy updated
```

Only the affected thread/core is stopped unless data or coherence containment is uncertain.

### 13.3 PCIe endpoint fatal error contained by DPC

```text
endpoint/AER error
    -> root port DPC contains hierarchy
    -> source adapter snapshots AER/DPC/requester data
    -> RAS worker correlates endpoint, switch, root port, IOMMU, and fabric
    -> CPER PCIe section and native AER policy delivered
    -> affected hierarchy reset or removed
```

Business cores continue.

### 13.4 Common coherent-fabric corruption

```text
coherency or routing integrity lost
    -> hardware broadcasts stop/checkstop
    -> per-core emergency state captured where possible
    -> host execution remains stopped
    -> out-of-band agent gathers global FIR/SMCA/fabric evidence
    -> POWER-style diagnosis determines primary source
    -> BERT/persistent record generated
    -> selective domain restart if provably safe, otherwise system reset
```

This is the class where all-core stop is correct.

---

## 14. Simulator requirements

RVSoC-Sim should model the architecture rather than shortcutting `fault -> handler`.

### 14.1 Required components

- local error detectors and state registers;
- hardware action/severity configuration;
- local, domain, and global aggregation;
- containment scope and escalation;
- mask/cloak/freeze and ACK/rearm behavior;
- per-domain event queues;
- dedicated RAS executor timing;
- OS delivery adapters;
- out-of-band agent and sideband path;
- raw evidence lifetime and overwrite/overflow behavior.

### 14.2 Required experiments

1. Local CE while all workload cores continue.
2. Core-local UE with only one core parked.
3. Memory-channel isolation while other channels continue.
4. PCIe DPC containment without CPU stop.
5. Common-fabric error causing system-wide broadcast stop.
6. RAS executor failure and failover.
7. Host checkstop with out-of-band postmortem.
8. Error storm causing notification masking and polling fallback.
9. Simultaneous upstream and downstream reports testing root-cause ranking.
10. CPER generation from a rich internal event without losing the full evidence journal.

### 14.3 Performance metrics

Measure:

```text
fast-path latency
number of CPUs interrupted or parked
business-core lost cycles
RAS queue depth
analysis latency
time to containment
time to OS notification
evidence loss or overwrite rate
false primary callout rate
recovery success rate
```

The architecture is successful only if it preserves diagnosis quality without turning every correctable error into a system-wide latency event.

---

## 15. Initial implementation roadmap

### Phase 1: Common model and synthetic sources

- implement `RasEvent`, `EvidenceSet`, lifecycle state, and target graph;
- implement local/core/socket/system containment scopes;
- add a synthetic FIR/SMCA source adapter;
- add a single RAS worker and deterministic event queue.

### Phase 2: POWER-style diagnosis

- build domain prioritization;
- implement declarative bit/register rules;
- add reporter/source/victim distinction;
- add causal-chain and confidence output;
- implement simple callout and threshold resolution.

### Phase 3: x86 delivery

- implement CPER builder separated from diagnosis;
- add GHES/BERT model;
- model MCE/LVT/SCI/NMI adapters;
- preserve internal event ID through all outputs.

### Phase 4: Containment and dedicated execution

- add source-core snapshot handoff;
- implement per-socket workers;
- add local park, domain quiesce, and global stop;
- add worker failover and watchdog.

### Phase 5: Out-of-band agent

- model independent management core and SRAM journal;
- add mailbox protocol and ownership transfer;
- implement corrected-ECC trend analysis and postmortem collection;
- add repair planning and BMC/Redfish output.

### Phase 6: Real platform adapters

- POWER FIR/IPOLL adapter;
- x86 SMCA/MCA adapter;
- PCIe AER/DPC adapter;
- CXL RAS adapter;
- IOMMU/SMMU and accelerator RAS adapters.

---

## 16. Architecture decisions recorded in v0.1

1. POWER-style root-cause analysis is the internal diagnostic foundation.
2. ACPI APEI and CPER are the primary x86 northbound reporting format.
3. SMI is a minimal capture/ownership bridge, not the full RAS engine.
4. Unaffected CPUs continue business execution whenever containment permits.
5. Full-system broadcast stop is reserved for uncontained common-fabric, coherency, ordering, clock, or power integrity failures.
6. A source CPU captures its own private architectural evidence before handing work to another core.
7. Full analysis runs on a dedicated/elected RAS executor, preferably per socket with coordinator and failover.
8. Reporter, suspected source, and victim are distinct fields.
9. Error ownership uses an explicit transactional lifecycle.
10. CPER is generated from, but does not replace, the richer internal event record.
11. An optional independent out-of-band RAS agent handles predictive ECC, long recovery, postmortem, persistence, and service integration.
12. The simulator must measure both diagnosis quality and workload disturbance.

---

## 17. Source study anchors

The design is informed by:

```text
POWER
    Hostboot PRDF rule and resolution framework
    Hostboot Attention Service
    skiboot hw/prd.c and PSI local error handling
    Linux OPAL-PRD and opal-prd/HBRT runtime path

x86/Hygon reference packages
    PlatformRas DXE/SMM
    MCA-to-SMI handling
    HEST/GHES/CPER construction
    SMCA collection and threshold configuration
    MCA cloak/uncloak ownership
    UMC address, syndrome, DIMM, rank, and ECC-counter handling
    PCIe/NBIO/CXL error paths
```

The detailed POWER notification study remains in:

```text
docs/ARCHFW_POWER_PRDF_FIR_ATTENTION_ROUTING_V0.1.md
```
