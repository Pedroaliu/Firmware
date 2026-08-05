# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-05
- **Working mode:** solo development with ChatGPT research/review support
- **Progress branch:** `roadmap/solo-development`
- **Stable integration branch:** `main`
- **Current milestone:** `M00-02 Complete RV64 TrapFrame`
- **Current status:** ACTIVE — design and implementation preparation

## Status legend

| Status | Meaning |
|---|---|
| `DONE` | Definition of Done satisfied and evidence recorded |
| `ACTIVE` | the single current primary milestone |
| `NEXT` | ordered immediately after ACTIVE |
| `PLANNED` | accepted roadmap item, not started |
| `FROZEN` | deliberately blocked by an architectural prerequisite |
| `RESEARCH` | exploratory work without an implementation commitment |

## Milestone ledger

| Milestone | Status | Evidence | Notes |
|---|---|---|---|
| M00-00 Minimal RV64 boot, stack, BSS, UART | DONE | `c30c0405b388a0fba4c528856236ff02267f1a77` | QEMU virt reset entry and initial firmware output |
| M00-01 Minimal fatal M-mode trap | DONE | `ce661a8c1f1798861cab2ef766749cae38bcdc69` | `mtvec`, `mcause`, `mepc`, and `mtval`; non-resumable; revalidated on 2026-08-05 after freestanding C++ compatibility fix |
| Architecture v0.1 | DONE | `1fe8fbcf16a477fc921e7b5aac7be066d13c65b2` | original ArchFW architecture record |
| LPAR/CECSIM co-design direction | DONE | `9f5422939fa5501811135170a1c8633ef18842f3` | long-term firmware-native partition and simulator direction |
| Jixia naming and persistent context | DONE | `6c6769adb8f1aa9c6e1b6f4afb9d3800b5d22433` | Jixia identity and project memory established |
| Semantic paths and `jixia::*` namespaces | DONE | `df8bc2d6bd32d1b13b659e5e33629d24c1488bc2` | C++ namespace boundary, semantic source paths, architecture image |
| Solo development roadmap | DONE | `1d9e1cfb9be782b5e7ad44d21b41600f021fd597` | single-threaded project execution and feature gates |
| Freestanding C++ compatibility fix | DONE | `7d8a66f4dbac12e6196d0fbbf3a28932647bbd0e` | `<stdint.h>`/`uintptr_t` source fix plus CMake compile-option cleanup; build and QEMU validated on the user's workstation |
| M00-02 Complete RV64 TrapFrame | ACTIVE | pending | current unique primary task |
| M00-03 Recoverable trap and `mret` | NEXT | pending | begins only after M00-02 is recorded DONE |
| M00-04 Timer interrupt | NEXT | pending | depends on stable trap entry/exit |
| M00-05 Per-hart state and stacks | NEXT | pending | required before multicore work |
| M00-06 Privilege transition foundation | PLANNED | pending | remains in firmware-first phase |
| M00-07 Early physical allocator | PLANNED | pending | supports later service/memory work |
| M00-08 Structured event and trace ABI | PLANNED | pending | shared later with Jingjie |
| M00-09 Automated QEMU test harness | PLANNED | pending | machine-checkable regression tests |

## NOW: M00-02 Complete RV64 TrapFrame

### Objective

Create a precise, shared RV64 trap-context representation that can later support recoverable exceptions, interrupts, service isolation, context switching, vCPU state, debug, and RAS evidence.

### Work breakdown

```text
[ ] study RISC-V trap-entry architectural state
[ ] define required saved registers and CSRs
[ ] define alignment and stack layout
[ ] define assembly/C++ shared offsets
[ ] implement complete save path
[ ] pass TrapFrame to C++ handler
[ ] implement complete restore path
[ ] create known-register-value test
[ ] record nesting and fatal-path policy
[ ] add machine-checkable QEMU test evidence
[ ] write design and learning notes
```

### Acceptance evidence

The milestone becomes DONE only when the repository contains:

- a documented TrapFrame layout;
- compile-time checks for size/alignment/offset assumptions where possible;
- assembly save and restore paths;
- a C++ handler boundary under `jixia::microkernel::trap`;
- a test that loads known register values, triggers a trap, and verifies preservation;
- failure-path behavior for an unsupported or fatal trap;
- build/test commands and captured expected results;
- an updated entry in this file.

## NEXT queue

1. `M00-03 Recoverable trap and mret`
2. `M00-04 Timer interrupt`
3. `M00-05 Per-hart state and stacks`

Only one becomes ACTIVE at a time.

## Frozen implementation areas

These remain architecture/documentation topics, not current code tasks:

```text
FROZEN  ArchHV and LPAR runtime
FROZEN  HS/VS execution and G-stage translation
FROZEN  virtual interrupt and virtual I/O
FROZEN  Service LPAR
FROZEN  confidential LPAR runtime
FROZEN  migration
FROZEN  simulator-dependent partition hardware experiments
```

They are released only through the gates in `docs/JIXIA_SOLO_ROADMAP.md`.

## Progress update protocol

When a milestone is completed, append a dated record using this template:

```markdown
### YYYY-MM-DD — <milestone> completed

- Status: DONE
- Branch/PR:
- Commit:
- Test command:
- Test result:
- What was learned:
- Design decisions:
- Known limitations:
- Documentation updated:
- Next ACTIVE milestone:
```

Then update:

1. the milestone ledger above;
2. the NOW section;
3. `PROJECT_CONTEXT.md` if the active milestone or architecture direction changed;
4. relevant design and learning notes.

## Progress history

### 2026-08-05 — freestanding C++ compatibility and QEMU revalidation

- Status: DONE
- Source fix: replace `<cstdint>`/`std::uintptr_t` with `<stdint.h>`/`uintptr_t` in `firmware_main.cpp` and `trap.cpp`.
- Build result: passed on the user's Ubuntu workstation with `riscv64-unknown-elf-gcc/g++`.
- Run command: `./scripts/run-qemu.sh`.
- QEMU result: Jixia entered the microkernel and intentionally executed a 32-bit `EBREAK`.
- Observed state: `mcause=3`, `mepc=0x00000000800000ee`, `mtval=0`, confirming the expected breakpoint exception path.
- Decision: the two-source-file change is the necessary and sufficient root-cause fix for the missing `<cstdint>` header. The CMake change in the merged build fix is retained as an independent compile-option cleanup, not as a prerequisite for solving the header error.
- Limitation: the trap path remains fatal and non-resumable; M00-02 remains ACTIVE.

### 2026-08-05 — solo development process established

- Status: DONE
- Branch: `roadmap/solo-development`
- Roadmap commit: `1d9e1cfb9be782b5e7ad44d21b41600f021fd597`
- Decision: one active milestone at a time; no parallel major subsystem development.
- Current ACTIVE milestone: `M00-02 Complete RV64 TrapFrame`.
- LPAR implementation moved behind the Jingjie simulator prerequisite gate.
- Persistent records: `PROJECT_CONTEXT.md`, `docs/JIXIA_SOLO_ROADMAP.md`, and this progress ledger.
