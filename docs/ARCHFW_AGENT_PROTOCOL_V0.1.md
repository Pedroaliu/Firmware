# ArchFW Agent Protocol v0.1 — Superseded

This document is retained as design history.

Use [ArchFW Agent Protocol v0.2](ARCHFW_AGENT_PROTOCOL_V0.2.md) for the current wire envelope, generation model, ownership checks, reset reconciliation, FFDC descriptors and transport bindings.

The v0.1 ideas retained in v0.2 are:

- one semantic protocol across SBE, SCP, memory, PCIe/CXL, RAS, BMC and simulator Agents;
- no compiler-layout-dependent shared C structures;
- versioned messages;
- request, acceptance, progress and completion states;
- `AgentId`, `BootEpoch` and `TransactionId` identity;
- transport independence across mailbox, shared-memory ring, doorbell, MCTP/PLDM and simulation;
- explicit descriptor ownership and completion queues;
- future ArchIDL-generated native, IPC, remote and simulator bindings.

v0.2 adds topology, service, Agent and target generations; idempotency/replay classification; ownership validation; bulk evidence descriptors; and deterministic reset reconciliation.
