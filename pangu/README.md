# Pangu / 盘古

Pangu is the future immutable Boot0 and first-instruction root.

It will remain deliberately small:

- establish minimum reset execution state;
- authenticate and measure Mozi;
- choose normal or recovery path;
- pass a minimal handoff structure;
- avoid product policy and complex device logic.

The current `mozi/arch/riscv/start.S` is an M00 bootstrap, not yet a separated Pangu implementation.
