# Jixia Microkernel Verification Foundation

**Status:** executable verification foundation with external acceptance
evidence complete; ready for PR review, but not a milestone closure record

**Date:** 2026-08-21

**Scope:** current M00-08.03 scheduler and IPC work, with interfaces reserved for
blocking IPC and later kernel facilities

## 1. Ownership decision

Feature implementation and feature-local acceptance may be delegated to GLM.
The independent verification side remains owned by ChatGPT and the project
architect:

- freeze invariants and failure semantics before reviewing an implementation;
- write the executable models, torture workloads, offline checkers and
  performance benchmarks;
- decide whether evidence supports a safety, liveness or performance claim;
- analyze failures and reject tests that merely recognize expected PASS text;
- review GLM changes against the model and acceptance contract.

The same agent must not silently weaken an oracle to make its implementation
pass. Feature code and independent verification should normally arrive in
separate commits or PRs and be reviewed as separate artifacts.

## 2. Benchmark is an umbrella, not one number

Jixia needs three explicitly different suites:

| Suite | Question | Failure meaning |
| --- | --- | --- |
| correctness/litmus | Is a forbidden state or history reachable? | kernel/specification defect |
| torture/soak | Can adversarial timing or teardown expose a defect? | counterexample to analyze |
| performance | What are latency, throughput, scaling and tail costs? | regression/trade-off, not a proof |

A throughput result never establishes FIFO, mutual exclusion, exactly-once
delivery or starvation freedom. A stress pass finds no counterexample for the
tested schedules; it does not prove absence of one.

## 3. Evidence labels

Every claim must carry the strongest evidence actually obtained:

```text
PROVEN          machine-checked proof under named assumptions
MODEL_CHECKED   exhaustive exploration of a named bounded abstract model
LITMUS          a focused target execution checking one forbidden outcome
STRESS_ONLY     randomized/soak evidence with seed, duration and topology
UNPROVEN        design argument or test exists, but no proof/refinement exists
```

`MODEL_CHECKED` must not be relabeled `PROVEN`. In particular, the current
Python state models are independent specifications; correspondence from those
models to freestanding C++ and RISC-V assembly is still `UNPROVEN`.

## 4. Verification pipeline

```text
accepted API and error precedence
        -> abstract state and invariants
        -> linearization points and lock requirements
        -> executable small-state exploration
        -> verification-only kernel events/test points
        -> QEMU trace + independent offline history checker
        -> host sanitizer/contention torture of production algorithms
        -> target litmus, nightly seed matrix and long soak
        -> performance distributions and regression budgets
        -> staged refinement/formal proof
```

The oracle lives outside the kernel whenever possible. The kernel emits facts;
it does not emit a conclusion that the checker blindly trusts.

## 5. Hook contract

`microkernel/verify/trace.h` defines a verification-only boundary. With
`JIXIA_VERIFICATION` disabled:

- macro arguments are not evaluated;
- trace storage and implementation are not linked;
- no call, branch, counter, lock or structure field survives in production;
- normal ABI and endpoint/scheduler semantics remain unchanged.

With verification enabled, CMake emits a distinctly named `jixia-verify.elf` /
`jixia-verify.bin` test image. The kernel records fixed-size events into per-hart,
single-writer buffers. Records contain a global sequence, timestamp, hart,
operation id, event, declared lockset, object, subject and two arguments. There
is no allocation, `printk` or kernel lock acquisition in the recording path.
The completed acceptance workload dumps records only after the measured
operations; a Python checker reconstructs histories in global sequence order.

The vocabulary covers:

- endpoint create/destroy/send/blocking-receive/try-receive begin, rejection and
  linearization;
- waiter publication, FIFO wake, receive-result publication and later READY
  publication;
- run-queue insertion/removal begin, rejection and publication/selection;
- task-current publication;
- deterministic bounded delay at named test points;
- waiter-popped and READY-publication perturbation points.

