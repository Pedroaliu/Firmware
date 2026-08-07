# Jixia Concurrency Correctness Rules

**Status:** architectural rule / future implementation contract  
**Date:** 2026-08-07  
**Scope:** Mozi microkernel, per-hart runtime, IPC, scheduling, lock-based and lock-free shared-state algorithms

## 1. Core decision

Jixia should not invent synchronization algorithms casually.

For mutexes, spinlocks, reader/writer locks, queues, atomics, barriers, reference counting, reclamation and other concurrent primitives, prefer established algorithms whose correctness and memory-ordering requirements are already well understood.

When Jixia must create or materially modify a concurrent algorithm, correctness is not accepted by stress testing alone. The implementation must have an explicit proof argument for its safety and liveness contract.

The default engineering order is:

```text
need for concurrency
        |
        v
can ownership/per-hart partitioning remove sharing?
        |
   yes  |  no
        |   |
        |   v
        |  use a standard published algorithm if possible
        |   |
        |   v
        |  adapt only the minimum required pieces
        |   |
        +---+----> state invariants + proof obligations
                     |
                     v
               executable validation
```

The first optimization is therefore often **less sharing**, not a more clever lock.

## 2. Required properties for mutual exclusion algorithms

A Jixia synchronization primitive must state exactly which properties it guarantees.

For a general-purpose mutex/lock intended to serialize a critical section, the desired contract is:

### 2.1 Mutual exclusion / safety

At most one owner is in the protected critical section at a time.

A proof must establish an invariant equivalent to:

```text
owners(critical_section) <= 1
```

No interleaving may permit two harts/threads to believe simultaneously that they own the same exclusive lock.

### 2.2 Deadlock freedom / global progress

If participating threads continue to take steps and the lock holder eventually releases the lock, the system must not reach a state in which all contenders are permanently unable to make progress solely because of the synchronization algorithm.

For multiple-lock protocols, this includes explicit lock-order or dependency rules where required.

### 2.3 Starvation freedom / bounded waiting

A general Jixia lock should not silently permit one waiter to be postponed forever while other contenders repeatedly enter the critical section.

If an algorithm is intentionally unfair for performance reasons, that must be part of the primitive's name/contract and its permitted call sites must be restricted. "Eventually probably gets the lock" is not a correctness argument.

Where bounded waiting is claimed, the proof should state the bound or the assumptions from which a bound follows.

### 2.4 Ownership and release correctness

The primitive must define:

- who may acquire;
- who may release;
- whether recursion is legal;
- behavior in interrupt context;
- behavior if a thread/hart fails while owning the primitive;
- whether migration between harts is legal while held.

These are part of the algorithm, not merely API documentation.

## 3. Lock-free algorithms need different proof obligations

"Lock-free" does not mean "correct because there is no mutex".

Every lock-free concurrent object must state its progress class:

```text
blocking
obstruction-free
lock-free
wait-free
```

The terms must be used precisely.

For a lock-free data structure, Jixia should normally require:

### 3.1 Linearizability

Each externally visible operation must have a defensible linearization point so the concurrent history is equivalent to a valid sequential history that respects real-time ordering.

Example reasoning form:

```text
enqueue(x)
    prepare node
    publish with successful CAS   <-- linearization point
    post-publication cleanup
```

### 3.2 Progress proof

If an operation retries a CAS loop, explain why retries imply that some competing operation made progress, rather than allowing the whole system to livelock forever.

If wait-freedom is claimed, every operation needs its own finite-step bound under the stated assumptions.

### 3.3 ABA and object lifetime

Algorithms using compare/exchange on pointers or indices must explicitly address object reuse and ABA where applicable.

Memory reclamation must be part of the proof contract. Hazard pointers, epochs, RCU-like grace periods, tagged generations or another established scheme should be used deliberately rather than adding ad-hoc lifetime rules.

## 4. Memory-model correctness is part of the proof

Sequentially consistent pseudocode is not enough.

Jixia runs on RISC-V and is written largely in freestanding C++. A concurrency proof therefore has two layers:

```text
algorithmic proof
    mutual exclusion / progress / linearizability

        +

memory-order proof
    compiler ordering
    C++ atomic ordering
    RISC-V visibility/order requirements
```

For every shared-memory primitive, document:

- which fields are atomic;
- which operations are acquire/release/relaxed/seq_cst or use explicit fences;
- which writes must happen-before which reads;
- why compiler and hardware reordering cannot violate the abstract proof.

A proof that assumes SC while the implementation uses weaker ordering is incomplete.

## 5. Preferred proof style for Jixia-written algorithms

Not every small primitive needs a full theorem-prover development, but every nontrivial original algorithm needs a proof artifact that another engineer can review.

