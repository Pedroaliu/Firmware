# ArchFW Agent Protocol v0.1

## Purpose

Different firmware processors need a common semantic protocol.

Agents:

- HostFW
- SBE
- SCP
- OCC
- GPU firmware
- NPU firmware
- DPU firmware
- BMC

## Do not share C structs

Communication ABI must not depend on compiler layout.

Use versioned messages.

## Message Model

```
Request
  |
Accepted
  |
Progress
  |
Completion
```

Support:

- RetryLater
- Event
- Cancel
- QueryState

## Identity

Transactions use:

```
AgentId
BootEpoch
TransactionId
```

This prevents stale completion after agent reset.

## Transport

The semantic protocol can run over:

- mailbox
- shared memory ring
- doorbell interrupt
- MCTP/PLDM
- simulator transaction model

## Relation to Virtio

Borrow concepts:

- descriptor ownership
- ring buffer
- completion queue
- notification

But extend for firmware:

- ownership transfer
- reset reconciliation
- RAS evidence
- capability discovery

## Interface Definition

Long term direction:

ArchIDL defines interfaces.

Generated bindings:

- C++ native API
- IPC stubs
- remote agent protocol
- simulator adapters
