# Guigu / 鬼谷

Guigu is the cross-backend dynamic debug, introspection, fault-injection, checkpoint, and replay framework.

It operates on semantic objects such as harts, vCPUs, LPARs, services, transactions, PlatformGraph nodes, ownership, DMA mappings, interrupts, boot states, and RAS cases.

Guigu frontends and scripts should work across QEMU, Jingjie, RTL/emulation, FPGA, and approved silicon debug backends.

Production access is capability-controlled, measured, and audited. Debug-enabled confidential LPARs must never receive production secrets.
