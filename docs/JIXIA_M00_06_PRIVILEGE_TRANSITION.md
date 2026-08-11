# Jixia M00-06 Privilege Transition Foundation

**Status:** ACTIVE  
**Stable baseline:** `main`  
**Umbrella branch:** `milestone/m00-06-privilege-transition`  
**Current submilestone branch:** `milestone/m00-06-02-supervisor-transition`

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
    -> controlled return or M-mode continuation
```

The architectural boundary is not merely the `mret` instruction. The critical security invariant is that M-mode must not trust lower-privilege stack storage when a trap arrives from S-mode.

## 2. Submilestone gates

M00-06 is split into four acceptance gates. Only one submilestone is ACTIVE at a time.

### M00-06.01 — Trusted M-mode trap entry — DONE

Accepted baseline: `b6b7e9c` (`milestone: establish Jixia baseline through M00-06.01`).

Delivered mechanisms:

```text
[x] explicit early mscratch = 0 state before HartLocal exists
[x] HartLocal assembly ABI for trap-entry scratch state
[x] privilege-aware trap entry using mstatus.MPP
[x] M-origin traps continue to use the current trusted M stack
[x] lower-origin traps do not dereference the interrupted sp before switching trust domains
[x] lower-origin x2/sp value is preserved in the TrapFrame
[x] lower-origin gp is preserved and kernel gp is restored before C++ dispatch
[x] TrapFrame remains the common C++ dispatcher contract
[x] existing M-mode TrapFrame/recoverable/timer/SMP regressions remain valid
[x] clean CI baseline builds and passes RV64 QEMU regression
```

Known limitation carried into M00-06.02:

> The lower->M entry currently uses the per-hart stack top as trusted storage. Before executing real S-mode code, M00-06.02 must make trap-stack ownership explicit so a lower-origin TrapFrame cannot overwrite a live normal M-mode call stack.

Nested/double traps remain unsupported during this milestone.

### M00-06.02 — First M->S transition with trusted trap-stack ownership — ACTIVE

Branch: `milestone/m00-06-02-supervisor-transition`.

Goal:

```text
M mode
  -> establish non-overlapping trusted trap-stack region
  -> disable/define delegation for the experiment
  -> satp = 0
  -> set MPP = S and mepc = S entry
  -> mret
  -> prove execution reached S mode
```

Required invariants:

```text
[ ] a live M-mode call stack cannot be overwritten by a lower-origin TrapFrame
[ ] M-mode never dereferences an untrusted S-mode sp during trap entry
[ ] the S entry establishes its own S stack as its first stack-owning action
[ ] medeleg/mideleg policy is explicit for the experiment
[ ] asynchronous noise is disabled or deliberately controlled
[ ] existing M-origin trap behavior is unchanged
```

The preferred first layout is either a dedicated per-hart trap stack or an explicitly reserved non-overlapping trap region. The choice must be documented before the first `mret` to S-mode.

### M00-06.03 — S->M ECALL round trip — NEXT

Planned branch name: `milestone/m00-06-03-supervisor-ecall-return`.

Goal:

```text
S payload
    -> ecall
    -> M trap entry on trusted storage
    -> jixia_trap_dispatch(TrapFrame*)
    -> validate S-origin state
    -> controlled return to S or controlled M continuation
```

Machine-checkable proof must validate at least:

```text
mcause == environment call from S-mode (9)
mstatus.MPP == S
saved x2/sp lies in the S stack range
saved gp is the S payload value, not the firmware gp
selected a0/a7 marker values survive entry
TrapFrame address lies in trusted M-mode storage
mepc matches the expected S-mode ECALL site
```

### M00-06.04 — Hostile lower-privilege stack and full acceptance — PLANNED

Planned branch name: `milestone/m00-06-04-privilege-boundary-acceptance`.

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

## 3. Trap-entry trust model

Two questions are independent on every M-mode trap entry:

```text
1. Is a HartLocal anchor installed in mscratch?
2. What was the previous privilege recorded in mstatus.MPP?
```

The intended policy is:

```text
M-origin trap + trusted current M stack
    -> save TrapFrame on current trusted stack

S/U-origin trap + HartLocal anchor
    -> preserve interrupted register values without trusting interrupted sp
    -> switch to trusted per-hart M trap storage
    -> build TrapFrame there

S/U-origin trap + no HartLocal anchor
    -> fail closed without storing through untrusted sp
```

`mscratch -> HartLocal` is a per-hart anchor, not the trap stack itself.

## 4. First-transition policy

For the first proof:

```text
satp    = 0
medeleg = 0
mideleg = 0
```

unless the implementation record explicitly documents a narrower controlled policy. This keeps the experiment focused on privilege transition rather than paging or delegated supervisor trap handling.

Future S-mode delegated traps will use a separate `stvec/scause/sepc/stval/sscratch` path and are not part of the current M-mode trap entry.

## 5. Stack ownership rule

The transition must not create an M-mode interval in which firmware code is executing ordinary stack-using code on a lower-privilege stack.

Preferred sequence:

```text
M-mode trampoline still owns trusted M stack
    -> prepare mstatus/mepc
    -> mret
S-mode entry
    -> establish S stack immediately
    -> execute payload
```

If a trap arrives immediately after `mret` and before S establishes its stack, `mstatus.MPP=S` still forces the lower-origin trusted-entry path; M-mode must not trust the inherited `sp` value.

## 6. Integration policy

M00-06 is one architectural milestone with multiple accepted checkpoints.

```text
main
  |
  +-- accepted M00-06.01 checkpoint
  |
  +-- M00-06.02 working branch
  |       -> test/CI
  |       -> squash accepted checkpoint into main
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

## 7. Explicit non-goals

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
