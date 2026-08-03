# ArchFW Cloud Firmware Architecture v0.1

## Vision

ArchFW is a cloud server firmware architecture inspired by IBM OpenPOWER Hostboot, coreboot, UEFI/PI, LinuxBoot/Petitboot, seL4/QNX/Zircon microkernel concepts, MCTP/PLDM/SPDM management protocols, and modern heterogeneous SoC requirements.

The goal is not to invent a new firmware universe, but to combine proven ideas into a firmware platform for CPU + GPU + NPU + DPU + CXL + PCIe heterogeneous servers.

## Core Principles

1. Platform model first
2. Host firmware owns global orchestration
3. Local agents own local containment
4. RAS is distributed but unified
5. Configuration is data driven
6. Debug/replay is a first-class capability
7. Firmware components communicate through versioned protocols

## High Level Architecture

```
Boot0
 |
Early Executive
 |
DRAM_READY transition
 |
Protected Host Firmware OS
 |
ServiceRoot Linux
 |
Hypervisor / Cloud OS
```

## Platform Configuration

Source configuration uses CUE.

Layers:

- Silicon description
- Board description
- Project customization
- Product policy
- Runtime override

The compiler generates:

- Targeting/PlatformGraph database
- Host firmware headers
- SBE/SCP/OCC manifests
- ACPI/DT views
- BMC inventory
- RVSoC-Sim topology

## Targeting / PlatformGraph

Targeting is retained as the runtime hardware object model.

It contains:

- Target types
- Instances
- Relations
- Attributes
- Ownership
- Health state

Static platform graph and runtime state are separated.

## Microkernel Direction

The kernel should not directly copy Hostboot kernel implementation.

Reference:

- Hostboot: firmware OS model, services, istep, RAS
- seL4: capability, isolation, IPC
- Zircon: handles, objects, services
- QNX: synchronous IPC/resource manager concepts

Preferred implementation:

- freestanding C++ kernel/services
- C-compatible minimal bootstrap boundary
- typed IPC
- capability based access

## Component Communication

Separate:

### Module ABI

Used only for native dynamically loaded components.

Prefer:

- ArchIDL defined interfaces
- generated C++ bindings
- minimal bootstrap ABI

### Agent Protocol

Used between firmware processors:

HostFW
SBE
SCP
OCC
GPU FW
NPU FW
DPU FW
BMC

Protocol concepts:

- Request
- Accepted
- Progress
- Completion
- RetryLater
- Transaction ID
- Boot Epoch
- Ownership
- Capability discovery

Transport can vary:

- shared memory ring
- mailbox
- MCTP/PLDM
- simulator transaction model

## SBE Model

SBE is modeled as an independent chip control agent.

Responsibilities:

- bootstrap
- chip operations
- core control
- register/ring access
- contained memory transition
- dump/recovery
- low level FFDC

Important states:

- IPL
- ISTEP
- Runtime
- Dump
- MPIPL
- Quiesce

## Cache Contained Transition

POWER-like contained mode is modeled as:

1. Host executes from contained memory
2. DDR initialization completes
3. Host execution is quiesced
4. Memory route switches
5. Cache contents are cast out
6. Host resumes with preserved address space

Alternative transition modes:

- in-place castout
- restart from DRAM handoff
- SCP initialized DRAM

## Distributed RAS Architecture

RAS layers:

```
Hardware RAS
   |
Local RAS Agent
   |
RAS Broker
   |
HostFW RAS Case Engine
   |
Hypervisor Recovery
   |
BMC/Fleet Service
```

Rules:

- closest component contains first
- topology owner diagnoses
- workload owner recovers
- BMC preserves service history

## RVSoC-Sim Integration

Firmware architecture is coupled with RVSoC-Sim.

Simulation models:

- SBE/SCP/OCC agents
- mailbox/ring communication
- Transaction/Completion protocol
- cache contained transition
- sleep/wakeup
- RAS fault injection
- checkpoint/replay

The simulator is the executable architecture specification.
