# Jixia RAS Architecture

## Status

- **Type:** canonical architecture / research direction
- **Implementation status:** future work; this document does not change the current milestone order
- **Primary roadmap home:** M00-08 Structured Event ABI, Phase C PlatformGraph, Phase D RAS
- **Companion document:** `docs/JIXIA_RAS_REASONING_VISION.md`
- **Primary inspiration:** IBM Power Hostboot PRDF / Processor Runtime Diagnostics, extended for an AI-era cloud-server control plane

This document records the overall Jixia Reliability, Availability, and Serviceability architecture discussed in August 2026.

The design goal is not simply to predict whether a DIMM, SSD, or PCIe device will fail. Jixia should build a firmware reliability control plane that can **observe, correlate, explain, investigate, contain, recover, remember, and improve** across the entire physical machine lifecycle.

The central idea is:

> **Power-style deterministic PRD rules remain the trusted diagnostic spine. AI-era reasoning improves how rules are discovered, how incidents are correlated, how missing evidence is acquired, and how experience is retained across machines and across time.**

A second core principle is:

> **AI may propose hypotheses and candidate rules; accepted recovery actions remain deterministic, auditable, versioned, and policy controlled.**

---

## 1. Problem statement

Traditional server RAS information is fragmented across several software and hardware domains:

```text
firmware
    -> boot errors, HWP failures, hardware registers

BMC
    -> SEL / PEL / sensors / FRU state

OS / hypervisor
    -> kernel logs, machine checks, AER, EDAC, SMART, driver resets

cloud control plane
    -> workload impact, maintenance history, fleet statistics
```

Each layer sees only part of the machine.

BMC is persistent but resource constrained. The host OS has abundant storage and context but can be reinstalled or replaced. Firmware understands hardware semantics but normally keeps little long-term state. Cloud systems have fleet history but often lack low-level topology and diagnostic meaning.

The result is usually a large set of logs rather than a coherent machine history.

Jixia should instead provide a shared semantic model:

```text
Machine Identity
      +
PlatformGraph
      +
BootEpoch
      +
Structured Event ABI
      +
Incident / Case Model
      +
Health State
```

This common language allows firmware, BMC, OS, cloud services, HWP, RAS rules, AI reasoning, and Jingjie simulation to describe the same physical machine.

---

## 2. Overall architecture

```text
                         Fleet / Cloud RAS
                    case mining / rule learning
                              |
                       signed knowledge
                              |
                              v
+------------------------------------------------------------------+
|                       Machine Reliability Plane                  |
|                                                                  |
|  +------------------+       +--------------------------------+   |
|  | Semantic RAS     |<----->| Deterministic Policy / PRD    |   |
|  | Reasoner         |       | rules, severity, containment  |   |
|  | correlation      |       | retry, degrade, offline       |   |
|  | hypotheses       |       +---------------+----------------+   |
|  | case retrieval   |                       |                    |
|  | active diagnosis |                       v                    |
|  +---------+--------+                 Recovery / Verify          |
|            |                                                     |
|            v                                                     |
|       Safe HWP Probe                                             |
|            |                                                     |
|            v                                                     |
|  +------------------+       +--------------------------------+   |
|  | PlatformGraph    |<----->| Machine Health Journal         |   |
|  | topology/owner   |       | incidents / history / state    |   |
|  | health/state     |       +--------------------------------+   |
|  +---------+--------+                                            |
|            ^                                                     |
|            |                                                     |
|       Structured Events / FFDC / Measurements                    |
+------------+-----------------------------------------------------+
             |
   CPU / cache / NoC / DDR / PCIe / CXL / NIC / SSD / power
```

The architecture separates **evidence**, **reasoning**, **policy**, and **execution**.

Reasoning is allowed to be probabilistic. Recovery authority is not.

---

## 3. Four layers of RAS intelligence

### L0 — Hardware protection

Fast mechanisms implemented in hardware or very low-level firmware:

```text
ECC
CRC
retry
link replay
spare
poison
machine check
scrub
PHY retraining
```

These mechanisms protect correctness and create raw evidence.

### L1 — Deterministic PRD-like diagnostics

This is the architectural descendant of Power PRDF.

It contains explicit, versioned knowledge such as:

```text
signature
rule expression
threshold
callout
severity
resolution
containment
recovery
```

Examples:

```text
repeated cache-array CE
    -> call out core/cache slice
    -> threshold policy
    -> possible core offline

PCIe fatal AER
    -> identify topology owner
    -> contain device
    -> recover or offline
```

