# Jixia OpenPOWER Tooling Reference

## 1. Purpose

This document records a first-pass survey of OpenPOWER tooling that may be useful to the Jixia/Firmware project.

The goal is **not** to copy the OpenPOWER software stack wholesale. The goal is to preserve useful engineering patterns and candidate reference implementations so that later milestones do not need to rediscover them from scratch.

The main areas covered are:

- hardware debug and target access;
- platform target/attribute configuration;
- structured error logging and trace decoding;
- flash/image layout and image assembly;
- RAS deconfiguration/guarding;
- secure boot and key management;
- power/thermal management;
- system validation and performance tooling;
- historical OpenPOWER configuration and platform utilities.

This is a **research checkpoint**. Before reusing code or interfaces in a concrete milestone, re-check the relevant upstream repository, license, current maintenance state, and processor-generation assumptions.

---

## 2. High-level conclusion

The most useful OpenPOWER tooling for Jixia can be reduced to four engineering infrastructure families:

```text
                         Jixia Firmware
                              |
        +---------------------+---------------------+
        |                     |                     |
  Target / Config          Debug / Access       RAS / Trace
  pdata-style model       pdbg/eCMD model     errl/fsp-trace
        |                     |                     |
        +---------------------+---------------------+
                              |
                      Image / Flash tooling
                    FFS + op-image-tools
```

The most important lesson is that Hostboot itself is only part of a production firmware system. A usable server firmware project also needs a coherent **configuration, debug, diagnostics, RAS, image-building and validation ecosystem**.

---

## 3. Priority classification

### S — directly useful to our architecture

| Project | Role | Jixia value |
|---|---|---|
| `pdbg` | POWER processor debug through FSI/SCOM and related backends | Reference for a unified low-level debugger and hierarchical target model |
| `eCMD` | IBM Systems hardware-access API | Reference for a stable hardware-access API above pluggable backends |
| `ecmd-pdbg` | glue between eCMD and pdbg | Demonstrates how a generic API can reuse a concrete hardware backend |
| `pdata` | target/attribute metadata and POWER system device-tree generation | Strong reference for Jixia target tree, attributes and generated platform metadata |
| `errl` | structured error-log parser/tooling | Reference for persistent structured RAS logs, FFDC and tooling |
| `fsp-trace` | binary trace decoder using string/hash data | Reference for compact firmware trace buffers plus host-side decoding |
| `ffs` | OpenPOWER flash file structure utilities | Reference for a firmware flash/partition format |
| `op-image-tools` | config-driven image merge/build/sign flow | Reference for reproducible Jixia image assembly |
| `guard` | persistent isolation of faulty hardware units | Reference for RAS deconfiguration/guard records and reboot-time isolation |

### A — important for later milestones

| Project | Role | Likely use |
|---|---|---|
| `libekb_p10` | P10 EKB/HWPF/HWP integration support | Reference when Jixia gains HWP/istep-like hardware procedures |
| `occ` | On-Chip Controller firmware | Power, thermal and frequency-management research |
| `hcode` | processor-specific low-level firmware/code | Deep POWER bring-up and power-management reference; not a generic framework |
| `HTX` | Hardware Test Executive | System validation once Jixia can boot a full OS and exercise CPU/memory/I/O |
| `sb-signing-utils` | OpenPOWER secure/trusted-boot container signing | Secure-boot image/container design |
| `sb-signing-framework` | signing workflow/framework | Later production-signing pipeline reference |
| `secvarctl` | host/guest Secure Boot variable tooling | PK/KEK/db/dbx-style secure-variable lifecycle after secure boot exists |
| `vpdtools` | VPD image creation/reversal | FRU/board/DIMM/system inventory and manufacturing data |

### B — useful secondary or historical references

