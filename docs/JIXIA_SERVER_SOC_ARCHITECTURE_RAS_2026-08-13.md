# Jixia RISC-V Server SoC Architecture & RAS Design Record

Date: 2026-08-13
Status: Architecture decision record / working baseline

## 1. Purpose

This document records the current Jixia RISC-V server-platform architecture direction and the RAS/serviceability decisions made together on 2026-08-13. The intent is to keep the SoC customization and RAS design in one place so later QEMU, firmware, management-complex, and SimSOC work all follow the same architecture.

The design is informed by RISC-V Server SoC / Server Platform requirements and by lessons from IBM POWER Hostboot/SBE/OCC/FSP/pdbg/FSI as well as Arm SCP/MCP firmware, but Jixia is not intended to clone either architecture.

## 2. Top-level platform domains

Jixia Server currently has four first-class execution / authority domains:

1. **SBE — Self-Boot Engine**
   - Small RISC-V boot engine.
   - Earliest bootstrap authority.
   - Tiny runtime/state-machine style firmware; does **not** run the Jixia microkernel.
   - Responsible for reset entry, minimal clock/reset/fabric/security/bootstrap work and starting the Management Complex.

2. **M-C — Management Complex**
   - Multi-core-capable RISC-V management processor complex.
   - Runs the Jixia service-profile microkernel/runtime.
   - Unified SoC management domain; do not split into separate SCP/MCP/OCC processors unless a future hard isolation/real-time requirement proves it necessary.
   - Absorbs the useful responsibilities of Arm SCP/MCP and POWER OCC/FSP-style chip service functions as software services.

3. **Host**
   - RV64 application processor cluster.
   - Runs the Jixia host-profile microkernel/firmware.
   - Hostboot-like platform initialization, SMP, memory, PCIe, IOMMU, AIA, RAS, ACPI/UEFI handoff/runtime, OS boot.

4. **BMC**
   - Board-level/OOB management authority.
   - Initially modeled using ASPEED/OpenBMC in QEMU.
   - Must not possess unrestricted raw access to internal SoC registers or the management service fabric.

Authority shorthand:

- **SBE = Bootstrap Authority**
- **M-C = SoC Service Authority**
- **Host = Compute Authority**
- **BMC = Platform/OOB Authority**

## 3. Jixia software model

Jixia is one microkernel architecture with different execution profiles, not separate operating systems for Host and management processors.

### Jixia-H

Host profile:

- RV64
- MMU / Sv48-class virtual memory
- SMP
- IPC / events / notifications
- IRQ/timer
- service/driver model
- fault isolation
- larger/dynamic memory-management facilities

### Jixia-S

Management-complex profile:

- RV32 or RV64 management cores
- PMP/ePMP-oriented protection
- small footprint
- static/fixed-region allocation first
- low-latency IRQ/event processing
- fixed-priority/real-time support where needed
- IPC/capability-style resource delegation
- individual management services isolated from one another where practical

SCP/MCP are therefore **service compositions**, not separate OS/kernel projects.

Typical M-C services:

- power
- clock
- reset
- DVFS/performance
- thermal
- sensor/telemetry
- watchdog
- RAS
- FFDC
- recovery
- manageability
- Host management protocol endpoint
- MCTP / PLDM / BMC-facing services
- service/debug functions

Power-control-loop functionality inspired by OCC can be pinned to a dedicated M-C core / RT priority without creating a separate OCC processor.

## 4. RISC-V Server SoC baseline

The Host SoC should follow the RISC-V Server SoC / Server Platform specifications rather than inventing incompatible software-visible interfaces.

Baseline blocks include:

- RV64 application harts
- AIA: APLIC + IMSIC
- ACLINT/timer facilities as appropriate
- RISC-V IOMMU
- PCIe Root Complex
- DDR / memory subsystem
- RAS architecture
- HPM / QoS hooks
- secure boot / RoT hooks
- management interfaces

Rule: **standardize software-visible contracts where a standard exists; use our own design freedom for internal topology, NoC, service processors, management fabric, reset/power/clock hierarchy, and RAS implementation.**