This layer must remain deterministic and explainable.

### L2 — Semantic RAS Reasoner

This layer does not replace L1. It works around it.

Responsibilities:

```text
temporal correlation
topology correlation
owner correlation
cross-subsystem correlation
historical comparison
incident graph construction
hypothesis generation
case retrieval
missing-evidence identification
active diagnostic requests
confidence/evidence tracking
```

Its output is not an unrestricted hardware action. It produces evidence-backed hypotheses and recommendations for the deterministic policy layer.

### L3 — Fleet intelligence

Cloud scale is used to improve knowledge rather than to control individual hardware directly.

```text
fleet incidents
      -> clustering / case mining
      -> recurring causal patterns
      -> candidate diagnostic rules
      -> offline validation
      -> Jingjie replay/fault injection
      -> engineering review
      -> canary deployment
      -> signed rule/policy update
```

The desired model is:

> **AI can learn the rules; firmware executes accepted rules deterministically.**

---

## 4. The machine needs a persistent identity and history

A server should not forget everything when it reboots.

Jixia should define stable identities for the machine and its replaceable components.

Possible identifiers:

```text
MachineId
ComponentId / serial identity
BootEpoch
EventId
IncidentId
CaseId
RuleVersion
FirmwareVersion
PlatformGraph generation
```

`BootEpoch` distinguishes one boot/runtime lifetime from another while still allowing the complete machine history to be correlated.

A component replacement must not erase the previous component's history. Instead the graph records lineage:

```text
MachineId: server-123

DIMM slot A1
    old ComponentId = DIMM-AAA
    removed at BootEpoch 81

    new ComponentId = DIMM-BBB
    installed at BootEpoch 82
```

This allows diagnosis to distinguish a bad slot/channel from a bad replaceable component.

---

## 5. Storage architecture: BMC, OS, and cloud cooperate

No single storage tier should be the only source of truth.

### Tier 0 — firmware / BMC persistent journal

The BMC is persistent but should **not** try to store the entire telemetry history.

It stores compact, high-value state:

```text
machine identity
current health summary
active incidents
recent critical events
boot history summary
last export watermark
critical FFDC references/hashes
component health state
```

The BMC should behave more like a **health-state keeper and durable queue** than a giant log server.

### Tier 1 — host OS journal

The OS has much more RAM, storage, and processing power. During normal runtime it should hold the rich machine record:

```text
structured firmware events
kernel RAS events
AER / EDAC / MCE
SMART / NVMe telemetry
HWP measurements
sensor history
compressed traces
incident graphs
case records
health time series
```

A future implementation might expose this under a dedicated host service rather than treating ordinary `dmesg` as the database.

Conceptually:

```text
/var/lib/jixia/
    events/
    incidents/
    cases/
    health/
    traces/
```

The exact storage format is intentionally deferred.

### Tier 2 — cloud / fleet history

Cloud storage is the long-term memory of the fleet.

It stores the durable history needed for:

```text
cross-machine case retrieval
fleet statistics
rule mining
component-lot analysis
firmware-version correlation
long-term machine history
maintenance outcomes
AI training / offline reasoning
```

If the host OS is reinstalled, the machine history still exists in the fleet database under the stable `MachineId`.

### Export protocol principle

Events should be sequence numbered and acknowledged.

```text
Jixia/BMC journal
      |
      | sequence 1001..1200
      v
OS collector / cloud collector
      |
      | ACK watermark = 1200
      v
BMC may compact old detailed records
but retains health summary / critical evidence
```

On network or OS failure, the local durable ring retains the unacknowledged high-value records and exports them later.

Critical incidents should support immediate out-of-band export when possible; ordinary telemetry can be batched.

---

## 6. Structured Event ABI: the foundation

AI cannot reason well over unstructured hexadecimal dumps alone.

The primary RAS input must therefore be structured events, not free-form strings.

Example conceptual event:

```text
Event {
    EventId
    BootEpoch
    timestamp
    source
    target ComponentId
    PlatformGraph node
    event_class
    severity
    register/evidence references
    owner
    correlation_id
    firmware_version
}
```

Representative classes:

```text
MEMORY_CE
MEMORY_UE
MEMORY_MARGIN_LOW
PCIE_AER
PCIE_LINK_RETRAIN
PCIE_REPLAY_BURST
CORE_MACHINE_CHECK
FABRIC_RETRY
HWP_TIMEOUT
DEVICE_TIMEOUT
THERMAL_EXCURSION
POWER_DROOP
SERVICE_CRASH
RESOURCE_OFFLINE
RECOVERY_RESULT
```

