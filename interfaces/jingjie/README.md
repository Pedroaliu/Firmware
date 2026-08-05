# Jingjie / 镜界 Interface

Jingjie is the full-system simulation and co-simulation world related to Jixia.

This directory records the firmware-facing boundary rather than implementing the simulator inside the Firmware repository.

Shared contracts will include:

- Nuwa platform schema;
- ArchSimCall-style test events;
- trace and fault schemas;
- LPID and BootEpoch correlation;
- target tests and architecture invariants;
- checkpoint/replay metadata;
- functional, timing, cycle, and RTL backend capabilities.

Before modifying a simulator repository, confirm which related user repository is currently the active Jingjie implementation.
