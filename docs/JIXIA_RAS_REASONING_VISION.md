# Jixia RAS Reasoning Vision

## Status

- **Type:** architecture/research vision
- **Implementation status:** future work; not an active milestone
- **Primary roadmap home:** Phase D — Rule-driven RAS, diagnosis, and recovery
- **Key prerequisites:** Structured Event ABI, PlatformGraph, HWP execution contract, persistent BootEpoch/health journal, Jingjie fault injection/replay
- **Design inspiration:** IBM Power Hostboot PRDF / Processor Runtime Diagnostics

This document records the direction discussed on 2026-08-10 for a Jixia RAS system that goes beyond simple failure prediction. The goal is not to put an opaque AI model in charge of server recovery. The goal is to combine Power-style deterministic hardware diagnosis rules with structured evidence, topology, machine history, active HWP probes, case memory, fleet learning, and simulator-assisted reasoning.

The central idea is:

> **Do not only predict whether hardware will fail. Make the firmware understand, explain, investigate, contain, and learn from hardware incidents.**

---

## 1. Why Power PRDF is the right starting point

Hostboot names PRDF as **Processor Runtime Diagnostics**. The open Hostboot tree contains a substantial diagnostic framework rather than a flat collection of error handlers. Relevant areas include:

```text
src/usr/diag/prdf/common/rule/
src/usr/diag/prdf/common/framework/service/
src/usr/diag/prdf/common/framework/resolution/
src/usr/diag/prdf/common/plat/
```

Examples in the source tree include rule expressions and rule metadata, a ServiceDataCollector, ResolutionFactory, RAS services, and platform-specific diagnostic plugins.

Jixia should treat this as an architectural clue:

```text
hardware evidence
      -> diagnostic knowledge
      -> collected context
      -> resolution
```

PRDF can therefore be viewed as a deterministic hardware-diagnosis knowledge model or expert system. Jixia should preserve this deterministic layer and extend it for an AI-era cloud-server environment.

### Important distinction

The future Jixia system is **not**:

```text
CE/SMART counters
      -> opaque ML model
      -> failure probability
      -> directly offline hardware
```

It is closer to:

```text
structured evidence
      + topology
      + deterministic rules
      + machine history
      + prior incidents
      + active diagnostic probes
      + simulator experiments
      -> explainable diagnosis hypotheses
      -> deterministic policy decision
      -> containment/recovery
      -> verification
```

---

## 2. Four-layer RAS intelligence model

Jixia should ultimately separate hardware protection, deterministic diagnosis, semantic reasoning, and fleet learning.

```text
L0  Hardware Protection
    ECC / CRC / retry / spare / link recovery

L1  Deterministic PRD-like Diagnostics
    signatures / rule expressions / thresholds / callouts / resolutions

L2  Semantic RAS Reasoner
    event correlation / topology reasoning / hypotheses /
    active diagnosis / case retrieval / incident summarization

L3  Fleet Intelligence
    cross-machine case mining / pattern discovery /
    candidate-rule generation / policy improvement
```

The layers have different authority.

### L0 — Hardware protection

Fast, local, architectural mechanisms handle correctable conditions whenever possible.

### L1 — Deterministic PRD-like diagnostics

This is the safety-critical diagnosis and recovery knowledge base. Rules are explicit, reviewable, versioned, testable, and deterministic.

### L2 — Semantic RAS Reasoner

This layer helps answer:

- Are these ten logs actually one incident?
- Which PlatformGraph objects are involved?
- What common upstream component can explain all symptoms?
- Which evidence supports or contradicts each hypothesis?
- What safe HWP probe would best distinguish the hypotheses?
- Have we seen a similar incident before?
- What happened after previous recovery attempts?

### L3 — Fleet intelligence

Fleet-scale AI may discover patterns that no single server can see. It can propose new rules, correlations, thresholds, or diagnostic procedures, but cannot silently rewrite live recovery policy.

---

## 3. Deterministic policy remains authoritative

This is a non-negotiable safety boundary.

```text
AI / statistical reasoning
        -> hypothesis
        -> evidence summary
        -> confidence
        -> recommended next probe/action

Deterministic RAS Policy Engine
        -> validate evidence
        -> validate action safety
        -> validate redundancy/availability
        -> enforce cloud policy
        -> execute or reject
```

