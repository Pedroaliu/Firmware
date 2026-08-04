# ArchFW Agent Protocol v0.2

This protocol connects isolated ArchFW services with remote firmware processors and simulator Agents.

Canonical architecture: [ArchFW Microkernel and Firmware Architecture v0.2](ARCHFW_MICROKERNEL_FIRMWARE_ARCHITECTURE_V0.2.md).

## 1. Goals

The protocol must survive:

- different CPU architectures and compilers;
- mailbox, shared-memory and management-bus transports;
- Agent reset and host reset;
- delayed and duplicated messages;
- service restart;
- target deconfiguration and topology changes;
- partial failure of a power, clock or coherence domain.

It is a semantic protocol. Transport bindings may differ, but transaction identity, ownership and completion rules do not.

## 2. Participants

Typical Agents include:

- SBE/core/cache Agent;
- SCP/system-control Agent;
- memory-controller or DDR Agent;
- PCIe/CXL Agent;
- independent RAS Processor;
- BMC/management endpoint;
- QEMU or ArchLab simulator adapter.

HostFW itself is not one global Agent. Each isolated host service owns its own Endpoint and Agent client state.

## 3. Wire rules

1. Do not share compiler-layout-dependent C/C++ structures.
2. All messages use fixed-width integer fields and an explicit byte order.
3. The envelope is versioned independently from service payload schemas.
4. Unknown optional fields are skipped by length; unknown mandatory fields fail negotiation.
5. Mutating operations must be idempotent or explicitly marked non-replayable.
6. A response always echoes the complete transaction identity.
7. Old boot epochs, service generations and target generations are rejected.
8. Bulk FFDC is immutable after publication and referenced by a descriptor.

## 4. Envelope

```cpp
struct AgentEnvelope {
    uint16_t protocol_version;
    uint16_t header_length;
    uint16_t service_id;
    uint16_t opcode;
    uint32_t flags;

    uint64_t transaction_id;
    uint64_t boot_epoch;
    uint64_t topology_generation;
    uint64_t target_id;

    uint32_t target_generation;
    uint32_t service_generation;
    uint32_t agent_generation;
    uint32_t payload_length;

    uint32_t status;
    uint32_t payload_crc32c;
};
```

Recommended flags:

```text
REQUEST
RESPONSE
EVENT
ACK_REQUIRED
IDEMPOTENT
NON_REPLAYABLE
HAS_BULK_DESCRIPTOR
MORE_FRAGMENTS
PRIVILEGED_OPERATION
```

The transport may add its own framing and integrity fields. Those do not replace the semantic identity fields above.

## 5. Transaction identity

A transaction is uniquely scoped by:

```text
BootEpoch
TopologyGeneration
ServiceGeneration
AgentGeneration
TransactionId
TargetId
TargetGeneration
```

Meaning:

- `BootEpoch`: changes on host boot/recovery epoch.
- `TopologyGeneration`: changes when committed topology or ownership changes.
- `ServiceGeneration`: changes when a host service restarts.
- `AgentGeneration`: changes when a remote Agent resets or reinitializes.
- `TransactionId`: monotonic or random unique ID within the above scope.
- `TargetGeneration`: changes when a target is removed, recreated or materially reconfigured.

An operation may complete only if all relevant generations still match.

## 6. Transaction state machine

```text
CREATED
  -> REQUESTED
  -> ACCEPTED
  -> IN_PROGRESS
  -> COMPLETED

Any active state may transition to:
  RETRY_LATER
  CANCEL_PENDING
  CANCELLED
  TIMED_OUT
  FAILED
  STALE
```

`ACCEPTED` means the Agent owns responsibility for the operation, not that hardware effects are complete.

`COMPLETED` must include the final observed state and, for mutating operations, the resulting target generation or configuration version.

## 7. Status model

Minimum status values:

```text
OK
RETRY_LATER
BAD_VERSION
BAD_LENGTH
BAD_PAYLOAD
UNSUPPORTED_SERVICE
UNSUPPORTED_OPCODE
NOT_OWNER
ACCESS_DENIED
NO_POWER
RESET_ASSERTED
DEPENDENCY_NOT_READY
BUSY
TIMEOUT
CANCELLED
STALE_BOOT_EPOCH
STALE_TOPOLOGY
STALE_SERVICE_GENERATION
STALE_AGENT_GENERATION
STALE_TARGET_GENERATION
EVIDENCE_UNAVAILABLE
PARTIAL_COMPLETION
FAILED
```

