# ArchFW Common RAS Deployment Model v0.1

## 1. Problem statement

ArchFW must support several server products with different management hardware:

- a high-end platform with an always-on dedicated RAS processor;
- a system with a capable external service processor;
- a commodity platform with only a normal BMC;
- a platform with no out-of-band processor, where host firmware performs diagnosis;
- QEMU and development systems.

The RAS architecture therefore cannot depend on the presence of an IBM-style FSP.

The design goal is:

> One rule source and one diagnosis model, with several controlled execution profiles.

---

## 2. Common engine versus deployment adapter

```text
Declarative rules + typed plugins
              |
          Common RAS IR
              |
      Common RAS Engine
              |
  +-----------+-----------+----------------+
  |                       |                |
RSP adapter          Host adapter     Offline adapter
  |                       |                |
RAS core          AFRT/HBRT-like       replay/tooling
```

The following are common across all profiles:

- topology and propagation graph;
- first-error and secondary-error analysis;
- source adapters for FIR/RERI/PCIe/CXL/memory/core/fabric;
- threshold state format;
- callout and confidence model;
- action-intent format;
- persistent evidence format;
- rule and plugin tests;
- diagnosis ownership protocol.

Only the execution environment and available host services vary.

---

## 3. Deployment profiles

## 3.1 RSP_PRIMARY

A dedicated always-on RISC-V RAS processor is the normal diagnosis owner.

Responsibilities:

- receive sideband attention/doorbell events;
- read RAS apertures across the SoC;
- preserve first-error evidence while host harts stop/reset;
- execute the complete Common RAS Engine;
- maintain thresholds and persistent service history;
- generate callouts, repair plans, and boot recovery manifests;
- coordinate with SCP/MCP/BMC;
- publish action intents to host firmware and OS.

Host responsibility:

- source-hart local architectural capture;
- synchronous execution-context recovery;
- standard ACPI/GHES/CPER delivery;
- execution of host-owned actions;
- fallback diagnosis when ownership transfers.

This is the preferred profile for high-end cloud and mission-critical systems.

## 3.2 HOST_PRIMARY

No FSP-class sideband diagnosis processor exists.

Preferred runtime implementation:

```text
reserved firmware hart
    M-mode AFRT/OpenSBI
    U-mode RAS Runtime Service
```

Fallback implementation when no hart can be reserved:

```text
selected host hart
    -> bounded M-mode AFRT entry
    -> signed HBRT-like RAS capsule
    -> slice work by budget
    -> return to S/HS-mode
```

The host runtime must not depend on a customer daemon. A cloud bare-metal customer may boot an arbitrary supported OS and still receive platform-level RAS handling.

The normal BMC receives records and supports remote service, but does not become diagnosis owner unless it meets the `EXTERNAL_SP_PRIMARY` requirements.

## 3.3 EXTERNAL_SP_PRIMARY

An external service processor may become primary only when all of the following are true:

- it survives host-domain reset/checkstop;
- it has secure boot and authenticated update;
- it has low-level sideband access to required RAS apertures;
- access latency is acceptable for evidence preservation;
- it has sufficient memory and execution capability for the rule engine;
- it can maintain persistent thresholds and event epochs;
- its control authority is capability-scoped;
- it can coordinate with host firmware without clearing evidence early.

A BMC that only provides sensors, IPMI/Redfish, remote console, and event logs does not satisfy these requirements.

## 3.4 BOOT_FALLBACK

Every ArchFW host image contains the boot-capable Common RAS Engine.

At boot it can:

- validate an RSP/service-processor recovery manifest;
- analyze the previous reset/checkstop if no valid manifest exists;
- read persistent first-error records;
- apply deconfiguration;
- update Targeting health state;
- choose the istep reconfiguration entry point;
- rebuild available topology;
- decide whether boot may continue.

This capability is mandatory even on `RSP_PRIMARY` systems.

---

## 4. Diagnosis ownership

Exactly one owner may finalize an event at any time.

```text
DIAG_OWNER =
    RSP
    EXTERNAL_SP
    HOST_RUNTIME
    HOST_BOOT
    OFFLINE_TOOL
```

Every event includes:

- Event ID;
- correlation ID;
- diagnosis-owner identity;
- owner epoch;
- evidence generation;
- topology generation;
- rule version;
- source-adapter version;
- lifecycle state.

Only the current owner may:

- perform final root-cause selection;
- clear or rearm the primary evidence source;
- commit persistent GARD/deconfiguration;
- issue final callout;
- close the event.

Ownership transfer requires an epoch change and an immutable handoff record. A stale owner must reject further writes.

---

## 5. Evidence access model

A RAS execution environment receives access to error evidence through typed host services, not unrestricted physical access.

```c
struct RasHostServices {
    read_record(source, record_id);
    snapshot_source(source);
    mask_source(source, bits);
    clear_source(source, evidence_generation);
    rearm_source(source);
    read_topology(target);
    persist_evidence(event);
    publish_cper(event);
    submit_action(intent);
    agent_request(agent, request);
};
```

The service must not expose arbitrary primitives such as:

```text
read_physical_address(anywhere)
write_scom_or_mmio(anywhere)
halt_any_hart()
read_guest_memory()
read_security_key_register()
```

---

## 6. RAS access aperture

Every hardware domain should expose a sideband-readable, capability-scoped RAS aperture.

Minimum classes:

```text
Identification
    source ID
    target ID
    topology generation

Evidence
    FIR/RERI record
    WOF/first-error state
    address and syndrome
    requester/transaction ID
    poison metadata
    retry/recovery status
    frozen trace pointer

Control
    mask
    snapshot
    clear with generation check
    rearm
    fence domain
    request reset/deconfigure

Status
    owner epoch
    evidence generation
    source frozen/active state
    overflow/lost-evidence flag
```

