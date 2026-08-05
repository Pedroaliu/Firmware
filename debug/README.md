# Dynamic Debug and Introspection

**Implementation codename:** Guigu / 鬼谷

This cross-backend engineering control plane provides semantic event breakpoints, selective pause, audited display/alter, trace filtering, fault injection, checkpoint/replay, state diff, and invariant checking.

Code and protocols use namespaces such as `jixia::debug`, `jixia::debug::event`, and `jixia::debug::replay`.

Frontends should work across QEMU, the full-system simulator, RTL/emulation, FPGA, and approved silicon backends. Production access is measured, capability-controlled, and audited.