| Project | Role / note | Use policy |
|---|---|---|
| `pnor` | older PNOR image creation/update scripts and layouts | Historical reference; prefer FFS + newer image tooling for new design |
| `serverwiz` | MRW authoring/update tool using XML libraries | Understand OpenPOWER platform-data flow; do not copy its XML-heavy design by default |
| `hostboot-targeting` | generated/board-specific Hostboot target XML | Reference for historical target representation and physical topology |
| `openpower-mrw` | old MRW package | Upstream README explicitly says it is unused; historical reference only |
| `hw-trace` | POWER hardware trace/HTM/XSCOM utilities | Deep bring-up/performance/debug stage |
| `libopalevents` | OPAL event support library | RAS/event ecosystem reference when OPAL-style events are studied |
| `amester` | remote power/thermal/performance metric collection | Power/thermal validation and telemetry concepts |
| `power-pmu-events` | POWER PMU event descriptions | Performance-analysis reference on POWER hardware |
| `op-benchmark-recipes` | benchmark recipes | Later validation/performance methodology |
| `kexec-lite` | small kexec-related utility | Boot/reboot-flow reference if needed |
| `docs` | OpenPOWER documentation repository | Supporting documentation for OCC, platform interfaces and firmware behavior |

### C — specialized, only study when the matching feature appears

| Project | Area |
|---|---|
| `ultravisor` | secure virtualization / protected execution |
| `svm-tools` | secure VM tooling |
| `capiflash` | CAPI flash stack |
| `capi2-flashgt` | CAPI2/FlashGT-specific tooling |
| `ocmb-explorer-fw` | Explorer OCMB firmware |
| `op-utils` | small OpenPOWER utility repository; defer detailed study until a concrete dependency appears |

---

## 4. Key reference projects

### 4.1 `pdbg`: model for Jixia hardware debugger

Repository: https://github.com/open-power/pdbg

`pdbg` is a simple application for debugging host POWER processors from the BMC. Its README describes access to GPRs, SPRs and system memory, and the command set includes operations such as:

- `getscom` / `putscom`;
- `getcfam` / `putcfam`;
- `getmem` / `putmem`;
- `getgpr` / `putgpr`;
- `getspr` / `putspr`;
- `getnia` / `putnia`;
- `start`, `stop`, `step`;
- `threadstatus`;
- target probing and selection.

A particularly valuable idea is the hierarchical target model:

```text
proc
 `- pib
     `- chiplet
         `- eq
             `- ex
                 `- core
                     |- thread0
                     |- thread1
                     |- thread2
                     `- thread3
```

#### Jixia direction

Do not copy POWER/FSI assumptions into the firmware core. Instead, preserve the abstraction:

```text
jdbg
 |- simulator backend
 |- MMIO backend
 |- BMC/debug-agent backend
 |- future JTAG/FSI-like backend
 `- optional remote backend
```

Candidate high-level commands:

```text
jdbg target show
jdbg reg read/write
jdbg spr read/write
jdbg mem read/write
jdbg cpu stop/start/step/status
jdbg fabric read/write
```

This can eventually give QEMU, our own simulator and real hardware the same logical debug interface.

---

### 4.2 `eCMD` and `ecmd-pdbg`: generic hardware-access layer

Repositories:

- https://github.com/open-power/eCMD
- https://github.com/open-power/ecmd-pdbg

`eCMD` describes itself as the hardware access API for IBM Systems. Its architecture is roughly:

```text
 Python / CLI / Perl
         |
      eCMD API
         |
      plugin
         |
      hardware
```

`ecmd-pdbg` is explicitly the glue needed to use `pdbg` as an eCMD plugin.

#### Jixia direction

Start smaller than eCMD. Use `pdbg` as the nearer-term inspiration, but design the Jixia debugger so that the low-level access layer can later become a reusable C API instead of being tied to a CLI.

---

### 4.3 `pdata`: platform target and attribute pipeline

Repository: https://github.com/open-power/pdata

`pdata` provides tools/libraries to manage the POWER system device tree and generate metadata from processor-specific targets/attributes plus system MRW XML. Its documented outputs include:

- `attributes_info.db`;
- `attributes_info.H`;
- POWER system CEC device tree.

The generated system device tree is used for POWER-server initialization and boot.

#### Jixia direction

Adopt the **generation pipeline**, not necessarily MRW/XML itself.

Possible Jixia model:

```text
soc.yaml
board.yaml
platform.yaml
    |
    v
config generator
    |
    +--> generated headers
    +--> target database
    +--> device tree / platform blob
    `--> validation report
```

Runtime target tree example:

```text
system
 |- socket0
 |   |- cpu/core hierarchy
 |   |- memory controllers
 |   `- PCIe roots
 `- socket1
     |- cpu/core hierarchy
     |- memory controllers
     `- PCIe roots