The RAS processor receives a god's-eye view of error evidence, not a god's-eye view of host data.

---

## 7. Core-private evidence

A sideband processor often cannot directly read application-hart local CSRs/register state. Therefore the architecture must create an explicit snapshot path:

```text
core detector
    -> local recovery unit prevents unsafe commit
    -> immutable Core RAS Snapshot
    -> sideband-visible SRAM/aperture
    -> doorbell to diagnosis owner
```

Suggested snapshot fields:

- hart and core ID;
- reporter/source code;
- first-error indicator;
- PC and privilege context when architecturally permitted;
- VMID/ASID/security-domain tag, not guest memory;
- checkpoint/retry ID;
- recovery result;
- poison/transaction identifier;
- secondary-error bitmap;
- evidence generation and checksum.

Fast local recovery remains hardware responsibility. The external rule engine cannot replace a recovery unit when the FIR path is too slow to prevent corruption.

---

## 8. Runtime execution rules

### 8.1 Immediate path

The host M-mode path performs only:

```text
capture
contain
mask repeated entry
create immutable event
forward
resume, isolate, or park
```

It must not perform:

- global topology scans;
- thousands of register reads;
- full rule traversal;
- FRU inventory lookup;
- BMC waits;
- CPER formatting of large records;
- repair planning.

### 8.2 Asynchronous path

```text
machine event
    -> AFRT creates Event ID
    -> event queued to current diagnosis owner
    -> owner executes rule engine
    -> owner emits Action Intent
    -> AFRT/Linux/SCP/BMC executes owned actions
    -> final CPER/service log/persistent record
```

A synchronous SBI call must never wait for the complete diagnosis. Submit operations return a transaction ID.

### 8.3 Runtime budget

When rules execute on a customer host hart:

- work is divided into bounded slices;
- each slice has a cycle/time budget;
- no slice waits on BMC or remote transport;
- unfinished work is checkpointed;
- watchdog aborts an unbounded plugin;
- normal host execution resumes between slices when containment permits.

This path is for rare platform errors, not periodic telemetry.

---

## 9. Boot recovery flow

Preferred path with a live RAS processor:

```text
checkstop/reset
    -> RSP preserves evidence
    -> RSP diagnoses
    -> RSP persists Boot Recovery Manifest
    -> host reset
    -> ArchFW boot validates manifest
    -> Targeting applies deconfiguration
    -> istep resumes from defined entry point
    -> UEFI publishes reduced ACPI topology
```

Fallback path:

```text
host reset
    -> ArchFW reads reset reason and persistent journal
    -> Host Boot RAS runs Common RAS Engine
    -> creates local recovery decision
    -> applies deconfiguration and reconfiguration loop
```

The host must not independently clear residual FIR/RERI state before ownership and evidence generation are established.

---

## 10. Hostboot/HBRT-style interface

The host runtime capsule follows the useful HBRT pattern of two explicit interfaces.

### Environment services supplied to the engine

```text
read/write controlled error records
map approved evidence region
allocate bounded scratch memory
persist error log
publish CPER/GHES
request memory/CPU/device offline
send agent request
read runtime topology snapshot
```

### Engine entry points

```text
enable_attention_sources()
disable_attention_sources()
handle_attention(event_id)
analyze_last_boot(evidence_id)
process_threshold_tick()
query_diagnosis(event_id)
```

Unlike a legacy unrestricted runtime image, the capsule receives only a generated capability manifest and a stable versioned host-service table.

---

## 11. Standard OS interfaces

The customer OS does not need a proprietary daemon for basic correctness.

Standard northbound path:

- ACPI HEST;
- GHES/GHESv2 error status blocks;
- CPER records;
- BERT for previous-boot evidence;
- ERST when persistent record access is supported;
- synchronous architectural exception for consumed poison or execution-context errors;
- standard CPU, memory, and device offlining where Linux supports it.

Private SBI/AFRT calls are optional management and diagnostic extensions. The machine remains safe if the customer kernel never invokes them.

---

## 12. Common engine policy split

```text
Common mechanism
    rule interpreter
    topology traversal
    source adapters
    evidence lifecycle
    plugin sandbox
    callout/action formats

Boot policy
    deconfigure before publication
    re-enter istep
    rebuild topology
    decide bootability

Runtime policy
    preserve host latency
    contain and offline
    publish CPER
    defer repair
    persist next-boot action

Service policy
    FRU callout
    predictive maintenance
    fleet threshold
    repair scheduling
```

Do not encode product policy directly in hardware source adapters.

---

## 13. First QEMU proof

The first end-to-end demonstration should include:

1. QEMU custom RAS MMIO device with FIR/RERI-like records.
2. First-error journal and evidence generation counter.
3. Injected L2 BIST failure on one hart.
4. Host Boot RAS analysis using the Common Engine.
5. Target marked deconfigured.
6. Step Engine re-enters core/cache initialization.
7. Available topology is rebuilt.
8. EDK II publishes ACPI without the failed hart.
9. Linux boots and reports the reduced CPU count.
10. BERT or boot log exposes the original event and diagnosis.

A later test switches the same rules to an emulated dedicated RAS agent without changing rule content.

---

## 14. Final decision

> ArchFW always carries a host-capable Common RAS Engine. High-end systems normally execute it on a dedicated RAS processor; commodity systems execute it in a reserved host runtime or a bounded HBRT-like capsule; a capable external service processor may own it only when it has FSP-class evidence access and lifecycle independence. The rule source, evidence model, and ownership protocol remain common.
