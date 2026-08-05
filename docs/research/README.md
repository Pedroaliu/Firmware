# ArchFW Research Index

This directory stores durable research conclusions, architecture mappings, decision snapshots, and unresolved questions. Raw PDFs are intentionally not committed.

## Current state

- [`ARCHFW_RESEARCH_STATE_2026-08-05.md`](./ARCHFW_RESEARCH_STATE_2026-08-05.md)
  - authoritative snapshot of current decisions;
  - boot/runtime lifecycle;
  - privilege split;
  - kernel/service boundaries;
  - istep and PlatformGraph direction;
  - QEMU M00–M04 implementation path;
  - open questions and immediate next work.

## Microkernel and system structure

- [`sel4/COMP9242_ARCHFW_MAPPING_V0.1.md`](./sel4/COMP9242_ARCHFW_MAPPING_V0.1.md)
  - maps COMP9242, L4/seL4, Microkit, LionsOS, execution-model, SMP, security, and performance lessons into ArchFW design rules;
  - defines the intended role of Endpoint, Reply, Notification, SPSC queues, capabilities, fault endpoints, initial task, scheduling, and future multikernel scaling.

## RAS

- [`ras/ARCHFW_COMMON_RAS_DEPLOYMENT_MODEL_V0.1.md`](./ras/ARCHFW_COMMON_RAS_DEPLOYMENT_MODEL_V0.1.md)
  - defines one Common RAS Engine with RSP-primary, host-primary, external-service-processor, boot-fallback, and offline profiles;
  - records diagnosis ownership, evidence apertures, HBRT-like runtime interfaces, boot recovery, and the first QEMU proof case.

## Existing architecture specifications

The research notes complement the main specifications in `docs/`, including:

- `ARCHFW_ARCHITECTURE_V0.1.md`
- `ARCHFW_CONFIG_MODEL_V0.1.md`
- `ARCHFW_PLATFORM_UNIFICATION_PRINCIPLE.md`
- `ARCHFW_POWER_PRDF_FIR_ATTENTION_ROUTING_V0.1.md`
- `ARCHFW_RISCV_RAS_ARCHITECTURE_V0.2.md`

## Source handling policy

- Do not commit restricted course or paper PDFs to the public repository.
- Record source title, author, local filename, and the design decisions derived from it.
- Distinguish clearly between source-supported facts and ArchFW engineering decisions.
- When a decision changes, update the dated research state and the corresponding detailed specification rather than relying on chat history.

## Update discipline

After a substantial research session:

1. update or add the topic note;
2. update the dated research-state snapshot when a project-wide decision changes;
3. record rejected alternatives and the reason;
4. list unresolved questions explicitly;
5. include the intended implementation milestone affected by the decision.

This directory is the continuity layer for the project. Chat history is useful working context, but it is not the system of record.
