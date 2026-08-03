# ArchFW Platform Unification Principle v0.1

## Vision

ArchFW is a Cloud Platform Firmware OS, not a traditional BIOS replacement.

The goal is to provide one unified platform control plane for heterogeneous SoCs:

- CPU
- Memory
- PCIe
- CXL
- GPU
- NPU
- DPU
- Security engines
- Power and management controllers

IP implementations may differ, but platform contracts must be unified.

## Core Principle

> Different IP firmware, one platform language.

Every hardware agent must expose:

- Configuration contract
- Capability model
- Lifecycle model
- RAS interface
- Debug interface
- Ownership model

## Architecture

```
Linux / Hypervisor
        |
        | ArchFW API
        |
ArchFW Microkernel
        |
+----------------------------+
| Platform Services          |
|                            |
| PlatformGraph / Targeting  |
| Step Engine                |
| RAS Engine                 |
| Settings Service           |
| Debug Plane                |
+----------------------------+
        |
+----------------------------+
| Hardware Agents            |
|                            |
| SBE  SCP  OCC  GPU  DPU    |
| CXL  PCIe  Memory Agents   |
+----------------------------+
```

## Platform Model

Use a unified PlatformGraph inspired by IBM Hostboot Targeting.

The graph describes:

- targets
- relationships
- attributes
- runtime state
- ownership
- health

All firmware components, RAS, debug and configuration are based on this model.

## Configuration

Single source of truth:

```
CUE Platform Model
        |
        v
fwcfg compiler
        |
+----------------+
| PlatformGraph  |
| Agent Manifest |
| ACPI / DT      |
| BMC inventory  |
| Simulator model|
+----------------+
```

Configuration layers:

```
Silicon
  |
Board
  |
Project
  |
Policy
  |
Runtime Override
```

## Microkernel Role

The microkernel provides:

- scheduling
- IPC
- memory isolation
- capability management
- interrupt handling
- service supervision
- debug control

Reference ideas:

- seL4: capability and isolation
- QNX: service model
- Zircon: handles and channels
- Hostboot: firmware OS concept

## Device Model

ArchFW should provide its own device service framework.

Platform-specific drivers:

- DDR controller
- PCIe root complex
- IOMMU
- CXL controller
- power/clock/reset

Standard device services:

- NVMe
- Ethernet
- USB
- GPU interface
- Storage devices

Drivers should run as isolated services, not directly inside the kernel.

## RAS Fabric

All IP errors are converted into a unified event model.

```
Hardware IP
    |
RAS Adapter
    |
RAS Fabric
    |
RAS Case Engine
```

Unified RAS covers:

- CPU MCA-like errors
- Memory ECC
- PCIe AER
- CXL poison
- GPU/NPU/DPU failures

## Debug Plane

Firmware must provide modern observability:

- tracing
- crash dump
- memory corruption detection
- fault injection
- checkpoint/replay
- live debug
- probe framework

## RVSoC-Sim Integration

RVSoC-Sim-v2 should execute ArchFW models with:

- CPU
- cache
- memory
- PCIe
- CXL
- agents
- RAS events
- debug framework

The simulator is not only hardware simulation, but firmware-in-the-loop platform simulation.
