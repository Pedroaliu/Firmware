# ArchFW RISC-V RAS Architecture v0.2

## 1. Correction to v0.1

RISC-V has no architectural equivalent of x86 SMM/SMI. ArchFW must therefore not describe its fast path as an SMI handler.

The RISC-V design uses:

- machine-mode trap handling for architectural CPU-local error capture;
- resumable NMI (Smrnmi) or platform-defined NMI for urgent, non-maskable errors;
- local or external interrupts for lower-priority RAS notification;
- RAS Error Record Register Interface (RERI) or source-specific MMIO/CSR error records;
- one or more dedicated RAS worker harts for complete diagnosis;
- an optional out-of-band RAS microcontroller for predictive, repair, and postmortem work.

ACPI APEI, HEST, GHES, CPER, BERT, and ERST remain northbound reporting formats when the RISC-V server platform and OS use ACPI. They do not imply the presence of SMM or SMI.

---

## 2. Core design statement

ArchFW RAS shall combine:

```text
POWER-style topology and root-cause diagnosis
    +
RISC-V-native trap, interrupt, RNMI, and RERI mechanisms
    +
ACPI APEI/CPER standard reporting where applicable
    +
containment-aware continued execution
    +
optional out-of-band RAS processing
```

The normal path is:

```text
Hardware detector
    -> source error record / RERI record
    -> hardware containment and severity classification
    -> local interrupt / external interrupt / M-mode trap / RNMI
    -> minimal source-hart capture
    -> immutable event snapshot
    -> RAS worker hart
    -> POWER-style topology/rule/plugin diagnosis
    -> action planner
    -> OS/hypervisor delivery and service logging
    -> clear, acknowledge, and rearm
```

No full diagnostic engine runs in the immediate trap or interrupt path.

---

## 3. RISC-V notification classes

### 3.1 Low-priority RAS event

Examples:

- corrected ECC threshold;
- predictive link or device degradation;
- corrected cache/fabric error with successful retry;
- non-urgent telemetry.

Recommended delivery:

```text
RERI low-priority signal
    -> local or external interrupt
    -> designated RAS worker hart
```

The source does not need to interrupt every application hart.

### 3.2 High-priority recoverable RAS event

Examples:

- source-hart architectural state must be captured immediately;
- local core or cluster must be parked;
- execution may resume only after bounded containment.

Recommended delivery:

```text
source-local M-mode trap or RNMI
    -> capture private architectural state
    -> park/recover source hart
    -> enqueue immutable snapshot
    -> RAS worker performs full diagnosis
```

Smrnmi is useful when the event must be non-maskable but resumable. The handler must still remain small.

### 3.3 Fatal common-fabric event

Examples:

- coherency directory corruption with unknown ownership;
- fabric routing corruption;
- uncontained poison or lost poison attribution;
- system address decode ambiguity across domains;
- non-redundant coherent-link recovery failure;
- global clock/reset/power integrity loss.

Recommended action:

```text
hardware broadcast stop/checkstop
    -> freeze all affected harts
    -> preserve per-hart and fabric evidence
    -> out-of-band RAS agent or reliable management domain performs postmortem
```

A common-bus report does not automatically mean global stop. Global stop is required only when continued coherent execution cannot be proven safe.

---

## 4. Execution model

### 4.1 Source-hart fast path

The source hart performs only operations that cannot be delegated:

1. save `pc`, privilege state, trap cause, and implementation-defined machine-check state;
2. snapshot private error CSRs or hart-local error records;
3. mask, freeze, or acknowledge only enough to prevent repeated entry;
4. execute mandatory immediate containment;
5. publish an immutable event descriptor;
6. resume, isolate, or park according to containment state.

The source hart shall not perform topology scans, address translation, syndrome decoding, CPER construction, PPR planning, BMC communication, or long recovery procedures.

### 4.2 Dedicated RAS worker hart

Full diagnosis is performed by one or more normal RAS worker harts.

Recommended topology:

```text
one system coordinator
    +
one worker per socket, die, or major coherence domain
```

A worker may be:

- a reserved hardware thread;
- a reserved hart;
- a dynamically elected healthy hart;
- a firmware/runtime service scheduled by the OS or monitor.

Unrelated business harts continue executing whenever hardware containment permits.

### 4.3 Why not one permanent global RAS hart only

A single global hart is simple but creates a failure dependency. A per-domain worker model provides:

- lower cross-socket latency;
- less dependence on a potentially failed fabric path;
- easier containment-domain ownership;
- failover when one worker or socket is unavailable.

The coordinator assigns correlation IDs and resolves cross-domain events; local workers own capture and domain-specific analysis.

---

## 5. RERI and source adapters

RERI provides a standard memory-mapped error-record interface, but the physical notification signal is platform-defined. ArchFW shall treat RERI as one source adapter among several.

```text
Source adapters
    FIR-like SoC records
    RERI records
    hart-local cache/core records
    memory-controller ECC records
    PCIe AER / DPC
    CXL component records
    IOMMU error records
    clock, power, thermal, and reset records
```

Each adapter implements the same lifecycle:

```text
quiesce()
capture()
classify_initial_scope()
clear()
rearm()
```

The physical signal may be routed to:

