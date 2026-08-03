# ArchFW Distributed RAS Design v0.1

## Philosophy

Future cloud servers are heterogeneous:

- CPU
- GPU
- NPU
- DPU
- CXL memory
- PCIe devices
- management processors

RAS cannot depend on one firmware domain.

## RAS Layers

```
Hardware autonomous RAS
        |
Local Agent
        |
RAS Broker
        |
HostFW RAS Case Engine
        |
Hypervisor recovery
        |
BMC/Fleet service
```

## Responsibilities

### Local Agent

Examples:

- SBE
- SCP
- GPU firmware
- DPU firmware
- CXL controller firmware

Responsibilities:

- detect local failure
- contain quickly
- collect evidence
- report event

### RAS Broker

Always-on domain:

- event routing
- journal
- boot epoch tracking
- host-dead evidence capture
- escalation

### HostFW RAS Engine

Responsibilities:

- topology diagnosis
- root cause analysis
- deconfiguration
- recovery decision

Inspired by Hostboot PRDF/HWAS.

### Hypervisor

Responsible for:

- VM impact
- tenant isolation
- workload recovery

### BMC

Responsible for:

- persistent storage
- service action
- fleet correlation

## RAS Event

Events contain:

- source agent
- boot epoch
- target ID
- severity
- evidence handle
- containment state
- recovery actions
- workload context

## Avoid Global Stop

Do not use SMI-style global stop for every error.

Containment domain should be minimal:

thread -> core -> device -> socket -> system

Only global synchronization for true system failures.

## RVSoC-Sim Integration

Simulation should support:

- fault injection
- agent reset
- delayed completion
- stale event filtering
- evidence collection
- recovery replay
