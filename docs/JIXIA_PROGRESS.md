# Jixia Development Progress

## Current snapshot

- **Last updated:** 2026-08-07
- **Working mode:** solo development with ChatGPT research/review support
- **Progress branch:** `milestone/m00-03-recoverable-trap`
- **Stable integration branch:** `main`
- **Current milestone:** `M00-03 Recoverable trap and mret`
- **Current status:** ACTIVE — implementation is working; dedicated machine-checkable acceptance run is the remaining close-out item

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
| M00-02 Complete RV64 TrapFrame | DONE | branch `milestone/m00-02-trap-frame`; `bash scripts/test-trap-frame.sh` on 2026-08-07 | complete x0-x31 + CSR frame, shared assembly/C++ ABI, save/restore path, known-register test, `TRAP_FRAME_TEST: PASS` |
| M00-03 Recoverable trap and `mret` | ACTIVE | branch `milestone/m00-03-recoverable-trap`; functional QEMU output observed on 2026-08-06/07 | 32-bit `EBREAK` and 16-bit `C.EBREAK` both resume correctly; dedicated `test-recoverable-trap.sh` final PASS remains the close-out gate |
| M00-04 Timer interrupt | NEXT | pending | depends on M00-03 close-out |
| M00-05 Per-hart state and stacks | NEXT | pending | required before multicore work |
| M00-06 Privilege transition foundation | PLANNED | pending | remains in firmware-first phase |
| M00-07 Early physical allocator | PLANNED | pending | supports later service/memory work |
| M00-08 Structured event and trace ABI | PLANNED | pending | shared later with Jingjie |
| M00-09 Automated QEMU test harness | PLANNED | pending | machine-checkable regression tests |

## DONE: M00-02 Complete RV64 TrapFrame

### Objective

Create a precise, shared RV64 trap-context representation that can later support recoverable exceptions, interrupts, service isolation, context switching, vCPU state, debug, and RAS evidence.

### Completed work

```text
[x] study RISC-V trap-entry architectural state
[x] define required saved registers and CSRs
[x] define alignment and stack layout
[x] define assembly/C++ shared offsets
[x] implement complete save path
[x] pass TrapFrame to C++ handler
[x] implement complete restore path
[x] create known-register-value test
[x] record nesting and fatal-path policy
[x] add machine-checkable QEMU test evidence
[x] write design and learning notes
```

### Acceptance evidence

- `TrapFrame` contains x0-x31, `mstatus`, `mepc`, `mcause`, and `mtval`.
- RV64 frame size, alignment, and assembly/C++ offsets are compile-time checked.
- Trap entry saves the complete integer context and trap restore reconstructs it before `mret`.
- The known-register test validates fixed GPR patterns plus live `sp`, `gp`, and `tp` snapshots.
- `bash scripts/test-trap-frame.sh` produced `TRAP_FRAME_TEST: PASS` on 2026-08-07.
- Design record: `docs/JIXIA_M00_02_TRAP_FRAME.md`.

## NOW: M00-03 Recoverable trap and `mret`

### Objective

Turn the previously fatal trap path into a deliberately recoverable path for explicitly recognized software breakpoint instructions while preserving fail-closed behavior for unsupported trap causes.

### Implemented so far

```text
[x] centralize XLEN and mcause masks
[x] type exception and interrupt causes
[x] distinguish interrupt-vs-exception before interpreting cause code
[x] decode 16-bit versus 32-bit instruction length
[x] recognize `C.EBREAK` and `EBREAK` encodings
[x] recover only explicitly recognized breakpoint instructions
[x] advance saved `mepc` by 2 or 4 as appropriate
[x] return through trap restore and `mret`
[x] observe successful 32-bit EBREAK resume in QEMU
[x] observe successful 16-bit C.EBREAK resume in QEMU
[x] retain M00-02 TrapFrame regression test
[ ] record dedicated `bash scripts/test-recoverable-trap.sh` final PASS
[ ] close milestone and activate M00-04
```

### Design decisions

- Trap recovery is whitelist-based: unsupported interrupts/exceptions remain fatal.
- `TrapFrame` is the authoritative software recovery state; C++ modifies `frame.mepc`, and assembly writes it back to the `mepc` CSR.
- `mcause` code alone is insufficient; the interrupt bit and code are interpreted together.
- Cause code 3 is not assumed to mean an executable EBREAK instruction; the handler verifies the instruction at `mepc`.
- Instruction length is decoded from the instruction stream; standard EBREAK advances by 4 and C.EBREAK by 2.

## NEXT queue

1. `M00-04 Timer interrupt`
2. `M00-05 Per-hart state and stacks`
3. `M00-06 Privilege transition foundation`

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

### 2026-08-07 — M00-02 Complete RV64 TrapFrame completed

- Status: DONE
- Branch: `milestone/m00-02-trap-frame`
- Test command: `bash scripts/test-trap-frame.sh`
- Test result: `TRAP_FRAME_TEST: PASS`
- Regression observation: the same firmware run also showed successful standard EBREAK and C.EBREAK resume before the TrapFrame test.
- What was learned: the trap frame is a software ABI between trap assembly and higher-level policy; hardware knows only architectural registers/CSRs, not the C++ `TrapFrame` type.
- Design decisions: save the full integer context, keep `sp` restoration last, make the saved frame the authoritative recovery state, and keep unsupported/nested behavior fail-closed for now.
- Known limitations: no dedicated per-hart trap stack or nested-trap policy yet.
- Documentation updated: `docs/JIXIA_M00_02_TRAP_FRAME.md`, this ledger.
- Next ACTIVE milestone: `M00-03 Recoverable trap and mret`.

### 2026-08-05 — freestanding C++ compatibility and QEMU revalidation

- Status: DONE
- Source fix: replace `<cstdint>`/`std::uintptr_t` with `<stdint.h>`/`uintptr_t` in `firmware_main.cpp` and `trap.cpp`.
- Build result: passed on the user's Ubuntu workstation with `riscv64-unknown-elf-gcc/g++`.
- Run command: `./scripts/run-qemu.sh`.
- QEMU result: Jixia entered the microkernel and intentionally executed a 32-bit `EBREAK`.
- Observed state: `mcause=3`, `mepc=0x00000000800000ee`, `mtval=0`, confirming the expected breakpoint exception path.
- Decision: the two-source-file change is the necessary and sufficient root-cause fix for the missing `<cstdint>` header. The CMake change in the merged build fix is retained as an independent compile-option cleanup, not as a prerequisite for solving the header error.
- Limitation: the trap path remained fatal and non-resumable at that point.

### 2026-08-05 — solo development process established

- Status: DONE
- Branch: `roadmap/solo-development`
- Roadmap commit: `1d9e1cfb9be782b5e7ad44d21b41600f021fd597`
- Decision: one active milestone at a time; no parallel major subsystem development.
- LPAR implementation moved behind the Jingjie simulator prerequisite gate.
- Persistent records: `PROJECT_CONTEXT.md`, `docs/JIXIA_SOLO_ROADMAP.md`, and this progress ledger.
