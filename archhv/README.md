# ArchHV

ArchHV is the Jixia firmware-native type-1 hypervisor.

Planned internal subsystems:

- **Yixing / 弈星** — vCPU dispatch, entitlement-aware scheduling, NUMA placement, and affinity.
- **Shouyue / 守约** — resource contracts, accounting, caps, weights, and policy evidence.
- **Dunshan / 盾山** — G-stage isolation, IOMMU, DMA windows, MMIO permissions, and ownership transfer.
- **Sunbin / 孙膑** — virtual time and migration continuity.

ArchHV provides minimum partition mechanisms and stable virtual primitives. Complex physical device drivers belong in Luban or another service LPAR.