A language model, embedding model, anomaly detector, or learned health score must never directly perform destructive operations such as:

```text
reset memory controller
offline hart
retire DIMM
power-cycle PCIe hierarchy
revoke device ownership
```

The model can recommend. The deterministic policy owns authority.

A useful project principle is:

> **AI may learn or propose the rules; firmware executes accepted rules deterministically.**

---

## 4. Incident Graph instead of a pile of logs

The fundamental RAS data structure should not be a text log line. It should be a semantic **Incident Graph**.

Example:

```text
Incident #4817

                 PCIe AER burst
                       |
          +------------+-------------+
          |                          |
      target=NIC0                timestamp window
          |                          |
          v                          v
       PCIeRC1                replay counter spike
          |
       Socket0
          |
          +-- LTSSM recovery
          +-- NIC reset
          +-- device timeout
          +-- previous BootEpoch: retrain x3
```

The graph should correlate at least four dimensions:

```text
temporal correlation
    events occurring in the same time window

topology correlation
    events sharing a PlatformGraph path or dependency

ownership correlation
    firmware service / OS / VM / device owner relationship

historical correlation
    related events from previous BootEpochs and prior incidents
```

The desired output is a compact, machine-readable diagnosis record rather than dozens of disconnected FFDC records.

Example:

```text
Incident #4817

Primary suspect:
    PCIeRC1 / NIC0 link

Hypothesis:
    PCIe link-margin degradation

Supporting evidence:
    AER corrected burst
    replay-rate spike
    LTSSM recovery events
    previous retrain history

Contradicting evidence:
    NIC internal-health checks normal

Missing evidence:
    PHY margin snapshot

Recommended probe:
    PCIE_READ_MARGIN
```

---

## 5. Hypothesis-driven RAS

A major Jixia extension beyond a traditional rule engine should be **hypothesis-driven diagnosis**.

Instead of only matching a signature and immediately choosing a resolution:

```text
symptoms
   -> H1: DIMM/rank problem
   -> H2: memory-controller problem
   -> H3: fabric corruption
```

The reasoner should ask:

> Which safe, bounded diagnostic action gives the most useful evidence to distinguish H1/H2/H3?

Possible evidence-gathering actions:

```text
read syndrome distribution
read neighboring-channel counters
capture PHY margin
capture LTSSM history
run targeted scrub
read retry counters
read thermal/power history
```

Then:

```text
hypotheses
    -> select safe probe
    -> HWP executes probe
    -> structured result
    -> update hypotheses
    -> repeat if justified
    -> deterministic policy resolves incident
```

This makes HWP part of runtime diagnosis, not only boot-time initialization.

---

## 6. HWP integration: fused but bounded

Jixia HWP should be **fused into the Jixia control plane**, while still allowing hardware/DV teams to own the low-level procedure implementation.

Hardware teams own:

```text
register sequences
training algorithms
PHY/controller-specific knowledge
silicon workarounds
```

Jixia owns:

```text
procedure lifecycle
Target identity
capabilities / allowed MMIO
structured status
error taxonomy
FFDC
trace
bounded timeout
retry budget
rollback/checkpoint semantics
health publication
```

For RAS reasoning, HWP must expose safe diagnostic probes with explicit contracts.

Example:

```text
RAS Reasoner
    "need PCIe PHY margin evidence"
            |
            v
Safe HWP Probe
    PCIE_READ_MARGIN(target=NIC0)
            |
            v
Structured Result
    lane / margin / status / timestamp / provenance
            |
            v
Incident Graph
```

The implementation may be silicon-specific or even opaque, but its externally visible behavior must not be opaque.

Project principle:

> **HWP implementation may be closed; HWP behavior, authority, evidence, and state transitions must be visible to Jixia.**

---

## 7. Case Memory: learn from solved incidents

One of the most practical AI-era extensions is not prediction but **case memory**.

Every completed incident should retain a compact structured case:

```text
Case
  trigger
  affected PlatformGraph objects
  event sequence
  FFDC references
  hypotheses considered
  diagnosis
  recovery actions attempted
  action outcomes
  final root cause if known
  verification result
  firmware/model/rule version
```

A new incident can retrieve similar prior cases:

