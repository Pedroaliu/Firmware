# ArchFW Distributed RAS Design v0.2

Canonical architecture: [ArchFW Microkernel and Firmware Architecture v0.2](ARCHFW_MICROKERNEL_FIRMWARE_ARCHITECTURE_V0.2.md).

This document corrects the earlier design that placed a complete root-cause engine in HostFW. The production design assigns complete platform diagnosis to an independent RAS Processor when that domain exists. HostFW executes the resulting boot/topology actions.

## 1. Architecture

```text
Hardware detector and containment
        |
        v
Local source adapter
  minimum capture, mask/ack, immutable snapshot
        |
        v
Independent RAS Processor
  first-error correlation
  topology-aware root cause
  thresholds and predictive analysis
  FRU/callout, repair, reset and deconfiguration plan
        |
        v
Signed or authenticated RAS Action Manifest
        |
        v
HostFW RAS Broker
  validate manifest
  update Runtime State Overlay
  revoke capabilities
  trigger istep reconfiguration
        |
        v
OS/hypervisor recovery
  page/CPU/device/process/VM actions
        |
        v
BMC and fleet service
```

The independent RAS Processor may be a dedicated RISC-V controller, SCP-class management core or BMC-side processor with sideband access.

## 2. Responsibility split

### Hardware and local adapter

- detect and contain at the smallest safe domain;
- capture volatile source-local evidence;
- prevent interrupt or attention storms;
- publish an immutable event descriptor;
- park, resume or isolate the source execution context as allowed by containment.

The immediate path does not run topology scans, CPER construction, service communication or long repair algorithms.

### Independent RAS Processor

- own the complete platform topology/rule/plugin diagnosis engine;
- distinguish reporter, suspected source and victim;
- correlate primary and secondary errors;
- preserve first-error ordering;
- manage thresholds, predictive actions and persistent evidence;
- produce an action manifest with confidence and prerequisites;
- perform host-dead postmortem through sideband paths;
- watchdog RAS workers and coordinate failover.

### HostFW RAS Broker

- authenticate and validate the manifest schema and epoch;
- reject stale target/topology generations;
- translate actions into Target State Deltas;
- revoke capabilities for unavailable targets;
- stop or restart affected services;
- ask the IStep engine to compute a safe re-entry checkpoint;
- expose boot-time evidence to EDK II/ACPI and the OS.

HostFW may contain a minimal fallback classifier for QEMU and bring-up, but it does not become a second production PRDF engine.

### OS/hypervisor

- offline pages, CPUs and devices;
- poison or SIGBUS affected execution;
- inject guest machine checks where required;
- terminate or migrate affected processes/VMs;
- report workload impact back to management software.

### BMC/fleet

- persistent service history;
- inventory and FRU replacement workflow;
- cross-machine trend correlation;
- operator and fleet policy;
- firmware rollout and repair scheduling.

## 3. Event model

```cpp
struct RasEvent {
    EventId event_id;
    CorrelationId correlation_id;
    uint64_t boot_epoch;
    uint64_t topology_generation;
    uint64_t timestamp;

    TargetRef reporter;
    TargetRef suspected_source;
    TargetRef victim;

    ErrorClass error_class;
    Severity severity;
    ContainmentScope containment;

    bool synchronous;
    bool precise_pc;
    bool restartable;
    bool poison_contained;
    bool coherency_lost;

    EvidenceDescriptor evidence;
    Confidence confidence;
    RasLifecycleState state;
};
```

A common-fabric reporter is not automatically the failed FRU. Diagnosis follows topology and propagation evidence.

## 4. Evidence lifecycle

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

Rules:

1. volatile evidence is not cleared before an immutable snapshot exists;
2. masking and clearing are separate actions;
3. new events remain distinguishable from an in-flight event;
4. every owner transition is journaled;
5. timeout and dead-consumer paths escalate explicitly;
6. rearm occurs only after the responsible owner acknowledges completion.

## 5. Containment classes

| Class | Scope | Immediate action | Unaffected work |
|---|---|---|---|
| C0 | corrected telemetry | snapshot and queue | continues |
| C1 | hart/core local | recover or park source | continues |
| C2 | channel/device/cluster contained | isolate target/domain | continues outside domain |
| C3 | die/socket shared-resource uncertainty | quiesce affected domain | may continue elsewhere |
| C4 | coherency/common-fabric integrity lost | broadcast stop/checkstop | stops |
| C5 | host no longer trustworthy | sideband postmortem and reset | stopped |

The containment decision is based on proof of safe continued execution, not only a severity label.

## 6. RISC-V host path

RISC-V has no architectural SMM/SMI equivalent. Urgent CPU-local capture uses an M-mode trap, RNMI/Smrnmi where implemented, or platform-defined NMI. Corrected/deferred events use local or external interrupts. RERI or source-specific CSR/MMIO records provide evidence.

The source hart performs only bounded capture and containment. One or more reserved/elected RAS worker harts or the independent processor perform full diagnosis.

See [RISC-V RAS Architecture v0.2](ARCHFW_RISCV_RAS_ARCHITECTURE_V0.2.md).

## 7. Action manifest

```cpp
struct RasActionManifest {
    ManifestId id;
    uint64_t producer_generation;
    uint64_t boot_epoch;
    uint64_t topology_generation;
    CorrelationId correlation_id;

    Span<TargetAction> target_actions;
    Span<RepairAction> repair_actions;
    Span<ResetAction> reset_actions;
    Span<OsRecoveryHint> os_hints;

    StepId minimum_reentry_step;
    Confidence confidence;
    EvidenceDigest evidence_digest;
    SignatureOrMac authentication;
};
```

Target actions include:

```text
DEGRADE
QUARANTINE
DECONFIGURE
GARD_PERSISTENT
CLEAR_GARD
REQUIRE_RETEST
REQUIRE_RETRAIN
```

HostFW checks that every action targets a known generation and does not exceed the RAS Processor's delegated authority.

## 8. Boot recovery

```text
Boot0/Root Orchestrator
  -> read persistent RAS journal
  -> validate producer identity, epoch policy and evidence digest
  -> apply allowed target health actions
  -> construct reduced Runtime State Overlay
  -> execute required retest/retrain isteps
  -> finalize Published OS View
  -> generate EFI memory map and ACPI
```

If a manifest is corrupt, stale or unauthorized, ArchFW enters a conservative recovery policy: preserve evidence, avoid re-enabling implicated resources, and request management intervention.

## 9. QEMU M04 scenario

```text
inject uncorrectable L2/core error on hart 3
  -> local adapter snapshots and parks hart 3
  -> simulated RAS Processor correlates source and writes manifest
  -> platform checkstop/reset
  -> next boot validates manifest
  -> hart/core target is deconfigured
  -> its MMIO/IRQ/TCB capabilities are revoked
  -> dependent isteps replay from the calculated checkpoint
  -> ACPI omits the failed hart
  -> Linux boots with reduced topology
```

The test must also inject stale manifests, lost completion, evidence corruption and RAS-processor reset.

## 10. Explicit non-goals

- SMI-style hidden host execution;
- clearing evidence in an ISR before durable capture;
- a full duplicate HostFW rule engine;
- global stop for every corrected or contained error;
- relying on a customer user-space daemon for basic containment or boot recovery;
- using CPER as the internal root-cause data model.

CPER/APEI are northbound reporting formats. The internal model preserves platform topology, causal evidence and ownership state.