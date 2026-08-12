# Jixia M00-06 Privilege Transition Foundation

**Status:** ACTIVE  
**Stable baseline:** `main`  
**Umbrella branch:** `milestone/m00-06-privilege-transition`  
**Accepted through:** `M00-06.03 S->M ECALL round trip`

## 1. Objective

M00-06 establishes the first controlled lower-privilege execution path in Mozi without mixing in paging, scheduling, allocation, or service isolation.

The first complete state machine is:

```text
M-mode Mozi
    -> prepare trusted per-hart trap state
    -> configure mstatus.MPP = S
    -> configure mepc = S entry
    -> keep satp = 0
    -> mret
    -> S-mode payload
    -> ecall / controlled trap
    -> M-mode trusted trap entry
    -> prove previous privilege and saved context
    -> controlled return to S
```

The architectural boundary is not merely the `mret` instruction. The critical security invariant is that M-mode must not trust an interrupted stack merely because the hart has already entered M privilege.

## 2. Submilestone gates

M00-06 is split into four acceptance gates. Only one submilestone is ACTIVE at a time.

### M00-06.01 — Trusted M-mode trap entry — DONE

Accepted baseline: `b6b7e9c` (`milestone: establish Jixia baseline through M00-06.01`).

Delivered mechanisms:

```text
[x] explicit early mscratch = 0 state before HartLocal exists
[x] HartLocal assembly ABI for trap-entry scratch state
[x] privilege-aware trap entry using mstatus.MPP
[x] M-origin traps preserved through the existing TrapFrame ABI
[x] lower-origin traps do not dereference the interrupted sp before switching trust domains
[x] lower-origin x2/sp value is preserved in the TrapFrame
[x] lower-origin gp is preserved and kernel gp is restored before C++ dispatch
[x] TrapFrame remains the common C++ dispatcher contract
[x] existing M-mode TrapFrame/recoverable/timer/SMP regressions remain valid
[x] clean CI baseline builds and passes RV64 QEMU regression
```

M00-06.01 used the normal per-hart M stack for M-origin traps and the per-hart stack top as temporary trusted storage for lower-origin traps. That was intentionally a minimum security boundary, not the final stack-ownership model.

### M00-06.02 — First M->S transition with explicit trap-stack ownership — DONE

Development branch: `milestone/m00-06-02-supervisor-transition`.

M00-06.02 chooses a dedicated per-hart runtime M trap stack rather than a reserved region inside the normal M stack.

Selected runtime layout:

```text
per hart

normal M stack
    ordinary M-mode C/C++ execution

trusted M trap stack
    every runtime trap handled by M-mode
    M -> M
    S -> M
    U -> M

S probe stack
    first controlled supervisor payload
```

There is still only one architectural x2/sp register per hart. The three stack names describe software-owned memory regions. Trap entry saves the interrupted x2 value, then loads the trusted trap-stack address into x2.

Accepted state transition:

```text
M mode
  -> establish non-overlapping trusted per-hart trap stack
  -> keep normal M call stack intact
  -> disable delegation/asynchronous noise for the experiment
  -> satp = 0
  -> install a permissive, unlocked PMP entry for probe execution
  -> set MPP = S and mepc = S entry
  -> mret
  -> S entry immediately establishes its own S stack
  -> emit machine-checkable S-entry marker
  -> park in S mode
```

Accepted invariants:

```text
[x] a live normal M-mode call stack is not used as runtime TrapFrame storage
[x] every runtime M-level trap uses a dedicated per-hart trusted trap stack
[x] trap entry saves interrupted x2/sp as a value before switching stacks
[x] M-mode never dereferences interrupted S/U stack storage during trap entry
[x] M-origin TrapFrame/recoverable/timer semantics are preserved after moving frame storage
[x] TrapFrame regression verifies the frame lies inside the current hart's trap stack
[x] nested/double runtime traps fail closed instead of overwriting the active TrapFrame
[x] the S entry establishes its own S stack before any call or stack-using operation
[x] medeleg = 0 and mideleg = 0 for the first transition probe
[x] satp = 0 for the first transition probe
[x] asynchronous M interrupts are disabled for the transition probe
[x] temporary PMP entry permits the S probe to execute/use RAM/UART without claiming isolation
[x] dedicated probe build emits M00_06_02_TRANSITION_ARMED and M00_06_02_SUPERVISOR_ENTRY markers
[x] CI includes a machine-checkable M00-06.02 transition test
```