## 5. Management Complex as FSP-like chip-service authority

The M-C is not only a power-management controller. It is the privileged SoC service authority and must have access to service/debug/RAS state of all significant IP blocks.

M-C should be able to inspect/control, subject to policy:

- CPU/core/cluster state
- cache/LLC state
- NoC/fabric state
- DDR controller/PHY state
- PCIe RC/PHY/LTSSM/service state
- IOMMU
- AIA
- CXL or future accelerators
- clock/reset/power controllers
- RAS/error blocks
- trace/history buffers

Each important IP should expose a dedicated management/service interface including, where applicable:

- FIR (Fault Isolation Register) state
- FIR mask/action/severity
- first-error capture
- error address
- syndrome
- counters
- internal progress/state-machine state
- reset/power/clock state
- debug/status
- trace/history buffer
- training/repair data
- version/identity

BMC is a client of this service layer; it is not the owner of raw chip-service access.

## 6. Dedicated service / escape fabric

A core requirement is that RAS/debug access **must not depend solely on the normal coherent/data NoC**.

If the main NoC, coherence fabric, DDR path, PCIe path, or Host is wedged, the M-C must still be able to read critical error state.

Therefore define a logically independent management/service path. Working conceptual names:

- **JESC — Jixia Emergency Service Channel**: minimum, always-on, highly reliable control/escape path.
- **JMAF — Jixia Management Access Fabric**: general privileged management-register/service path.
- **JDF — Jixia Dump Fabric**: higher-bandwidth FFDC/trace bulk-data path.

Names are provisional; the separation of concerns is the important architecture decision.

### JESC requirements

- always-on clock/power domain where practical
- independent from normal Host coherence/data traffic
- simple transaction model
- no dependence on Host DDR
- no dynamic allocation
- no cache-coherence dependency
- reliable access to FIR/first-error/reset/freeze/dump-trigger state

### JDF requirements

Used for bulk FFDC rather than individual register reads:

- burst transfers
- DMA-style movement
- multiple outstanding transactions if useful
- wide data path where justified
- sized from target dump volume and target dump time, not from an arbitrary bus frequency

Control path reliability and bulk-dump throughput are deliberately separated.

## 7. IP-local retained diagnostic state

Each major IP should latch critical fault state into a simple service-visible bank that remains readable even if the normal datapath/FSM is unhealthy.

Recommended IP-local structures:

- FIR
- first-error source
- error address/syndrome
- error sequence / timestamp
- minimal FSM/queue summary
- optional local FFDC/trace SRAM

The principle is:

> datapath dead != diagnostic state dead

For larger blocks, provide a local FFDC/trace buffer and a hardware freeze mechanism so root-cause state is not overwritten by secondary failures.

## 8. First-failure preservation and correlation

Fatal/error handling should preserve first-failure information before recovery/reset destroys it.

Desired metadata includes:

- VALID
- BOOT_EPOCH
- ERROR_SEQUENCE
- TIMESTAMP
- CAPTURE_GENERATION
- source/domain ID
- consumed/ack state

This prevents a retained FIR from an earlier boot being mistaken for a new failure and allows cross-IP correlation of cascaded errors.

## 9. Crash Capture Engine

Do not rely exclusively on M-C software to preserve fatal-state data. A small hardware Crash Capture Engine should be considered part of the RAS architecture.

On a global fatal trigger it may:

1. latch first-failure state
2. freeze selected IP trace/FFDC state
3. collect a minimal crash capsule from each relevant block
4. write it to retention SRAM / always-on crash storage
5. assert capture-valid
6. allow reset after capture-complete or timeout

A reset must not wait forever for capture. Hardware rule:

`capture_done OR capture_timeout -> reset_allowed`

The retained crash capsule is intentionally richer than a few FIR bits but smaller and more deterministic than a full live dump.

## 10. Four-level failure/recovery ladder

The current preferred RAS/serviceability hierarchy is:

### Level 0 — service fault