```

Candidate attributes:

```text
ATTR_PHYS_PATH
ATTR_CHIP_ID
ATTR_SOCKET_ID
ATTR_NUMA_ID
ATTR_MMIO_BASE
ATTR_FREQ
ATTR_PRESENT
ATTR_FUNCTIONAL
ATTR_GUARDED
```

The same target identity should be shared by bring-up, RAS, debug, VPD and guard logic.

---

### 4.4 `errl` + `fsp-trace`: structured diagnostics

Repositories:

- https://github.com/open-power/errl
- https://github.com/open-power/fsp-trace

`errl` parses persistent binary error-log entries and supports structured decoding. The source also shows integration with trace user-data sections and external string files.

`fsp-trace` is a parser for binary trace buffers. It uses a hash/trex string file to associate compact trace identifiers with readable strings.

#### Jixia direction

Keep three diagnostic levels distinct:

```text
1. Console / printk
   - immediate bring-up feedback

2. Structured trace
   component + timestamp + trace-id + arguments
   -> compact binary ring buffer
   -> host-side string database + decoder

3. Structured RAS error log
   error/event id
   severity
   module/reason
   target
   callouts
   user data / FFDC
   trace snapshot
```

This prevents the project from evolving into a firmware where every diagnostic path is only a formatted `printf()`.

---

### 4.5 `ffs` + `op-image-tools`: image and flash layout

Repositories:

- https://github.com/open-power/ffs
- https://github.com/open-power/op-image-tools

`ffs` provides the OpenPOWER/FSP Flash File Structure used for flash layout.

`op-image-tools` provides a newer config-driven image builder that can merge components, use overrides/prebuilt inputs, build dependencies, and digitally sign output images.

#### Jixia direction

Eventually replace ad-hoc binary concatenation with a declarative image description such as:

```text
Jixia image
 |- BOOT        boot code
 |- FWCORE      main firmware
 |- CONFIG      generated platform data
 |- TRACE       trace string database / metadata
 |- VPD         manufacturing/platform data
 |- GUARD       persistent guard records
 |- ERRLOG      persistent error-log area
 `- SECURITY    manifest/signature/container metadata
```

The exact format is not decided here; the important lesson is to make layout explicit, versioned and tool-generated.

---

### 4.6 `guard`: close the RAS loop

Repository: https://github.com/open-power/guard

`guard` stores records for permanently isolated faulty components and provides create/list/invalidate/reset operations. It identifies resources through physical target paths.

#### Jixia direction

Target identity should connect error detection to future-boot configuration:

```text
hardware error
     |
     v
structured ERRL + FFDC
     |
     v
callout target
     |
     v
GUARD/deconfigure record
     |
     v
next boot: target PRESENT but not FUNCTIONAL
```

This is a useful model for CPU/core/memory-channel/PCIe-device deconfiguration and recovery policy.

---

## 5. Later feature families

### 5.1 Secure boot

Relevant repositories:

- `sb-signing-utils`;
- `sb-signing-framework`;
- `secvarctl`;
- later `ultravisor` / `svm-tools` for secure virtualization.

`sb-signing-utils` documents the OpenPOWER secure/trusted-boot container approach, including hardware/software signing-key hierarchy and a multi-step container signing workflow.

`secvarctl` focuses on manipulating and generating host/guest Secure Boot variables, including PK, KEK, db and dbx-style variables.

**Use later:** after the basic boot chain, recovery chain and image format are stable.

### 5.2 Power/thermal management

Relevant repositories:

- `occ`;
- `hcode`;
- `amester`.

`OCC` is the On-Chip Controller firmware and produces its own trace string file (`occStringFile`). `AMESTER` remotely gathers power/thermal/performance metrics from service-processor interfaces without consuming host POWER CPU cycles.

**Use later:** when Jixia gains a power-management agent and telemetry framework.

### 5.3 Validation

Relevant repositories:

- `HTX`;
- `op-benchmark-recipes`;
- `power-pmu-events`;
- `hw-trace`.

HTX is particularly valuable as a methodology reference: it concurrently stresses CPU, memory and I/O to expose hardware design flaws and hardware/software interaction issues.

