# Driver and Boot Service Domain

**Implementation codename:** Luban / 鲁班

This Linux-based service domain reuses PCI endpoint, NVMe, SCSI, RAID, HBA, NIC, filesystem, network, boot-discovery, and administration support.

Code and interfaces use semantic names such as `jixia::services::driver_domain` and `jixia::services::boot`.

The host firmware owns platform control and resource ownership. The driver domain owns only explicitly assigned device functions and is not automatically part of the minimum trusted computing base.