- One M-C user/service task fails.
- Jixia-S isolates/restarts the service where possible.

### Level 1 — M-C software/core hang

- Attempt to reset/recover the Management Complex without necessarily resetting the Host.

### Level 2 — M-C unavailable, live state still valuable

- BMC may perform a **signed break-glass takeover** of the emergency service path.
- Goal: collect live FIR/FFDC/trace data before reset.

### Level 3 — live dump impossible or recovery must be immediate

- hardware crash capsule / reset-retained FIR state
- sync-flood-like warm reset/reboot
- Jixia-H performs early previous-boot RAS harvest before destructive initialization

This is intentionally not an either/or choice between BMC takeover and reboot scanning. We retain both because they optimize different things:

- **BMC live takeover -> best diagnostic fidelity**
- **reset-retained state -> strongest final recovery guarantee**

## 11. BMC break-glass takeover

Normally:

- M-C owns privileged chip-service access.
- BMC uses protocol-level requests to M-C.
- BMC has no unrestricted raw register access.

If M-C heartbeat is lost and a fatal condition is latched, always-on hardware may switch the emergency service-path owner to a BMC emergency gateway.

The BMC must present a signed authorization token verified by immutable/early trusted hardware, e.g. SBE ROM / RoT logic.

Token should bind at least:

- signature
- nonce
- boot epoch
- expiration
- target mask
- allowed operation class
- read/write permission
- anti-replay counter

Production default should be read-mostly:

Allowed examples:

- READ_FIR
- READ_FFDC
- READ_TRACE
- FREEZE_CAPTURE
- START_DUMP
- GET_DUMP_STATUS

Arbitrary register writes, PLL manipulation, memory modification, or CPU-state modification should remain disabled unless a stronger signed manufacturing/development/RMA policy explicitly authorizes them.

The BMC takeover concept is inspired by POWER serviceability/pdbg/FSI-style access, including BMC GPIO/OpenFSI/SBEFIFO backends, but Jixia should implement a stricter authenticated privilege boundary.

## 12. BMC dump interface: control plane vs data plane

Do not move a large FFDC dump over a slow GPIO/I2C/MCTP command path byte-by-byte.

Use two planes:

### Control plane

- MCTP/PLDM or emergency sideband
- AUTH
- TAKEOVER
- FREEZE
- START_DUMP
- GET_STATUS

### Bulk data plane

- high-bandwidth shared window / PCIe / dedicated dump aperture
- DMA from IP-local FFDC SRAM into M-C/retention dump buffer
- BMC bulk reads staged dump data

A typical sequence:

1. BMC requests `START_DUMP(target_mask)`.
2. hardware/M-C gathers data to a dump buffer.
3. response includes dump ID, size, checksum, generation.
4. BMC performs bulk transfer over the high-bandwidth path.

## 13. Post-reset Jixia-H RAS harvest

When a fatal event forces reset, Jixia-H must inspect retained RAS state **very early** in boot, before later initialization clears or overwrites it.

Proposed early stage:

`PHASE_PREINIT_RAS_HARVEST`

Sequence:

1. read reset cause
2. test previous-crash-valid
3. harvest retention crash capsule
4. scan reset-retained FIRs
5. create previous-boot RAS context
6. persist/export to M-C/BMC
7. ACK/consume the capture generation
8. only then perform destructive reset/clock/PHY/FIR initialization

This follows the serviceability lesson visible in POWER Hostboot/PRDF: boot firmware itself participates in FIR scanning and error analysis.

## 14. QEMU-first implementation strategy

SimSOC will take time, so development should start with a functional QEMU server platform.

Initial QEMU topology:

- **Host QEMU** — RV64 SMP
- **Management QEMU** — RV32/RV64 SMP running Jixia-S
- **BMC QEMU** — ASPEED/OpenBMC
- **SBE** — initially a simple QEMU state-machine/device; later a real RV32 QEMU instance + firmware if useful

A small SimFabric/orchestration layer connects instances through mailbox, doorbell, shared SRAM, reset/power, MCTP, I2C/UART, RAS-event and service-fabric abstractions.

