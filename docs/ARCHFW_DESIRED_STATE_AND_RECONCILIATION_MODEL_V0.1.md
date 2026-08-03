# ArchFW Desired State and Reconciliation Model v0.1

## Background

Traditional BIOS preserve configuration relies on exporting NV variables before upgrade and restoring them after flashing a new BIOS image. This approach solves compatibility problems but keeps configuration tied to firmware layout and requires BMC to understand BIOS state.

ArchFW separates firmware image lifecycle from persistent platform state.

## Core Principle

Firmware upgrade should migrate semantic user intent, not binary NV layout.

## Desired State Model

Persistent storage keeps desired configuration:

- object identity
- schema version
- value
- owner/source
- timestamp
- checksum

Example:

```
object: pcie.port0.policy
value:
  speed: Gen5
  width: x16
source: user
version: 2
```

The storage does not preserve C struct offsets.

## State Layers

```
CUE Default Configuration
        |
        +
User Desired Override
        |
        v
Effective Desired State
        |
Reconciliation Engine
        |
PlatformGraph Current State
        |
Hardware
```

## Firmware Upgrade Flow

1. Snapshot persistent desired state
2. Upgrade firmware image
3. Load new schema
4. Run migration rules
5. Merge new defaults with existing user intent
6. Reconcile hardware state
7. Verify final platform state

## Reconciliation

Firmware should continuously compare:

Desired State:

```
User/platform expectation
```

Current State:

```
Actual hardware condition
```

When they differ, controllers execute workflows to converge.

Examples:

- PCIe desired Gen6 x16, current Gen5 x8 -> run PCIe recovery workflow
- DIMM desired enabled, current disabled -> evaluate memory recovery workflow
- Firmware desired version differs from installed version -> execute update workflow

## Design Inspiration

- Kubernetes desired state and reconciliation loop
- Database schema migration and transactions
- Filesystem journal/copy-on-write ideas
- Hostboot targeting and hardware state model

## ArchFW Components

```
Config Service
State Store
Schema Migration Engine
Reconciliation Engine
PlatformGraph
Agent Controllers
```

## Key Decision

ArchFW does not restore old NV structures. It restores and migrates platform intent.