```text
current incident
      -> retrieve similar cases
      -> compare event/topology patterns
      -> surface prior successful/failed actions
```

The reasoner can then explain why a prior case is relevant instead of presenting an unexplained neural-network score.

Example:

```text
Why suspect the retimer?

Because Case #104 and Case #827 had the same sequence:
AER burst -> LTSSM recovery -> replay spike,
with the same retimer type and topology location.
Both were confirmed as retimer degradation.
```

This is particularly suitable for retrieval/embedding techniques because the output remains traceable back to concrete incidents.

---

## 8. Machine Health Journal: the server must not forget every reboot

Jixia should maintain a persistent **Machine Health Journal** across BootEpochs.

```text
BootEpoch N
   -> load previous health journal
   -> collect boot HWP measurements
   -> compare with history
   -> establish boot health baseline
   -> runtime events update health history
   -> persist compact state

BootEpoch N+1
   -> continue from history
```

Useful historical evidence may include:

```text
memory CE/UE and scrub history
training margins
link retries/retrains
AER history
corrected machine checks
array-repair/spare usage
thermal excursions
power anomalies
timeouts and reset history
HWP degraded-mode decisions
service/recovery history
```

The purpose is not necessarily to predict a failure date. It is to give the reasoner context:

```text
"This is the fifth link retrain over three BootEpochs"
```

is much more informative than:

```text
"A link retrain happened"
```

---

## 9. Health models are supporting evidence, not the product

Jixia may eventually use compact adaptive health models, but the project should not be centered on generic DDR/SSD failure prediction.

On-machine models should favor bounded, explainable mechanisms:

```text
counters
rates and acceleration
EWMA/trends
small decision trees
Bayesian updates
hazard/risk scores
threshold adaptation
```

They should have:

```text
small memory footprint
bounded execution time
versioned state
reproducible output
explainable features
safe fallback behavior
```

A large opaque neural model is not required for the firmware-side value proposition.

The model is useful when it contributes evidence such as:

```text
CE rate is accelerating
link retrain frequency is increasing
training margin is steadily shrinking
this target differs strongly from topology peers
```

The deterministic RAS layer still decides whether to retire, degrade, retry, or continue.

---

## 10. Fleet intelligence

Cloud scale gives Jixia another dimension unavailable to a single machine.

```text
many machines
    -> incident cases
    -> offline analysis
    -> discover recurrent patterns
    -> propose candidate PRD rule / probe / threshold
    -> simulator validation
    -> human review
    -> canary deployment
    -> signed deterministic policy update
```

Fleet intelligence can supply a prior, while the local machine contributes its own history.

```text
fleet knowledge
       +
local machine history
       -> local diagnosis context
```

A future cloud control plane may also receive compact Incident/Health summaries instead of raw firmware logs.

---

## 11. Jingjie: counterfactual diagnosis and replay

Jingjie should eventually be part of the RAS reasoning loop.

Some diagnostic experiments are unsafe on a live cloud server. The firmware should be able to export enough semantic state to reproduce or approximate an incident in the simulator.

```text
PlatformGraph snapshot
+ Incident Graph
+ structured event trace
+ selected register/FFDC state
        |
        v
      Jingjie
        |
        +-- replay
        +-- inject hypothesis A
        +-- inject hypothesis B
        +-- compare resulting event sequences
```

This enables **counterfactual diagnosis**:

```text
Hypothesis A reproduces the observed evidence.
Hypothesis B does not.
```

The simulator is not an oracle, but it provides a safe environment for active experiments that cannot be attempted on production hardware.

---

## 12. Proposed core objects

The eventual design should converge on a small set of durable schemas.

### RasEvent

A semantic event, not a log string.

Possible fields:

```text
event_id
BootEpoch
timestamp
source TargetId
error class
severity
register/syndrome evidence
owner/service context
correlation IDs
```

### Incident

Groups correlated events into one diagnosis episode.

```text
incident_id
start/end time
involved targets
supporting events
hypotheses
active probes
current diagnosis
policy state
```

### Hypothesis

```text
cause class
suspected target(s)
confidence/rank
supporting evidence
contradicting evidence
missing evidence
recommended probe
```

### DiagnosisRecord

Final compact explanation.

