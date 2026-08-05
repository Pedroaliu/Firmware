# Host Firmware Microkernel

**Implementation codename:** Mozi / 墨子

This directory contains the executable Jixia host firmware microkernel.

C++ implementation code uses semantic namespaces such as:

```cpp
namespace jixia::microkernel {}
namespace jixia::microkernel::trap {}
namespace jixia::microkernel::ipc {}
```

Assembly and external firmware boundaries use stable C ABI symbols prefixed with `jixia_`.

The microkernel owns minimum trusted mechanisms: trap/interrupt entry, hart lifecycle, memory-domain primitives, capabilities, typed IPC, service lifecycle, ownership enforcement, secure launch hooks, and root RAS routing. It does not own full device drivers, filesystems, compatibility personalities, or large management policy.