Human-readable logs are derived views. The structured event is authoritative.

M00-08 should establish the first minimal version of this ABI.

---

## 7. PlatformGraph: correlate errors with physical reality

Events have limited value without topology.

PlatformGraph should describe objects and relationships such as:

```text
Machine
  -> Socket
      -> Die
          -> Hart
          -> Cache
          -> MemoryController
              -> Channel
                  -> DIMM
      -> PCIeRootComplex
          -> Retimer
              -> NIC / SSD
```

Nodes also carry semantic state:

```text
owner
health
firmware service
power state
capabilities
resource state
component identity
```

An incident can therefore move from:

```text
"BDF 31:00.0 AER"
```

to:

```text
NIC0
  -> Retimer1
  -> PCIeRC1
  -> Socket0
  -> NetworkService owner
  -> affected cloud workload
```

This topology/ownership correlation is a major requirement for cloud-server RAS.

---

## 8. Incident Graph: turn many logs into one fault story

Modern server failures commonly produce cascades rather than isolated messages.

Example:

```text
AER corrected burst
      -> replay counter increases
      -> LTSSM recovery
      -> NIC reset
      -> DMA timeout
```

Five logs may describe one underlying link problem.

Jixia should construct an `IncidentGraph` using:

```text
time proximity
topology proximity
shared owner
causal rule knowledge
previous incidents
firmware/HWP context
```

An incident record should summarize:

```text
primary suspect
alternative hypotheses
supporting evidence
contradicting evidence
affected resources
actions attempted
recovery result
confidence
```

The objective is not merely log compression. It is reconstructing the **hardware fault story**.

---

## 9. HWP as both initialization and active diagnosis

Jixia HWP is integrated into the Jixia control plane rather than becoming a separate FSP-like semantic world.

Hardware/DV teams may own the internal register procedure, but Jixia owns:

```text
procedure identity
target
permissions
lifecycle
timeout
retry
structured result
FFDC
trace
health update
recovery semantics
```

Important design principle:

> **HWP implementation may be closed; HWP behavior must not be opaque.**

The same framework used for initialization can support safe diagnostic probes.

Examples:

```text
DDR_MARGIN_CHECK
PCIE_PHY_MARGIN_READ
PCIE_LTSSM_HISTORY
CORE_ARRAY_HEALTH_CHECK
FABRIC_RETRY_COUNTER_SNAPSHOT
NVME_HEALTH_SNAPSHOT
```

This enables **active diagnosis**.

```text
observe incident
      -> create hypotheses
      -> determine missing evidence
      -> request safe HWP probe
      -> receive structured result
      -> update hypotheses
      -> deterministic policy decides action
```

This is intentionally closer to a medical diagnostic loop than to passive log parsing.

---

## 10. Machine Health Journal

The journal represents long-term machine state rather than a raw event dump.

Example conceptual DIMM health state:

```text
DIMM A1
    ComponentId
    state = DEGRADED
    total_CE
    CE_rate
    CE_rate_trend
    scrub_corrections
    training_margin_history
    temperature_history
    previous_retries
    previous_incidents
    last_repair
    current_rule_findings
```

A PCIe path may track:

```text
AER counts
replay bursts
LTSSM recovery count
retrain count
speed downgrade history
PHY margin
retimer temperature
previous recovery results
```

This is where simple statistical models can be useful:

```text
EWMA
rate / acceleration
change detection
small decision trees
Bayesian scores
hazard estimates
```

The firmware-side model should remain small, deterministic in execution cost, explainable, and versionable. Large models belong in BMC-class or fleet infrastructure, not in the critical firmware path.

---

## 11. Case Memory: machines and fleets should remember solved incidents

A solved incident should become a reusable case rather than disappearing into a ticket system.

A case should retain:

```text
trigger
PlatformGraph context
event sequence
FFDC
hypotheses
diagnostic probes
recovery attempts
which action succeeded
final root cause
replaced component / repair outcome
firmware and rule versions
```

Future incidents can retrieve similar cases.

Example reasoning:

```text
Current incident resembles Case #104 and Case #827:

common pattern:
    AER burst
    LTSSM recovery
    replay burst
    same retimer family

historical outcomes:
    2/2 confirmed retimer degradation
```