**Use later:** after Linux/full-system boot is reliable enough for sustained stress.

---

## 6. Historical configuration ecosystem

The older OpenPOWER configuration path is still valuable for understanding how production firmware represented hardware:

```text
Serverwiz / MRW XML
        |
        v
hostboot-targeting / generated target data
        |
        v
Hostboot runtime target model
```

However:

- `openpower-mrw` explicitly states it is no longer used;
- `serverwiz` is XML/Java tooling from an older generation;
- `hostboot-targeting` contains system-specific generated XML artifacts.

For Jixia, use these as **architectural history**, while preferring a simpler declarative source format and generated runtime metadata.

---

## 7. Suggested adoption order

Do not create all of these subsystems now. Introduce them when the firmware reaches the matching pressure point.

1. **Target model foundation** — use `pdata`/Hostboot targeting as design references.
2. **Unified debug access** — `pdbg` first, with eCMD-style API separation kept in mind.
3. **Structured trace** — compact trace records plus host decoder inspired by `fsp-trace`.
4. **Structured ERRL/FFDC** — add when RAS paths become real rather than toy errors.
5. **Declarative firmware image** — `ffs` + `op-image-tools` concepts when multiple persistent sections appear.
6. **GUARD/deconfigure** — once real recoverable hardware faults and target state exist.
7. **Secure boot/signing** — only after the image and boot/recovery chain are stable.
8. **OCC/power agent** — when platform power/thermal control becomes a milestone.
9. **HTX/system validation** — once full-system boot is mature.

---

## 8. Design principles to carry forward

The OpenPOWER ecosystem suggests several durable rules for Jixia:

- **One target identity across subsystems.** Debug, initialization, RAS, guard, VPD and telemetry should refer to the same hardware object model.
- **Separate hardware access from user-facing tools.** A CLI should sit above a reusable access API/backend model.
- **Keep human strings out of hot firmware paths when possible.** Compact trace IDs plus host-side decode data scale better.
- **Make persistent errors structured.** Error logs need machine-readable IDs, target identity, severity, FFDC and callouts.
- **Make image construction reproducible.** Partition layout and signing should be declarative and generated, not hidden in shell concatenation.
- **RAS must affect future configuration.** Detecting an error is incomplete without a way to deconfigure/guard the faulty target.
- **Configuration should be generated from a source of truth.** Avoid hand-maintained duplicated constants across firmware modules.
- **Validation tooling is part of the firmware project.** Bring-up is not finished merely because the machine boots once.

---

## 9. Source repositories from the initial survey

```text
https://github.com/open-power/pdata
https://github.com/open-power/hcode
https://github.com/open-power/pdbg
https://github.com/open-power/sb-signing-utils
https://github.com/open-power/op-image-tools
https://github.com/open-power/occ
https://github.com/open-power/guard
https://github.com/open-power/pnor
https://github.com/open-power/sb-signing-framework
https://github.com/open-power/fsp-trace
https://github.com/open-power/libekb_p10
https://github.com/open-power/op-utils
https://github.com/open-power/ultravisor
https://github.com/open-power/secvarctl
https://github.com/open-power/ecmd-pdbg
https://github.com/open-power/docs
https://github.com/open-power/eCMD
https://github.com/open-power/ocmb-explorer-fw
https://github.com/open-power/serverwiz
https://github.com/open-power/vpdtools
https://github.com/open-power/svm-tools
https://github.com/open-power/amester
https://github.com/open-power/HTX
https://github.com/open-power/capi2-flashgt
https://github.com/open-power/capiflash
https://github.com/open-power/op-benchmark-recipes
https://github.com/open-power/kexec-lite
https://github.com/open-power/errl
https://github.com/open-power/hw-trace
https://github.com/open-power/ffs
https://github.com/open-power/openpower-mrw
https://github.com/open-power/libopalevents
https://github.com/open-power/hostboot-targeting
https://github.com/open-power/power-pmu-events
```

---

## 10. Checkpoint status

This document is intentionally a **reference map**, not a commitment to implement any particular OpenPOWER interface.

When a future Jixia milestone reaches one of these areas, use this document as the starting index, then perform a focused source study of the relevant repository and record the resulting concrete design decision in that milestone's architecture note.
