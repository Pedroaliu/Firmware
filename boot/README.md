# Boot0

**Implementation codename:** Pangu / 盘古

Boot0 is the future immutable first-instruction root. It establishes the minimum reset state, authenticates and measures the host firmware microkernel, chooses normal or recovery flow, and transfers control through a minimal handoff structure.

Code will use namespaces such as `jixia::boot` and `jixia::boot::verified_launch`.

The current reset entry under `microkernel/arch/riscv/` is an M00 bootstrap and is not yet a separated Boot0 implementation.
