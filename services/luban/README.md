# Luban / 鲁班

Luban is the Linux driver and boot service domain.

It reuses Linux support for PCI endpoints, NVMe, SCSI, RAID, HBA, NIC, filesystems, networking, boot discovery, and administration tools.

Mozi owns platform control and resource ownership. Luban owns only explicitly assigned device functions and must use controlled MMIO, DMA, and interrupt contracts.

Luban is powerful but is not automatically inside the minimum trusted computing base.
