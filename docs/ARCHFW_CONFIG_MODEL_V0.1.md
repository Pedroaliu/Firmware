# ArchFW Configuration Model v0.1

## Goal

One source of truth for silicon, board, project and product configuration.

The human maintained format is CUE.

The firmware never parses CUE at runtime.

Flow:

```
CUE
 |
fwcfg compiler
 |
Platform IR
 |
Targeting Service / generated interfaces
```

## Configuration Layers

```
Silicon
  capabilities and immutable hardware facts

Board
  wiring, slots, power, clocks, retimers

Project
  SKU customization and resource allocation

Policy
  security, RAS, cloud behavior

Runtime Override
  authorized user/BMC changes
```

## Important Rule

User settings override defaults, not hardware facts.

Priority:

```
Hardware constraints
 > Fleet policy
 > Debug override
 > Persistent user setting
 > Project default
 > Silicon default
```

## Settings Model

Persistent settings are stable key/value records.

Do not use C structs as persistent ABI.

Each setting has:

- stable ID
- type
- default
- ownership
- permission
- apply time
- migration rule

## Generated Outputs

- C++ typed APIs
- Target schema
- PlatformGraph
- ACPI
- DeviceTree
- BMC registry
- Agent manifests
- Simulator topology

## Platform Topology

Complex SoC topology is represented as graph:

- nodes
- relations
- capabilities
- ownership

Examples:

- PCIe lanes
- CXL ports
- GPU/NPU/DPU links
- power domains
- clock domains
- reset domains