Each linearization event declares the lockset that must be held. The checker
fails closed on trace overflow, sequence gaps, duplicate operation completion,
wrong locksets, queue-count mismatch, FIFO violations, duplicate task
membership and stale-handle success.

## 6. Current executable evidence

### 6.1 Independent small-state models

`verification/model/ipc_model_check.py` exhaustively explores a deliberately
small endpoint state machine. It covers create, destroy, send, blocking receive,
message FIFO, receiver FIFO, exactly-once wake, generation advancement and
permanent retirement at the generation ceiling. Messages and waiters are never
simultaneously pending, and a task cannot wait on two endpoints.

`verification/model/scheduler_model_check.py` explores three tasks on two
harts and checks exactly-one membership among READY/run queue,
RUNNING/current-hart, BLOCKED/wait set and ENDED/no membership.

The models print their state/transition counts and explicitly print that C++
refinement and scheduler starvation freedom are unproven.

### 6.2 Host torture of production IPC code

`verification/host/ipc_torture.cpp` compiles and links the real
`microkernel/core/ipc_manager.cpp`, rather than a replacement queue. It runs:

- four-producer FIFO validation, independently per sender;
- four-producer/four-consumer exactly-once validation;
- send/receive racing endpoint destruction and stale-handle use;
- repeated generation churn;
- seeded yield/spin perturbations, deadlines and payload checksums.

The harness is built in optimized, ASan/UBSan and optional TSan lanes. To make
this possible without pretending that RISC-V task context is host code,
`TaskId` and `EntryPoint` are factored into `task_types.h`. A hosted compile-only
Task surface and fail-closed stubs satisfy target context/scheduler references
that only the blocking path needs; entering such a path aborts the host run.
The host torture deliberately exercises the non-blocking production operations,
while the actual blocking path is checked by the model and target QEMU trace.

### 6.3 Target trace and nightly runner

The M00-08.03.01 QEMU acceptance entry accepts SMP count, TCG thread
mode, seed, jitter and trace size. Verification mode runs the offline checker.
Single-hart acceptance retains the complete exact-marker and marker-order
oracle. Multi-hart verification does not use concurrent `printk` records as a
history oracle: the current console emits characters without record-level
serialization, so valid lines can interleave. SMP acceptance instead requires
a complete, overflow-free structured trace, exact expected-hart participation
and a passing independent history check. Making normal multi-writer console
records atomic remains a separate product observability requirement.

Inspection after the first SMP run confirmed that this requirement is not
merely cosmetic: `kernel_console::put()` concurrently updates the plain
`write_position`, `kernel_log_buffer` and `was_truncated` state, while UART
output is emitted one character at a time. This is an open product-side data
race as well as a record-framing defect. The verification wrapper avoids using
that racy stream as its SMP history oracle, but does not claim to fix or excuse
the console. A production fix must define interrupt/preemption recursion and
panic behavior; wrapping `printk` in an ordinary spinlock can self-deadlock if
an interrupt or trap prints while the interrupted hart owns that lock.

The nightly runner combines:

- small-state models;
- high-volume host IPC torture with TSan;
- QEMU MTTCG with 2 and 4 harts;
- blocking-receive QEMU acceptance at 1 and 2 harts for every nightly seed;
- a rotating seed matrix and deterministic hook perturbations;
- complete logs and a run manifest retained as artifacts.

The first external execution on commit `2563f14` established a green
single-hart target baseline and completed the 2-hart/4-hart IPC workloads with
full structured traces and zero dropped records. The original SMP wrapper then
misclassified both runs because concurrent console characters corrupted exact
legacy marker lines before the trace checker ran. The corrected checker later
accepted both retained histories (`340` records on 2 harts and `354` records on
4 harts), and fresh corrected-wrapper runs passed again (`340` records on both
2 and 4 harts, all expected harts represented and zero records dropped).
M00-08.03.01 SMP structured-history verification is therefore green for these
named seeds and schedules; this is bounded stress/history evidence, not a
general proof of all interleavings or starvation freedom.