A transport failure is not automatically a hardware-operation failure. The client reconciles by `QueryTransaction` or `QueryTargetState` before retrying a possibly mutating operation.

## 8. Ownership and authorization

For destructive operations, an Agent validates:

- caller identity;
- service and opcode permission;
- lifecycle/configuration ownership lease;
- target generation;
- power/reset preconditions;
- dependency state;
- optional security token or signed manifest.

Kernel capability checks protect the host-side Endpoint, MMIO and shared buffers. Agent-side ownership checks protect remote hardware control.

## 9. Service classes

Reserved service classes:

```text
DISCOVERY
LIFECYCLE
POWER_CLOCK_RESET
CORE_CACHE
MEMORY
PCIE_CXL
INTERRUPT_ROUTING
SECURITY
RAS
TRACE_FFDC
IMAGE_UPDATE
WATCHDOG
```

Each service defines:

- opcodes;
- request/response schemas;
- required ownership role;
- replay behavior;
- timeout class;
- completion semantics;
- FFDC schema;
- reconciliation query.

## 10. Discovery and negotiation

Connection establishment:

```text
transport ready
  -> Hello(protocol ranges, AgentId, AgentGeneration)
  -> Capabilities(service versions, limits, target domains)
  -> Time/epoch synchronization
  -> ownership reconciliation
  -> READY
```

Capabilities report:

- supported service and schema versions;
- maximum inline payload and bulk descriptors;
- queue depths;
- cancellation support;
- target/resource domains controlled;
- evidence retention limits;
- reset and watchdog behavior.

## 11. Transport bindings

### Mailbox / Message Unit

Suitable for small control messages. A shared payload area may hold larger data. Interrupt or doorbell delivery is bound to a Notification on the host.

### Shared-memory queues

Use separate request, completion and event queues. Queue ownership follows producer/consumer indices. The descriptor ring is transport state; the transaction state remains in the semantic envelope.

### MCTP/PLDM or management bus

Use when the Agent resides behind a management controller. Fragmentation, retries and authentication are transport services; ArchFW transaction generations remain mandatory.

### Simulator

The simulator binding may schedule messages as timed events, inject delay/loss/reset and expose target state. It must not bypass ownership and generation validation.

## 12. Bulk data and FFDC

```cpp
struct BulkDescriptor {
    uint64_t handle;
    uint64_t length;
    uint32_t schema_id;
    uint32_t flags;
    uint8_t  digest[32];
};
```

Bulk data rules:

- immutable after publication;
- bounded lifetime and explicit release;
- digest verified before consumption;
- ownership transfer or read-only sharing is explicit;
- sensitive evidence is access-controlled and may be encrypted by the transport/security service.

## 13. Events

Unsolicited Agent events use a separate event sequence number and contain:

```text
AgentId
AgentGeneration
BootEpoch
EventSequence
TargetId
TargetGeneration
EventClass
Severity
EvidenceDescriptor
ContainmentState
```

Events are acknowledged only after the receiver has durably captured the required evidence or transferred ownership to the RAS domain.

## 14. Reset and reconciliation

On Agent reset:

1. increment `AgentGeneration`;
2. stop accepting old-generation completions;
3. rediscover service capabilities;
4. query actual target state;
5. compare actual state with Desired State and committed istep journal;
6. reissue only idempotent or proven-incomplete operations;
7. escalate ambiguous non-replayable operations to the Step Engine.

On HostFW restart, the Agent must not assume that old ownership leases remain valid.

## 15. Watchdogs

The physical watchdog is owned by the SCP/RAS/management domain where possible. Host services receive virtual watchdog contracts:

- boot-progress watchdog for the IStep engine;
- service watchdog for the supervisor;
- Agent transaction watchdog;
- OS watchdog after handoff.

A watchdog timeout produces an event with the owning transaction and target context rather than only a global reset reason.

## 16. Deferred work

Not required in M02:

- cryptographic session establishment on every transport;
- hot protocol upgrade during a transaction;
- cross-machine federation;
- exactly-once delivery without reconciliation;
- generic distributed consensus.

M02 requires version negotiation, generations, idempotency classification, query/reconcile and deterministic failure injection.