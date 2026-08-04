# ArchFW Firmware Architecture

This branch develops ArchFW as a capability-protected firmware OS for heterogeneous cloud-server platforms.

## Canonical architecture

Start here:

- [ArchFW Microkernel and Firmware Architecture v0.2](docs/ARCHFW_MICROKERNEL_FIRMWARE_ARCHITECTURE_V0.2.md)

That document freezes the current integration of:

- a seL4-inspired minimal object-capability kernel;
- a Hostboot-inspired istep, Targeting, HWP and reconfiguration model;
- NXP-inspired resource domains and power/clock/reset ownership;
- isolated hardware services and remote Agents;
- independent RAS diagnosis;
- EDK II, EFI memory map and ACPI handoff;
- a QEMU implementation plan through M00-M04.

## Supporting design documents

### Platform and configuration

- [Platform Unification Principle](docs/ARCHFW_PLATFORM_UNIFICATION_PRINCIPLE.md)
- [Configuration Model v0.1](docs/ARCHFW_CONFIG_MODEL_V0.1.md)
- [Desired State and Reconciliation Model v0.1](docs/ARCHFW_DESIRED_STATE_AND_RECONCILIATION_MODEL_V0.1.md)
- [Firmware State Store and Migration v0.1](docs/ARCHFW_FIRMWARE_STATE_STORE_AND_MIGRATION_V0.1.md)

### Agents and ownership

- [Agent Protocol v0.1](docs/ARCHFW_AGENT_PROTOCOL_V0.1.md)

### RAS

- [Distributed RAS Design v0.1](docs/ARCHFW_RAS_DESIGN_V0.1.md)
- [Hybrid RAS Architecture v0.1](docs/ARCHFW_HYBRID_RAS_ARCHITECTURE_V0.1.md)
- [RISC-V RAS Architecture v0.2](docs/ARCHFW_RISCV_RAS_ARCHITECTURE_V0.2.md)
- [POWER PRDF/FIR/Attention Routing v0.1](docs/ARCHFW_POWER_PRDF_FIR_ATTENTION_ROUTING_V0.1.md)

## Implementation order

```text
M00  microkernel bootstrap and isolated service restart
M01  PlatformGraph, Targeting and transactional istep engine
M02  hardware services, Agent protocol, ownership and reconfiguration
M03  EDK II, EFI memory map, ACPI and Linux boot
M04  independent RAS processor, checkstop and next-boot recovery
```

The v0.2 canonical document controls when older v0.1 notes disagree with it. Older notes remain as focused design studies and will be folded forward as implementation begins.
