# Firmware-Native Hypervisor

**Technical name:** ArchHV

The hypervisor provides the minimum logical-partition runtime: vCPU dispatch, G-stage translation, virtual interrupts, IOMMU/DMA isolation, resource contracts, virtual time, and partition lifecycle.

Code will use namespaces such as:

```cpp
namespace jixia::hypervisor {}
namespace jixia::hypervisor::scheduler {}   // codename Yixing
namespace jixia::hypervisor::contract {}    // codename Shouyue
namespace jixia::hypervisor::isolation {}   // codename Dunshan
namespace jixia::hypervisor::time {}        // codename Sunbin
```

Complex physical device drivers belong in a driver service domain, not in the minimum hypervisor.