Preferred structure:

### 5.1 State model

Define the state variables and legal states.

Example:

```text
owner      = NONE | hart/thread id
next_ticket
serving_ticket
waiter state
```

### 5.2 Invariants

Write invariants before implementation details.

Examples:

```text
I1: serving_ticket <= next_ticket
I2: at most one participant has ticket == serving_ticket in the critical section
I3: ticket values are not reused while an older live ticket can still exist
```

### 5.3 Transition proof

For each operation, show that every state transition preserves the invariants.

Typical transitions:

```text
acquire
retry/wait
enter critical section
release
preemption/interrupt interaction
```

### 5.4 Liveness argument

State scheduler/hardware assumptions explicitly and show why a waiter cannot starve under those assumptions when starvation freedom is part of the contract.

### 5.5 Linearization point

For concurrent objects, identify the exact event at which each operation logically takes effect.

### 5.6 Failure and exceptional context

Consider at least:

- interrupt arriving during an operation;
- nested trap policy;
- panic/machine-check path;
- hart stopping or failing;
- cancellation/termination if later supported.

The emergency/RAS path must not depend on a lock that can remain owned by failed state.

## 6. Verification ladder

Jixia uses several layers of evidence. They complement rather than replace one another.

```text
1. written invariants and proof sketch
2. compile-time/static checks where applicable
3. small-state exhaustive model/model checking where valuable
4. memory-model litmus tests
5. deterministic simulator schedules / Jingjie replay
6. randomized stress and fault injection
7. real multi-hart hardware/QEMU tests
```

A stress test can find bugs but cannot prove their absence.

For subtle algorithms, future tools may include TLA+/PlusCal, Spin/Promela, CBMC, herd-style memory-model litmus tests, or an equivalent model. Tool choice is secondary to preserving a small explicit state model and stated proof obligations.

## 7. Design bias: ownership before synchronization

Jixia should prefer designs such as:

```text
per-hart state
single-writer data
message passing
immutable snapshots
explicit ownership transfer
```

over global shared mutable state when practical.

This applies directly to future kernel logging, scheduling state, interrupt bookkeeping and firmware work queues.

For example:

```text
Hart0 -> Hart0 local queue/log/state
Hart1 -> Hart1 local queue/log/state
```

is preferred to immediately introducing one global lock when later aggregation can happen outside the critical path.

## 8. Kernel and interrupt restrictions

Synchronization used in Mozi must additionally declare context constraints.

A normal lock that can sleep or depend on the scheduler is not automatically valid in:

```text
interrupt handler
trap path
panic path
machine-check/RAS emergency path
very early boot
```

Likewise a spinlock used with interrupts enabled can deadlock if the local interrupt handler attempts to acquire the same lock.

Therefore every primitive should eventually carry a context contract such as:

```text
thread-only
irq-safe
nmi/machine-check-safe
panic-safe
boot-safe
```

Do not infer these properties from the word "spinlock".

## 9. Reuse policy

When a well-known algorithm satisfies the required contract, reuse the algorithmic design rather than "improving" it without evidence.

Examples of areas where classic designs should be considered first:

```text
ticket/MCS-style scalable locks
seqlock-style read-mostly state
SPSC/MPSC queues
bounded ring queues
reference-count patterns
RCU/epoch-style reclamation
barriers
work stealing/deques where justified
```

The exact primitive must still be checked against RISC-V/C++ memory ordering, Jixia interrupt semantics and firmware failure requirements.

## 10. Acceptance rule for new concurrent primitives

A new or materially modified Jixia concurrent primitive is not DONE until its design record states:

```text
[ ] abstract state
[ ] ownership model
[ ] safety invariants
[ ] mutual exclusion property when applicable
[ ] deadlock/progress property
[ ] starvation/bounded-waiting property or explicit documented exception
[ ] linearization points for concurrent objects when applicable
[ ] memory-order requirements
[ ] interrupt/preemption assumptions
[ ] failure/panic constraints
[ ] executable tests/model evidence
[ ] known limitations
```

This rule exists so concurrency correctness is an architecture property, not a conclusion drawn from "the test ran many times without failing."

## 11. Relationship to Jixia trace/debug vision

Concurrency verification and Jixia's waveform/timeline observability are complementary.

The trace system should eventually expose enough information to visualize:

```text
lock acquire / release
wait intervals
ownership transfer
IPC send / receive
interrupt entry / exit
scheduler transitions
atomic retry counts
queue depth
cross-hart causal flow
```

This makes execution evidence understandable, but visualization is not the proof itself. The mathematical invariant/progress argument remains the correctness contract; trace and Jingjie replay are powerful counterexample and debugging tools.
