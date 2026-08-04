# ArchFW PNOR and BootFS direction v0.1

## Decision

ArchFW needs PNOR/flash storage and a firmware file namespace, but a
POSIX-like general VFS is not part of the microkernel and is not required
for the first UART bootstrap.

The first useful abstraction is a small, read-only **BootFS service** over
flash regions. It loads verified firmware objects by stable object name.
Writable state is handled by separate journal/NVRAM services so that boot
images and mutable state do not share failure semantics.

## Why it is outside the kernel

The microkernel needs only memory, protection, scheduling, IPC and IRQ
mechanisms. Flash discovery, partition tables, compression, signature
verification, rollback and module naming are policies implemented by
Boot0, Image Service and BootFS Service.

## Initial image model

```text
PNOR / QEMU pflash
+-- immutable boot manifest
+-- Boot0
+-- ArchFW kernel
+-- Root Orchestrator
+-- Platform IR
+-- service images
+-- EDK II payload
+-- recovery image
+-- mutable state area
    +-- UEFI variables
    +-- boot/reconfiguration journal
    +-- RAS manifest cache
```

Each immutable object will eventually carry:

```text
ObjectId
ObjectType
Version
Offset
StoredSize
ExpandedSize
Hash
Signature/verification policy
LoadAddress policy
Dependencies
```

## QEMU staging

M00 links one flat kernel image and uses `-bios` so the CPU-entry path is
small and observable.

M01 introduces a generated flash image and read-only manifest parser.
M02 moves module loading into an isolated Image/BootFS service.
M03 adds a second pflash device for UEFI variables and persistent journals.

## Explicit non-goals for the first implementation

- no POSIX path lookup;
- no directory mutation;
- no runtime package installation;
- no filesystem cache inside the kernel;
- no writable boot partition;
- no driver dynamically loaded before the trust chain and ownership model
  are established.