This is a much more explainable use of retrieval/AI than returning only an opaque failure probability.

---

## 12. AI-era evolution of PRD rules

The key opportunity is **rule engineering automation**.

Traditional process:

```text
hardware expert observes field failures
      -> derives pattern manually
      -> writes threshold/rule
      -> validates
      -> releases firmware
```

AI-era process:

```text
fleet cases + repair outcomes
      -> AI pattern mining
      -> candidate causal/event-graph rule
      -> statistical evidence
      -> Jingjie replay/fault injection
      -> hardware/RAS engineer review
      -> canary validation
      -> signed rule package
      -> deterministic PRD engine
```

AI can help discover that a combination such as:

```text
PCIe replay rate rising
AND repeated LTSSM recovery
AND retimer temperature high
AND Gen5 margin low
```

has a strong relationship with a particular field failure.

The production rule remains explicit and auditable.

Rules should therefore become versioned data/knowledge artifacts rather than being inseparable from monolithic firmware code wherever practical.

Candidate long-term rule metadata:

```text
RuleId
RuleVersion
supported hardware revisions
required evidence
expression / event graph
confidence basis
callout
severity
allowed resolutions
source cases
validation evidence
signature
```

---

## 13. Deterministic recovery policy

AI or statistical reasoning must not directly perform destructive hardware operations.

The authority boundary is:

```text
RAS Reasoner
    -> recommendation + evidence

Deterministic Policy Engine
    -> validates action against explicit rules

Recovery Executor
    -> performs bounded action

Verifier
    -> confirms health/result
```

Typical actions:

```text
retry HWP
retrain link
downgrade link speed
restart firmware service
retire page
isolate memory region
offline hart/core/device
revoke capability
drain workload
request maintenance
system reboot as last resort
```

Every recovery attempt becomes new evidence.

A recovery is not complete until verification runs.

```text
Detect
  -> Diagnose
  -> Correlate
  -> Contain
  -> Recover
  -> Verify
  -> Record
```

---

## 14. Jingjie simulator: counterfactual and repeatable diagnosis

Some hypotheses are unsafe to test on a production server.

Jingjie should eventually accept:

```text
PlatformGraph snapshot
structured incident trace
register/health snapshot
firmware version
rule version
```

and support fault injection/replay.

Example:

```text
production incident
    -> hypothesize DDR-channel fault
    -> reproduce with Jingjie fault injection
    -> compare generated event graph

alternative hypothesis: PCIe corruption
    -> replay
    -> evidence does not match
```

This provides a future form of **counterfactual diagnosis** and also validates newly mined rules before deployment.

Firmware and simulator should share:

```text
Event ABI
Fault ABI
PlatformGraph identifiers
HWP semantic hooks
BootEpoch
health-state schema
```

---

## 15. BMC / OS / cloud responsibilities

The desired division of responsibility is:

```text
Jixia firmware
    hardware semantics
    event generation
    deterministic PRD
    local health state
    safe recovery

BMC
    durable identity
    compact health summary
    critical-event spool
    out-of-band export
    survives host failure

Host OS / hypervisor
    rich local history
    host telemetry
    large trace/event storage
    runtime collector

Cloud RAS backend
    fleet memory
    long-term case database
    component-lot analysis
    AI reasoning / rule mining
    rule distribution
```

The design must continue to function when any one higher tier is unavailable.

Cloud enhances reliability intelligence; cloud is not a prerequisite for basic correctness or containment.

---

## 16. Example: PCIe link degradation

Raw events:

```text
T0      PCIE_AER_CORRECTED NIC0
T0+1ms  PCIE_REPLAY_BURST NIC0
T0+2ms  PCIE_LTSSM_RECOVERY Retimer1
T0+4ms  DEVICE_TIMEOUT NIC0
```

PlatformGraph identifies a common path:

```text
NIC0 -> Retimer1 -> PCIeRC1 -> Socket0
```

The deterministic rule engine identifies known safe facts.

The semantic reasoner creates hypotheses:

```text
H1: link signal integrity degradation
H2: NIC internal failure
H3: root-complex issue
```

Missing evidence is requested through safe HWP probes:

```text
PCIE_PHY_MARGIN_READ
PCIE_LTSSM_HISTORY
retimer temperature
```

Evidence shows low Gen5 margin and repeated recovery.

The reasoner recommends retraining and possible speed degradation.

The deterministic policy permits:

```text
attempt 1: retrain Gen5
if repeated incident:
    degrade Gen5 -> Gen4
```

The result is verified and recorded as a case.

Fleet analysis may later discover the same pattern across a retimer lot and propose a new PRD rule.

---

## 17. Example: DDR degradation

Instead of only asking whether a DIMM will fail, Jixia combines:

```text
CE count
CE rate
CE acceleration
syndrome distribution
scrub corrections
training margin
temperature
channel topology
past BootEpoch history
previous repair results
```

The system may conclude:

```text
current state: DEGRADED
likely scope: DIMM A1 rather than memory controller
reason:
    repeated syndrome concentration
    margin decline across three boots
    peer DIMMs healthy
```

A targeted HWP probe or scrub can acquire additional evidence.

Only the deterministic policy decides whether to retire pages, deconfigure memory, drain workloads, or request maintenance.

Prediction is optional; diagnosis and explainable action are primary.

---

## 18. Non-goals

Jixia RAS is explicitly **not** intended to become:

```text
an LLM directly writing hardware registers
an opaque neural network replacing deterministic RAS rules
a giant BMC log database
an OS-only diagnostic system that loses history after reinstall
a cloud-dependent firmware correctness mechanism
only a DDR/SSD failure predictor
```

Large language models can be useful for engineering assistance, case summarization, rule mining, document retrieval, and hypothesis generation. They must not become an unbounded privileged execution path.

---

## 19. Roadmap alignment

This architecture should be built incrementally.

### M00-08 — Structured Event ABI

First prerequisite:

```text
stable EventId / BootEpoch
structured event record
basic per-hart/source metadata
machine-checkable trace
```

### Phase C — PlatformGraph and semantic debug

Add:

```text
stable physical object identities
topology relationships
owner / health fields
state snapshots
fault injection hooks
```

### HWP v0

After the early allocator/event foundation is stable:

```text
HwpContext
TargetHandle
structured HwpResult
timeout/retry
FFDC
RegisterAccess
safe diagnostic probe contract
```

### Phase D — deterministic RAS

Implement:

```text
RAS event classes
PRD-like rule engine
severity / callout / resolution
IncidentGraph
contain/recover/verify
Machine Health Journal
```

### Later — AI reasoning and fleet learning

Only after structured data and deterministic recovery are reliable:

```text
Case Memory
similar-case retrieval
hypothesis engine
active diagnosis
fleet rule mining
candidate-rule validation
Jingjie counterfactual replay
```

This ordering is intentional: **AI is built on top of reliable semantics, not used to compensate for missing semantics.**

---

## 20. First meaningful end-to-end RAS demo

A future milestone should eventually demonstrate something close to:

```text
inject PCIe link degradation
      -> produce several raw hardware events
      -> correlate them into one IncidentGraph
      -> map the incident through PlatformGraph
      -> apply deterministic PRD knowledge
      -> identify missing evidence
      -> execute a safe HWP diagnostic probe
      -> update the diagnosis
      -> retrain the link
      -> observe recurrence
      -> degrade Gen5 to Gen4
      -> verify stable operation
      -> persist the incident as a Case
      -> export summary through BMC and full evidence through OS/cloud
      -> replay the same case in Jingjie
```

If Jixia can do this coherently, it has moved beyond a boot-centric firmware model into a genuine **Hardware Reliability Control Plane**.

---

## 21. Architectural summary

The overall Jixia RAS model is:

```text
Hardware protection
        |
        v
Structured evidence
        |
        v
PlatformGraph + Machine History
        |
        +-----------------------------+
        |                             |
        v                             v
Deterministic PRD              Semantic Reasoner
        |                 correlation / hypothesis
        |                 case memory / active probe
        +---------------+-------------+
                        |
                        v
              Deterministic Policy
                        |
                        v
                  HWP / Recovery
                        |
                        v
                     Verify
                        |
                        v
                 Incident / Case
                        |
        +---------------+----------------+
        |                                |
        v                                v
BMC compact durable state        OS rich local history
        |                                |
        +---------------+----------------+
                        |
                        v
                 Fleet RAS Memory
                        |
                        v
              AI rule/case learning
                        |
                        v
            validated signed knowledge
```

The intended differentiation is not that Jixia contains more AI than existing firmware.

The intended differentiation is that Jixia gives AI and deterministic RAS a **shared, structured, testable model of the physical machine**, and then closes the loop from evidence to diagnosis to recovery to learning.