Early device behavior may be stubbed. The firmware-facing hardware contract must remain stable so later QEMU functional models can be replaced by SimSOC timing models or RTL without rewriting firmware.

## 15. Key architecture principles captured today

1. Keep only four first-class domains: **SBE, M-C, Host, BMC**.
2. Do not create separate SCP/MCP/OCC processors by default; make them M-C services.
3. Jixia microkernel is shared by Host and M-C via different profiles; SBE stays tiny and separate.
4. M-C owns SoC-level privileged serviceability; BMC owns board/OOB management.
5. Every significant IP must have a management/RAS interface, including FIR/FFDC visibility.
6. Diagnostic escape access must survive main-fabric failure.
7. Separate reliable emergency control from high-bandwidth bulk FFDC movement.
8. Preserve first-failure state in retained/always-on logic.
9. Add a hardware crash-capture path so fatal-state preservation does not depend on M-C software.
10. Retain both BMC break-glass live dump and reset-retained post-reboot harvest.
11. BMC emergency access requires hardware-verified signed authorization and least privilege.
12. Jixia-H must perform early previous-boot RAS harvest before destructive initialization.
13. Design dump bandwidth from FFDC-size/time requirements.
14. Start all of this in QEMU; later replace functional components with SimSOC timing/cycle models.

## 16. Reference directions for later study

Continue studying and comparing:

- RISC-V Server SoC / Server Platform specifications
- Arm `scp-firmware`: framework, modules, events, API binding, SCMI, SCP/MCP products
- POWER SBE
- POWER OCC
- Hostboot PRDF/RAS/FIR flows
- OpenPOWER `pdbg`, OpenFSI, GPIO-FSI, SBEFIFO and FFDC paths
- OpenBMC

The goal is to build a design-source matrix that explains what problem each reference architecture solves, what mechanism it uses, and which parts should or should not be adopted by Jixia.

## 17. Dependability is the umbrella, not only traditional RAS

Jixia should use a broader **dependability** model rather than treating RAS and performance as unrelated subsystems.

The theoretical baseline is Behrooz Parhami's *Dependable Computing: A Multilevel Approach*.

The source defines a dependable system in terms of delivering specified actions/results in a **trustworthy and timely** manner. From the user/service viewpoint, undependability can appear as late, incomplete, inaccurate, or incorrect results. Therefore, timeliness is part of dependability, not merely a separate performance concern.

For Jixia, this means:

- wrong result -> dependability problem
- missing result -> dependability problem
- service unavailable -> dependability problem
- result arrives too late to satisfy its service contract -> dependability problem
- large unexpected latency degradation can therefore be a dependability event even when no classical FIR/checkstop occurs

A useful operational rule is:

> **Machine alive != service dependable.**

A system may still execute instructions and run Linux while already violating the service expectations that make it useful.

## 18. Jixia multilevel impairment model

Adopt Parhami's multilevel sequence as a conceptual diagnostic backbone:

```text
Ideal
  -> Defective
  -> Faulty
  -> Erroneous
  -> Malfunctioning
  -> Degraded
  -> Failed
```

The levels provide a useful distinction between physical/component imperfections, logic deviations, information errors, architectural malfunctions, service degradation, and result-level failure.

Jixia should attempt to correlate observations across levels rather than stopping at the first visible symptom.

Example:

```text
PHY marginality / environment
        -> link retries
        -> PCIe service slowdown
        -> NoC/DDR queue pressure
        -> Host memory latency spike
        -> application SLA violation
```

The final SLA violation is the service-level symptom; the root cause may be far below it.

The architecture should therefore preserve enough cross-level evidence to reconstruct the chain.

## 19. Performability as a first-class Jixia target

Parhami explicitly discusses **performability** as the combined view of performance and reliability for systems that can operate in degraded states.

Jixia should adopt this idea as a core server metric:

```text
Dependability
    |
    +-- correctness / trustworthiness
    +-- timeliness / service performance
    +-- availability
    +-- diagnosability
    +-- recoverability
    +-- serviceability
```

Performance degradation is not automatically a hardware fault. The important distinction is:

- **normal variation**: behavior stays within expected/specification/historical envelope
- **degradation**: behavior is still functional but significantly worse than the expected service envelope
- **failure**: degradation exceeds the acceptable service contract or the useful service disappears

This makes Jixia's health model more expressive than a binary `healthy/failed` flag.

## 20. M-C dependability engine and hardware performance self-diagnosis

M-C should evolve from a traditional RAS controller into the always-on **Jixia Dependability Engine**.

Its job is not only to answer `Is something broken?`, but also:

- Is a service path becoming abnormally slow?
- Is there local congestion even when total bandwidth is below peak?
- Did a specific queue, credit pool, bank, home-node, memory channel, PCIe link, IOMMU walk engine, or cache miss structure become the bottleneck?
- Did hardware behavior deviate from this machine's own historical baseline?
- Is there hardware evidence correlated with a software-observed latency incident?

Each major SoC IP should expose low-overhead runtime observability suitable for always-on analysis, not only lab-only debug.

Examples include:

- queue occupancy and high-watermarks
- latency histograms / tail-latency buckets
- arbitration wait cycles
- downstream backpressure cycles
- credit starvation
- retry/replay counters
- MSHR/fill/writeback pressure
- DDR bank conflicts / row hit-miss / refresh stalls / read-write turnaround
- IOMMU IOTLB/page-walk/invalidation pressure
- PCIe traffic class / completion / replay / credit state
- CPU stall classes, throttling, frequency and thermal/power limits

Average-only counters are insufficient for rare latency spikes. Histogram/tail/high-watermark state is required because a one-in-ten-thousand outlier may disappear in an average.

All participating hardware and software event streams should share a common time base or an accurately correlatable time domain so M-C can reconstruct causal ordering.

## 21. Incident-triggered SoC observability

The architecture should use a low-overhead always-on path plus triggered fidelity escalation:

```text
NORMAL
  low-cost counters / histogram / breadcrumbs

SUSPICIOUS
  finer sampling / additional structured events

INCIDENT
  freeze pre-trigger history + enable high-resolution trace

FATAL
  freeze FFDC + crash capsule + recovery
```

This is conceptually a **SoC oscilloscope**: continuously retain a rolling pre-trigger window and increase observation fidelity when a performance/RAS anomaly appears.

The design must minimize Heisenberg effects. Always-on instrumentation should avoid formatted logging and heavyweight debug actions; use fixed-size binary records, hardware counters, timestamps, and short breadcrumbs.

## 22. Software-to-M-C performance incident marking

The Host OS/runtime should be able to tell M-C that software observed a latency/SLA incident.

A future Jixia Host performance agent may send a structured `PERF_MARK` containing timestamp, workload/flow identifier, affected CPU/device/context, and anomaly type.

M-C can then inspect the corresponding hardware window and answer with evidence such as:

```text
Hardware anomaly observed:
  DDR channel 3 queue saturation
  NoC node 17 downstream backpressure
  PCIe DMA burst correlated with incident

or

No abnormal hardware condition observed
within covered paths/time window;
inspect scheduler/lock/page-fault/NUMA/cgroup/software causes.
```

The wording must remain evidence-based. M-C should not claim absolute hardware health when observability coverage is incomplete.

## 23. Host semantic agent + M-C hardware evidence

M-C is authoritative for SoC hardware observation, but Host software provides higher-level semantics that M-C cannot infer directly.

Host evidence may include:

- scheduler delay / run-queue state
- IRQ and softirq activity
- lock contention
- page faults
- NUMA migration
- cgroup throttling
- VM exits
- driver reset / VFIO / IOMMU activity
- current boot/runtime phase and service context

The final diagnosis should combine:

```text
M-C hardware evidence
      +
Host software evidence
      +
Flight Recorder history
      =
Cross-layer causal diagnosis
```

