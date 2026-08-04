# ArchFW Distributed RAS Design v0.1 — Superseded

This document is retained as design history.

The production ownership model changed in v0.2: complete platform root-cause diagnosis belongs to an independent RAS Processor when that domain exists; HostFW validates its action manifest, updates Target state and drives istep reconfiguration. HostFW no longer owns a duplicate full RAS Case Engine.

Use:

- [ArchFW Distributed RAS Design v0.2](ARCHFW_DISTRIBUTED_RAS_DESIGN_V0.2.md)
- [ArchFW Microkernel and Firmware Architecture v0.2](ARCHFW_MICROKERNEL_FIRMWARE_ARCHITECTURE_V0.2.md)
- [ArchFW RISC-V RAS Architecture v0.2](ARCHFW_RISCV_RAS_ARCHITECTURE_V0.2.md)

---

## Historical v0.1 model

The original model proposed:

```text
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

Useful ideas retained in v0.2:

- local detection and smallest-domain containment;
- structured evidence and boot-epoch identity;
- an always-on broker/journal;
- topology-aware diagnosis;
- workload recovery in the hypervisor/OS;
- persistent service action and fleet correlation;
- avoiding SMI-style global stop for contained errors;
- simulator fault injection, delayed completion and stale-event filtering.

The ownership of complete diagnosis is the corrected part: it moved from HostFW to the independent RAS Processor, leaving HostFW as the host-context action executor and boot reconfiguration authority.