Acceptance command:

```bash
bash scripts/test-m00-06-02-supervisor-transition.sh
```

Accepted CI evidence:

```text
GitHub Actions run 31568251600
RV64 QEMU regression: PASS
M00-06.02 supervisor transition: PASS
```

The first supervisor probe initially trapped immediately after `mret` with an instruction-access fault. That result showed two useful facts at once: the lower-origin trap was safely captured on the new trusted M trap stack, and bare `satp=0` alone did not make the S probe executable on the QEMU CPU while PMP was present. M00-06.02 therefore installs a deliberately permissive, unlocked PMP NAPOT entry before `mret`. This entry is only bootstrap permission for the transition experiment; it is not a service-isolation policy.

### M00-06.03 — S->M ECALL round trip — DONE

Development branch: `milestone/m00-06-03-supervisor-ecall-return`.

Accepted state transition:

```text
M-mode probe arm
    -> MPP = S, mepc = supervisor ECALL payload
    -> mret
    -> S entry installs S stack
    -> emit M00_06_03_SUPERVISOR_ENTRY
    -> install known gp/a0/a7 markers
    -> ecall
    -> M trap entry saves interrupted S context as values
    -> switch to per-hart trusted M trap stack
    -> construct TrapFrame
    -> validate S-origin state
    -> advance saved mepc by the 32-bit ECALL length
    -> common trap restore
    -> mret
    -> resume at instruction after ECALL in S-mode
    -> validate restored sp/gp/a0/a7
    -> emit M00_06_03_SUPERVISOR_ECALL_RETURN
```

The M00-06.03 handler is compiled only for the dedicated probe build. Ordinary firmware does not silently treat arbitrary S-mode ECALLs as successful syscalls.

Accepted M-side invariants:

```text
[x] mcause == environment call from S-mode (9)
[x] mstatus.MPP == S
[x] mepc == jixia_m00_06_03_ecall_site
[x] saved x2/sp lies in the S probe stack range
[x] saved gp equals the S marker rather than firmware gp
[x] saved a0/a7 equal the S marker values
[x] TrapFrame is aligned to TRAP_FRAME_ALIGNMENT
[x] the complete TrapFrame lies inside current HartLocal.trap_stack_bottom..trap_stack_top
[x] HartLocal.trap_active == 1 while the handler owns the trap stack
[x] saved mepc is modified only after every validation succeeds
```

Accepted S-side restore invariants:

```text
[x] mret resumes at jixia_m00_06_03_after_ecall
[x] restored x2/sp equals the original S probe stack top
[x] restored gp equals the S gp marker
[x] restored a0 equals the S a0 marker
[x] restored a7 equals the S a7 marker
[x] firmware gp is explicitly re-established before calling normal C ABI code
[x] address materialization before gp validation is protected from gp relaxation
```

Shared marker values live in `microkernel/arch/riscv/privilege_transition_test_values.h` so assembly and C++ consume one test contract.

Acceptance command:

```bash
bash scripts/test-m00-06-03-supervisor-transition.sh
```

Machine-checkable markers:

```text
M00_06_03_ECALL_ARMED: PASS
M00_06_03_SUPERVISOR_ENTRY: PASS
M00_06_03_SUPERVISOR_ECALL_RETURN: PASS
M00-06.03 supervisor ECALL round trip: PASS
```

The acceptance script also requires the completed SMP, Kernel Print, recoverable-trap, and machine-timer regressions and rejects fatal-trap or explicit M00-06.03 FAIL output before accepting the round trip.

### M00-06.04 — Hostile lower-privilege stack and full acceptance — NEXT

Branch: `milestone/m00-06-04-privilege-boundary-acceptance`.

Goal: prove the privilege boundary fails closed rather than merely working on the normal path.

Required evidence:

```text
[ ] deliberately hostile/invalid S sp cannot redirect M-mode TrapFrame writes
[ ] lower-origin entry still switches to trusted M storage
[ ] unexpected privilege/anchor states fail through a controlled path
[ ] M00-06 acceptance marker is machine-checkable
[ ] 1/2/4-hart M00-05 population regressions still pass
[ ] TrapFrame, recoverable trap, machine timer, and Kernel Print regressions still pass
[ ] delegation, PMP, paging, nested-trap, and security limitations are recorded
```