```text
primary cause
secondary effects
callouts
confidence/evidence
recovery recommendation
```

### RecoveryRecord

```text
action
policy/rule version
preconditions
result
verification evidence
rollback/degraded state
```

### HealthJournal

Persistent history keyed by stable PlatformGraph TargetId and BootEpoch.

### CaseRecord

Longer-lived incident memory suitable for retrieval and fleet learning.

---

## 13. Example: PCIe link instability

Raw evidence:

```text
T0        corrected AER
T0+0.7ms  replay counter spike
T0+1.1ms  LTSSM recovery
T0+3ms    NIC timeout
T0+4ms    driver/device reset
```

Traditional logging can leave these as separate records.

Jixia target behavior:

```text
1. correlate all events to NIC0 -> PCIeRC1 -> Socket0
2. create one Incident
3. PRD-like rules identify link/fabric candidates
4. retrieve prior cases for the same retimer/link type
5. hypothesis engine ranks:
      H1 link-margin degradation
      H2 NIC internal fault
      H3 upstream power event
6. request bounded HWP probes:
      PHY margin
      LTSSM history
      power telemetry
7. update evidence
8. deterministic policy chooses:
      retrain once
9. observe recurrence
10. policy chooses degraded Gen4 operation
11. verify stability
12. update PlatformGraph health and Case Memory
```

The important output is not only `link recovered` but an auditable story explaining why the system acted.

---

## 14. Example: memory incident

Raw evidence may include:

```text
CE burst
syndrome concentration
scrub corrections
training-margin history
thermal history
neighboring-rank status
```

The reasoning system can distinguish candidate causes:

```text
H1 local DRAM/rank degradation
H2 memory-controller/channel problem
H3 broader fabric/data-path corruption
```

A targeted scrub or diagnostic HWP can provide additional evidence. The deterministic layer decides whether to retire pages, degrade capacity, call out a component, or escalate.

This is intentionally richer than:

```text
if CE_count > threshold then predict failure
```

---

## 15. Non-goals and safety rules

The following are explicitly not the objective:

```text
[ ] put a general LLM in M-mode and let it control hardware directly
[ ] replace ECC/RAS rules with probabilistic prediction
[ ] hide diagnosis behind one unexplainable health score
[ ] allow fleet AI to push unreviewed recovery logic
[ ] collect unlimited telemetry without bounded storage/retention
[ ] treat temporal correlation as proof of causality
```

Required properties:

```text
explainable evidence
bounded execution
versioned rule/model provenance
replayable decisions
safe authority boundaries
machine-checkable recovery verification
```

---

## 16. Relationship to current roadmap

This design is future work and must not interrupt the current single-active-milestone discipline.

Prerequisite path:

```text
M00-08 Structured Event ABI
        |
        v
PlatformGraph + semantic trace
        |
        v
HWP execution/probe contract
        |
        v
Phase D deterministic PRD-like RAS
        |
        +-- Incident Graph
        +-- Health Journal
        +-- Case Memory
        +-- hypothesis-driven diagnosis
        +-- active HWP probes
        |
        v
Jingjie replay/fault injection
        |
        v
fleet-assisted rule discovery
```

The first implementation should be small. A useful initial demo would use only a handful of synthetic fault classes and prove that the system can turn noisy events into a coherent incident explanation.

Suggested first demo:

```text
inject one of:
    PCIe link flap
    DDR CE storm
    timer/device timeout
    service crash

produce:
    one Incident Graph
    involved PlatformGraph targets
    ranked cause hypotheses
    missing evidence
    recommended safe HWP probe
    deterministic resolution
    verified recovery record
```

---

## 17. Core project thesis

The RAS differentiator for Jixia should not be:

> "We use AI to predict SSD/DDR failures."

It should be:

> **Jixia turns low-level hardware evidence into a structured, explainable incident; reasons over topology, history, and prior cases; actively gathers missing evidence through bounded HWP probes; executes recovery only through deterministic policy; verifies the result; and preserves the case so the machine and fleet become better at diagnosis over time.**

A concise architecture slogan is:

> **Power-style PRD rules provide the trusted diagnostic spine; AI-era reasoning adds correlation, memory, hypothesis generation, and learning around that spine.**

This is the intended direction for Jixia's future cloud-server reliability control plane.
