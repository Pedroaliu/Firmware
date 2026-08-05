# Full-System Simulator Interface

**Implementation codename:** Jingjie / 镜界

This directory records the firmware-facing contracts for full-system simulation and co-simulation. The simulator itself may live in a separate repository.

Shared contracts will use semantic names and namespaces such as `jixia::simulator`, covering:

- PlatformGraph schema;
- ArchSimCall-style test events;
- trace and fault schemas;
- LPID and BootEpoch correlation;
- target tests and invariants;
- checkpoint/replay metadata;
- functional, timing, cycle, and RTL backend capabilities.

Confirm the active simulator repository before making cross-repository changes.