- a designated application hart;
- an M-mode firmware hart;
- a local interrupt;
- an external interrupt controller;
- RNMI/NMI;
- a dedicated RAS microcontroller;
- hardware-only containment logic.

---

## 6. Internal diagnosis model

CPER is an output format, not the internal diagnostic truth model.

```cpp
struct RasEvent
{
    EventId id;
    CorrelationId correlation;
    Timestamp timestamp;

    Target reporter;
    Target suspectedSource;
    Target victim;

    ErrorClass errorClass;
    Severity severity;
    ContainmentScope containment;

    bool synchronous;
    bool precisePc;
    bool restartable;
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

The model separates:

- reporter: the component that detected the error;
- suspected source: the current root-cause candidate;
- victim: the hart, page, VM, device, or transaction affected.

This preserves POWER PRDF's central lesson: the reporting unit may be a downstream victim rather than the failed FRU.

---

## 7. POWER-style diagnosis engine

The diagnosis engine retains:

- topology and propagation graph;
- domain ordering and prioritization;
- primary versus secondary-error analysis;
- declarative register/bit rules;
- typed plugins for complex algorithms;
- callout, threshold, deconfiguration, repair, and reset policy;
- evidence-before-acknowledge semantics.

Example causal graph:

```text
Fabric first-error
    -> memory-controller poison reception
    -> DIMM-path reporter
    -> CPU load victim
```

A local DIMM-path report is not called out as primary when an upstream first-error record explains it.

---

## 8. Containment policy

| Class | Scope | Immediate action | Unaffected harts |
|---|---|---|---|
| C0 | corrected/local telemetry | snapshot and queue | continue |
| C1 | hart/core-local recoverable | recover or park source hart | continue |
| C2 | channel/device/cluster contained | isolate affected unit | continue outside domain |
| C3 | die/socket shared-resource uncertainty | quiesce affected domain | continue elsewhere if protocol permits |
| C4 | system coherency/common-fabric integrity lost | hardware broadcast stop | stop |
| C5 | host no longer trustworthy | out-of-band postmortem/reset | stopped |

The decision is based on containment proof, not only on severity labels.

---

## 9. Northbound reporting

When the RISC-V server platform uses ACPI, ArchFW may publish:

- HEST error sources;
- GHES/GHESv2 status blocks;
- CPER processor, memory, PCIe, CXL, firmware, and OEM sections;
- BERT for boot-time evidence;
- ERST for persistent records.

Native delivery remains source-specific:

- synchronous machine exception or implementation-defined machine-check trap for execution-context errors;
- RNMI for urgent resumable non-maskable capture;
- local/external interrupt for corrected or deferred platform events;
- fatal stop/checkstop for uncontained coherence loss.

APEI is a reporting and ownership interface. It does not define the internal diagnosis engine and does not require SMI.

---

## 10. Out-of-band RAS agent

The optional out-of-band agent may be an always-on RISC-V microcontroller, SCP-like management core, or BMC-side processor.

It is responsible for:

- corrected-ECC trend analysis;
- patrol scrub scheduling;
- per-rank/per-device predictive thresholds;
- PPR, sparing, and repair orchestration;
- persistent evidence journal;
- host checkstop postmortem;
- RAS worker watchdog and failover;
- Redfish/BMC/service reporting;
- selective domain reset and recovery.

It should have independent clock/reset/power where possible and sideband access that does not depend on the failed coherent fabric.

It is not responsible for precise source-hart exception recovery, OS page offlining, process termination, or VM machine-check injection.

---

## 11. Unified ownership state machine

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

This abstracts POWER IPOLL mask/ACK, x86 MCA cloak/uncloak, and RISC-V RERI/source-record ownership into one model.

Rules:

1. no volatile evidence is cleared before durable snapshot;
2. notification may be masked while analysis continues;
3. new events remain distinguishable from the in-flight event;
4. rearm occurs only after the responsible consumer acknowledges ownership;
5. timeout, dead worker, and failed OS consumer paths have explicit escalation.

---

## 12. Initial implementation plan

### Phase A: simulator model

Implement in ArchLab RVSoC-Sim:

- local error records;
- chiplet/domain summaries;
- RERI-like record blocks;
- low/high-priority signals;
- source-hart trap/RNMI capture;
- per-domain RAS queues;
- worker-hart scheduling;
- common-fabric broadcast stop;
- out-of-band agent takeover.

### Phase B: firmware skeleton

Implement:

- M-mode fast-path adapter;
- immutable preallocated snapshot ring;
- one coordinator plus per-domain workers;
- normalized RasEvent schema;
- simple topology graph and rule interpreter;
- CPER encoder as a northbound adapter.

### Phase C: diagnosis and recovery

Add:

- memory address/syndrome plugins;
- primary/secondary causal analysis;
- threshold persistence;
- page/device/domain isolation requests;
- out-of-band predictive and postmortem services.

---

## 13. Final design principle

```text
Use RISC-V mechanisms to receive and contain errors.
Use POWER-style logic to determine root cause.
Use ACPI APEI/CPER to communicate with standard OS software.
Use an out-of-band agent when host execution is unavailable or unnecessary.
```

The architecture contains no SMM and no SMI.
