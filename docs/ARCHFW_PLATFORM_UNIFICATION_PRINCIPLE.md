# ArchFW Platform Unification Principle v0.2

ArchFW is a cloud-platform firmware OS rather than a large BIOS replacement.

Canonical architecture: [ArchFW Microkernel and Firmware Architecture v0.2](ARCHFW_MICROKERNEL_FIRMWARE_ARCHITECTURE_V0.2.md).

## Core principle

> Different IP firmware, one platform language; one physical writer, explicit ownership, isolated execution.

The platform language consists of:

- stable Target IDs and typed attributes;
- explicit topology and dependency relationships;
- lifecycle, configuration, diagnosis and workload ownership;
- versioned Agent transactions;
- restartable isteps;
- structured FFDC and RAS evidence;
- a final OS-published topology.

## Architecture boundary

```text
ArchFW microkernel
  TCB / CSpace / VSpace / Frame
  Endpoint / Reply / Notification / IRQ
  Untyped/retype and fault IPC
        |
        v
Root Orchestrator and isolated services
  Targeting / PlatformGraph
  IStep engine
  Supervisor
  Trace / FFDC
  RAS Broker
  Hardware services
        |
        v
Remote Agents
  SBE / SCP / Memory / PCIe / RAS / BMC
        |
        v
EDK II -> EFI memory map + ACPI -> Linux/hypervisor
```

The microkernel contains no platform configuration policy, hardware drivers, istep tables, ACPI builder or diagnosis rules.

## Three platform views

### Static PlatformGraph

Compiled immutable facts:

- targets and physical hierarchy;
- MMIO and IRQ resources;
- power/clock/reset dependencies;
- coherent, PCIe, CXL and memory topology;
- security and RAS containment domains;
- Agent attachment and hardware capabilities.

### Runtime State Overlay

Boot and runtime state:

```text
PRESENT / FUNCTIONAL / INITIALIZED / POWERED
DEGRADED / QUARANTINED / DECONFIGURED
OWNER / OWNER_EPOCH
LAST_ERROR / LAST_SUCCESSFUL_STEP
```

### Published OS View

Only safe, available resources:

- enabled harts;
- usable memory;
- NUMA/cache topology;
- enabled PCIe/CXL roots and devices;
- interrupt and IOMMU relationships;
- RAS interfaces.

EFI memory maps and ACPI tables are generated from the Published OS View. They do not independently rediscover hardware.

## Configuration flow

CUE is the human-maintained source language. Early firmware does not parse CUE.

```text
CUE platform source
        |
        v
fwcfg compiler
        |
        +-- Platform IR
        +-- generated Target IDs and typed attributes
        +-- Agent manifests
        +-- simulator model
        +-- ACPI-builder inputs
```

Configuration layers remain:

```text
silicon -> board -> product -> policy -> validated runtime override
```

A runtime override may change policy or desired state but cannot invent physical resources absent from the compiled graph.

## Ownership model

A Target declares four roles:

- Lifecycle Owner: power, clock, reset and lifecycle transitions;
- Configuration Owner: register programming and HWP execution;
- Diagnosis Owner: evidence analysis, root cause and action planning;
- Workload Owner: OS, hypervisor or accelerator runtime using the resource.

A physical control register has one writable owner at a time. Other components use an Endpoint/Agent request or receive read-only evidence.

Kernel capabilities enforce actual access. Platform ownership leases enforce the current lifecycle policy. Destructive operations require both.

## Hardware services

Platform-specific hardware support runs in isolated services or remote Agents:

- core/cache;
- DDR and memory-controller;
- power/clock/reset;
- interrupt routing;
- PCIe/CXL;
- IOMMU;
- security.

QEMU fake hardware follows the same service and Agent contracts. Test-only direct function calls do not define the production architecture.

## RAS boundary

```text
hardware containment
  -> local immutable evidence snapshot
  -> independent RAS Processor
  -> action manifest
  -> HostFW RAS Broker
  -> Target overlay and istep reconfiguration
  -> Linux/hypervisor recovery
```

HostFW does not duplicate the complete platform diagnosis engine when the independent RAS domain exists.

## Simulator integration

ArchLab RVSoC-Sim can consume the same generated Platform IR and Agent contracts to model:

- topology and resource ownership;
- CPU/cache/memory/NoC;
- PCIe/CXL/IOMMU;
- power/clock/reset;
- Agent latency, reset and stale completion;
- RAS containment and evidence;
- firmware-in-the-loop boot and reconfiguration.