The first execution passed optimized and ASan/UBSan host torture, while the GCC
TSan executable terminated before producing a TSan race report. The captured
Deepin result identified GCC 12.3.0 on Linux 6.18.34 and failed in TSan
initialization with `unexpected memory mapping`, before the torture body
executed. That first result was therefore classified as an inconclusive runtime
environment failure rather than a kernel defect or a sanitizer pass.

Commit `25bcef4` made the Linux harness use
`setarch --addr-no-randomize` when the per-process personality operation is
available. This changes only the sanitized child process, not the host-wide
ASLR sysctls; it can be disabled with `JIXIA_TSAN_DISABLE_ASLR=0` or required
with `JIXIA_TSAN_DISABLE_ASLR=1`. The harness records the compiler/platform,
ASLR mode and available VM layout sysctls, captures sanitizer stderr and
preserves the failing status; `JIXIA_HOST_TSAN_CXX` permits a second
runtime/compiler without weakening the mandatory lane.

External rerun on the same Deepin host at `25bcef4` completed the optimized
seed matrix (`1`, `7`, `42`, 1000 messages per producer), representative
ASan/UBSan seed `1`, and TSan seed `1`. Every workload reported MPSC FIFO,
MPMC exactly-once, destroy-race and generation-churn PASS; TSan executed the
workload and ended with `HOST_IPC_TSAN: PASS`. The recorded environment was GCC
12.3.0, Linux 6.18.34, `tsan_aslr_mode=disabled-for-child`,
`vm.legacy_va_layout=0`, and host-wide `kernel.randomize_va_space=2`. Thus the
host torture acceptance lane is green on the project developer's actual host
without disabling global ASLR. This remains named-seed `STRESS_ONLY` evidence,
not proof that all data races or interleavings are absent.

## 7. Test families to grow with every feature

Every IPC/scheduler increment must add or update its tests in the same
increment. Waiting until all features are complete makes interleavings harder
to localize and allows untestable internal contracts to fossilize.

Required families are:

1. **Boundary and exhaustion:** empty/full queues, endpoint pool full, generation
   ceiling, invalid and stale handles, maximum task/endpoint counts.
2. **Pairwise concurrency:** send/send, send/receive, receive/receive,
   destroy/send, destroy/receive, exit/reply, timeout/wake and interrupt/wake.
3. **History properties:** FIFO, exactly once, no reply-token reuse, one state
   membership, legal state transitions and stable error precedence.
4. **Metamorphic checks:** results invariant under harmless seed, hart-number,
   producer-renaming and independent-endpoint permutations.
5. **Differential checks:** one versus multiple harts, TCG single versus MTTCG,
   verification versus normal build, simulator versus board when available.
6. **Fault injection:** task crash, server exit, endpoint revocation, delayed
   interrupt, forced allocation failure and trace-buffer exhaustion.
7. **Replay and shrinking:** every failure reports seed/topology/build id and a
   compact operation history; reduce a long failure to the shortest replay.
8. **Long liveness:** contention and convoy workloads measure maximum wait,
   service distribution and forward-progress windows, not only aggregate rate.

PR gates stay bounded. Nightly jobs broaden schedules and seeds. Weekly/explicit
soak jobs run large operation counts and simulator/board differentials.

## 8. Safety and liveness obligations

For each shared object the design record must state:

- abstract state, ownership and legal transitions;
- safety invariant and exact linearization point;
- lock order and memory-order/happens-before requirements;
- deadlock assumptions and progress class;
- whether starvation freedom or bounded waiting is guaranteed;
- interrupt, preemption, task-exit and failure behavior;
- which evidence label supports each claim.

The current test-and-set `Spinlock` has a plausible acquire/release mutual
exclusion argument but is intentionally unfair. It does not provide FIFO or
bounded waiting, so Jixia must not claim starvation freedom for it. Contention
measurements can expose bad behavior but cannot upgrade that contract. If
bounded waiting becomes required, use and prove a standard ticket/MCS-style
primitive under the project's interrupt and RISC-V memory-model assumptions.