This avoids the traditional `hardware says software / software says hardware` debugging loop.

## 24. Flight Recorder, memory, and rare-event diagnosis

Jixia should be designed with persistent/retained structured memory of important execution history.

The **Jixia Flight Recorder** should capture low-overhead structured events such as:

- boot ID / epoch
- timestamp
- hart/domain/service
- boot stage / state-machine state
- IPC/event/notification relationships
- key MMIO/device state changes
- FIR/FFDC references
- performance anomaly markers
- configuration/firmware version
- recovery action

The major goal is to diagnose rare failures and latency spikes that may occur once in thousands or tens of thousands of runs and cannot easily be reproduced.

The system should compare a failed/anomalous execution with previous normal runs and locate the **first divergence**, not merely report the last visible error.

M-C provides external observation of Host progress so evidence can survive Host software failure.

## 25. PRD-inspired RAS intelligence and update model

POWER Hostboot PRD/PRDF is a useful ancestor for Jixia's diagnostic engine because its rule system already separates hardware facts, capture recipes, analysis rules, callouts, and actions.

Jixia should evolve this concept into a hybrid engine containing:

- deterministic PRD-style rules
- FIR/register definitions
- FFDC capture recipes
- known error signatures
- causal templates
- thresholds / rate models
- known errata / workarounds
- local historical baseline
- statistical anomaly detection
- optional compact ML models
- recovery playbooks

The preferred execution/update roles are:

```text
M-C
  runtime inference / diagnosis / recovery authority

Host runtime agent
  high-level software and firmware semantic evidence

BMC
  persistent history / fleet gateway / package staging

Fleet or Lab service
  data aggregation / rule mining / model training /
  QEMU-SimSOC replay and validation
```

Learning and safety-critical execution must remain separated. A server may continuously learn its own normal baseline, but must not autonomously rewrite destructive recovery policy based only on local unsupervised observations.

## 26. Signed RAS knowledge packages

Define a future **Jixia RAS Knowledge Pack (JRKP)** as an independently updatable, signed diagnostic knowledge artifact.

A JRKP may contain:

- deterministic rules
- FIR metadata
- capture recipes
- callout/signature database
- causal templates
- thresholds / leaky-bucket parameters
- errata/workarounds
- recovery playbooks
- anomaly models
- compatibility metadata
- provenance/confidence/version
- cryptographic signature

BMC may transport/stage the package, but M-C/SoC trust logic must authenticate, anti-rollback-check, compatibility-check, and activate it.

New knowledge should support shadow/canary validation before promotion. QEMU and later SimSOC should be used to replay historical cases and fault/performance scenarios before a new JRKP is permitted to affect production recovery decisions.

## 27. Long-term Jixia dependability goal

The long-term goal is larger than `good RAS`.

Jixia should become a **self-observing, self-diagnosing dependable server** capable of answering evidence-based questions such as:

- Why did the previous boot fail?
- Why was this boot unusually slow?
- What was the first divergence from thousands of normal boots?
- Is this latency spike caused by hardware congestion or software scheduling?
- Which hardware path was congested?
- Is the machine still within its expected service envelope?
- Is an apparently healthy component slowly degrading relative to its own history?
- What evidence supports the diagnosis?
- What recovery was performed and why?

The architecture principle is:

> **Jixia should continuously know whether it is still delivering trustworthy and timely service, remember the evidence needed to explain deviations, and preserve enough state to learn from rare failures rather than merely survive them.**

## 28. Additional theoretical reference

Add the following book as a primary architectural reference for Jixia dependability work:

- Behrooz Parhami, *Dependable Computing: A Multilevel Approach*

Especially relevant themes:

- dependability as trustworthy and timely service
- multilevel impairment model
- malfunction diagnosis / self-diagnosis
- service degradation / graceful degradation
- performability
- degradation management
- failure awareness and learning from failures

This reference provides the conceptual umbrella under which Jixia's classic RAS, performance degradation analysis, serviceability, Flight Recorder, and self-diagnosis should be developed together rather than as unrelated features.