M00-06 is DONE only after M00-06.04 passes and the umbrella milestone is closed in `docs/JIXIA_PROGRESS.md`.

## 3. Trap-entry trust and stack model

Two questions remain independent on every M-level trap entry:

```text
1. Is a HartLocal anchor installed in mscratch?
2. What previous privilege is recorded in mstatus.MPP?
```

However, M00-06.02 deliberately removes previous privilege from runtime stack selection. Once HartLocal exists, every runtime M-level trap uses the same trusted per-hart trap stack.

Accepted policy:

```text
runtime trap + HartLocal anchor
    -> preserve interrupted t0/t1/x2 without dereferencing interrupted x2
    -> reject nested trap if trap_active is already set
    -> switch x2 to HartLocal.trap_stack_top
    -> construct TrapFrame on trusted per-hart trap stack
    -> save mstatus.MPP as part of TrapFrame
    -> dispatch
    -> clear trap_active
    -> restore interrupted x2
    -> mret

early M-origin trap + no HartLocal anchor
    -> bootstrap-only current-M-stack path

S/U-origin trap + no HartLocal anchor
    -> fail closed using registers only
```

`mscratch -> HartLocal` is the trusted per-hart anchor, not the stack itself.

The historical M00-06.01 branch distinguished M-origin and lower-origin stack paths. M00-06.02 intentionally unifies runtime trap storage while retaining MPP for semantic origin/return decisions.

## 4. First-transition policy

For the accepted M00-06.02/M00-06.03 probes:

```text
satp      = 0
medeleg   = 0
mideleg   = 0
MIE       = 0
mie       = 0
pmpaddr0  = maximal NAPOT encoding
pmpcfg0   = RWX + NAPOT, unlocked (probe only)
```

This keeps the experiment focused on privilege transition rather than paging, delegation, asynchronous interaction, or real PMP isolation.

The permissive PMP entry is necessary only to grant the S probe access to the firmware RAM/S stack/QEMU UART address space in this experiment. It deliberately does not claim protection between M and S. PMP-backed service ownership and isolation remain later work.

The S-mode markers use direct QEMU UART access through the existing `uart_puts()` helper after the S stack is established. That access is test observability, not a supervisor console ABI.

Future S-mode delegated traps will use a separate `stvec/scause/sepc/stval/sscratch` path and are not part of the current M-mode trap entry.

## 5. Stack ownership rule

The transition must not create an M-mode interval in which firmware executes ordinary stack-using code on lower-privilege storage.

M00-06.03 sequence:

```text
normal M-mode C++
    x2 -> normal per-hart M stack

M transition trampoline
    still x2 -> normal M stack
    configure PMP/satp/delegation/mstatus/mepc
    mret

S entry
    inherited x2 exists only for the first register-only instructions
    immediately x2 -> dedicated S probe stack
    then calls/stack use are permitted

S ECALL
    hardware enters M but leaves x2 holding the S value
    trap entry saves x2 as a value without dereferencing it
    x2 -> dedicated per-hart M trap stack
    TrapFrame is constructed only on trusted M storage

M return
    common restore writes the saved S x2 value back last
    mret returns to S after the ECALL
```

## 6. Nested-trap policy

M00-06 does not support nested M-level traps.

Each HartLocal has a `trap_active` state:

```text
0 -> runtime trap may claim the per-hart trap stack
1 -> another trap would overwrite active trap state, so fail closed
```

This is deliberately stronger than silently resetting x2 to the trap-stack top and overwriting an outer TrapFrame. A later milestone may introduce a real trap-depth/stacking policy if required.

## 7. Integration policy

M00-06 is one architectural milestone with multiple accepted checkpoints.

```text
main
  |
  +-- accepted M00-06.01 checkpoint
  |
  +-- accepted M00-06.02 checkpoint
  |
  +-- M00-06.03 branch from latest main
  |       -> test/CI
  |       -> squash accepted checkpoint into main
  |
  `-- M00-06.04 branch from latest main
          -> full M00-06 acceptance
          -> squash closure into main
```

Development branches may contain fine-grained commits. `main` keeps stable semantic checkpoints rather than debug/fixup history.

## 8. Explicit non-goals

M00-06 does not add:

```text
Sv39/page tables
physical allocator
scheduler/tasks
U-mode applications
PMP-backed service isolation
ArchHV / HS / VS
G-stage translation
full syscall ABI
nested trap support
```

Those mechanisms remain later milestones.