## 9. Hostboot-derived production/test boundary

The pinned Hostboot tree does not turn its normal boot image into a benchmark
runner. Its build/test flow creates a separate `hbicore_test.bin`, distributes
test modules with `hbDistribute --test`, launches them through CxxTest, and
drives the resulting image under simulation with external watchdog, FFDC and
coverage collection (`src/build/citest/build-script`, `cxxtest-start.sh`,
`autocitest`, `src/usr/cxxtest/cxxtestexec.C`, and `src/makefile`).

Jixia adopts that boundary:

- `jixia.bin` is the production artifact and contains no verification trace or
  jitter machinery;
- `jixia-verify.bin` is the test image and is never a deployable production
  artifact;
- models, history checkers, mutation tests, sanitizer harnesses and nightly
  orchestration remain hosted under `verification/` and `scripts/`;
- both images compile the same IPC state-transition implementation, so the test
  suite does not validate a replacement queue;
- verification needs do not add output parameters or counters to production IPC
  interfaces.

## 10. Open scheduler proof obligation found by this work

The present yield/preemption sequence calls `return_runnable()` before
`set_next_runnable()`. The first call can publish the old current task as READY
and enqueue it globally while the old hart's `HartLocal.current_task` still
points to it. A second hart can dequeue it before the old hart replaces that
pointer. The old hart is executing the trusted trap path rather than the task's
U-mode code, so this is not by itself proof of simultaneous user execution.
It is nevertheless incompatible with a literal abstract invariant saying that
a task is always in exactly one of run queue or any hart's current pointer.

Before a refinement proof, the architecture must choose one of two explicit
models:

1. add a transient DISPATCHING/RELINQUISHING ownership state and prove that an
   old current pointer grants no execution right; or
2. change publication order so current ownership is relinquished before the
   task becomes globally READY, while still closing all lost-wakeup windows.

This document does not silently alter production scheduling. Reserved trace
points make the transition observable in the next scheduler/IPC increment.

## 11. seL4-inspired formal path

Jixia should copy seL4's discipline, not claim seL4's assurance level. The
useful staged structure is:

1. freeze a mathematical abstract specification for one small subsystem;
2. define an executable specification with the same externally visible
   transitions;
3. prove safety invariants and operation refinement;
4. connect the executable spec to the C++ implementation at named
   linearization points;
5. add compiler/binary and RISC-V memory-model obligations only after the
   source refinement is stable;
6. keep integration, assumption and unverified-code tests even after proofs.

Start with the bounded endpoint manager, then blocking IPC transaction/reply
state, then task/run-queue membership. Do not begin with the entire kernel or
with performance/liveness claims. Proof-friendly implementation rules include
static pools, explicit generations, deterministic error precedence, small
transition functions, one documented lock order and no hidden probe mutation.

Official seL4 material makes two important constraints clear: its proofs form
layers from specification to C (and for selected configurations to binary),
and verified guarantees are configuration-specific; testing remains necessary
for assumptions, unverified code and integration. References:

- <https://sel4.systems/Verification/proofs.html>
- <https://docs.sel4.systems/projects/sel4/verified-configurations.html>
- <https://docs.sel4.systems/projects/sel4test/>

## 12. Immediate acceptance boundary

This verification foundation is acceptable for review when:

- normal production builds contain no trace implementation or probe effect;
- verification C++ passes warning-clean syntax checks;
- both executable models pass and retain explicit UNPROVEN labels;
- optimized, ASan/UBSan and TSan host torture passes with recorded seeds;
- the offline checker rejects synthetic overflow, sequence, FIFO, count,
  duplicate-completion and lockset violations;
- at least one project-toolchain QEMU SMP trace passes before any CI gate is
  enabled;
- the scheduler ownership transition above is recorded as an open proof
  obligation, not papered over by the checker.

No M00-08.03 ledger state is changed by this document. Milestone closure still
requires the canonical acceptance and CI evidence of the implemented feature.
