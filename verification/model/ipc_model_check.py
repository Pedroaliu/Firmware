#!/usr/bin/env python3

"""Exhaustive small-state checker for Jixia Endpoint IPC through blocking recv.

This model is deliberately independent of the C++ implementation.  It checks
the accepted M00-08.03.01/.03.02 state machine with tiny bounds, including
pending-message FIFO, receiver-wait FIFO, destroy wake, and permanent slot
retirement at the generation ceiling. It is not a refinement proof of the C++
code; that distinction is printed in the evidence summary.
"""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass, replace
from typing import Iterable, NamedTuple


TASKS = (1, 2)
PAYLOADS = (0x11, 0x22)
SLOT_COUNT = 2
QUEUE_DEPTH = 2
MAX_GENERATION = 2
NO_OWNER = 0


@dataclass(frozen=True)
class Endpoint:
    allocated: bool = False
    active: bool = False
    retired: bool = False
    generation: int = 0
    owner: int = NO_OWNER
    queue: tuple[tuple[int, int], ...] = ()
    waiters: tuple[int, ...] = ()
    issued_generations: frozenset[int] = frozenset()


@dataclass(frozen=True)
class State:
    endpoints: tuple[Endpoint, ...]


class Transition(NamedTuple):
    name: str
    slot: int
    actor: int
    payload: int
    receiver: int
    before_queue: tuple[tuple[int, int], ...]
    after_queue: tuple[tuple[int, int], ...]


def update_endpoint(state: State, slot: int, endpoint: Endpoint) -> State:
    endpoints = list(state.endpoints)
    endpoints[slot] = endpoint
    return State(tuple(endpoints))


def check_state(state: State) -> None:
    assert len(state.endpoints) == SLOT_COUNT

    waiting_tasks: set[int] = set()
    for endpoint in state.endpoints:
        assert 0 <= endpoint.generation <= MAX_GENERATION
        assert len(endpoint.queue) <= QUEUE_DEPTH
        assert len(endpoint.waiters) <= len(TASKS)
        assert not (endpoint.queue and endpoint.waiters)

        if endpoint.active:
            assert endpoint.allocated
            assert not endpoint.retired
            assert endpoint.owner in TASKS
            assert endpoint.generation != 0
            assert endpoint.generation in endpoint.issued_generations
        else:
            assert endpoint.owner == NO_OWNER
            assert not endpoint.queue
            assert not endpoint.waiters

        if endpoint.retired:
            assert endpoint.allocated
            assert not endpoint.active
            assert endpoint.generation == MAX_GENERATION

        if not endpoint.allocated:
            assert not endpoint.active
            assert not endpoint.retired

        for generation in endpoint.issued_generations:
            assert 1 <= generation <= MAX_GENERATION

        # Every older published handle must remain stale forever.  A live
        # endpoint may use only its current generation, never an older epoch.
        if endpoint.active:
            for stale_generation in endpoint.issued_generations:
                if stale_generation != endpoint.generation:
                    assert stale_generation < endpoint.generation

        for waiter in endpoint.waiters:
            assert waiter in TASKS
            assert waiter not in waiting_tasks
            waiting_tasks.add(waiter)


def check_transition(transition: Transition) -> None:
    if transition.name == "send":
        assert transition.after_queue == transition.before_queue + (
            (transition.actor, transition.payload),
        )
    elif transition.name == "recv":
        # FIFO is checked at the abstract linearization point, independently
        # of any target-side log ordering.
        assert transition.before_queue
        assert transition.after_queue == transition.before_queue[1:]
        assert transition.actor == transition.before_queue[0][0]
        assert transition.payload == transition.before_queue[0][1]
    elif transition.name == "send_wake":
        assert transition.receiver in TASKS
        assert not transition.after_queue


def successors(state: State) -> Iterable[tuple[Transition, State]]:
    # endpoint_create chooses the lowest free, non-retired slot exactly as the
    # current implementation does.  Owner is nondeterministic over all tasks.
    free_slot = next(
        (
            index
            for index, endpoint in enumerate(state.endpoints)
            if not endpoint.allocated and not endpoint.retired
        ),
        None,
    )
    if free_slot is not None:
        before = state.endpoints[free_slot]
        generation = 1 if before.generation == 0 else before.generation
        assert generation not in before.issued_generations
        for owner in TASKS:
            after = replace(
                before,
                allocated=True,
                active=True,
                owner=owner,
                generation=generation,
                queue=(),
                waiters=(),
                issued_generations=before.issued_generations | {generation},
            )
            yield (
                Transition("create", free_slot, owner, 0, 0, (), ()),
                update_endpoint(state, free_slot, after),
            )

    for slot, endpoint in enumerate(state.endpoints):
        if not endpoint.active:
            continue

        # Only the owner can destroy.  Destroy either advances the epoch and
        # frees the slot or permanently retires it at the ceiling.
        if endpoint.generation == MAX_GENERATION:
            destroyed = replace(
                endpoint,
                active=False,
                retired=True,
                owner=NO_OWNER,
                queue=(),
                waiters=(),
            )
        else:
            destroyed = replace(
                endpoint,
                allocated=False,
                active=False,
                owner=NO_OWNER,
                generation=endpoint.generation + 1,
                queue=(),
                waiters=(),
            )
        yield (
            Transition("destroy", slot, endpoint.owner, 0, 0, endpoint.queue, ()),
            update_endpoint(state, slot, destroyed),
        )

        if endpoint.waiters:
            receiver = endpoint.waiters[0]
            for sender in TASKS:
                for payload in PAYLOADS:
                    sent = replace(endpoint, waiters=endpoint.waiters[1:])
                    yield (
                        Transition(
                            "send_wake", slot, sender, payload, receiver, endpoint.queue, ()
                        ),
                        update_endpoint(state, slot, sent),
                    )
        elif len(endpoint.queue) < QUEUE_DEPTH:
            for sender in TASKS:
                for payload in PAYLOADS:
                    queue = endpoint.queue + ((sender, payload),)
                    sent = replace(endpoint, queue=queue)
                    yield (
                        Transition("send", slot, sender, payload, 0, endpoint.queue, queue),
                        update_endpoint(state, slot, sent),
                    )

        if endpoint.queue:
            sender, payload = endpoint.queue[0]
            queue = endpoint.queue[1:]
            received = replace(endpoint, queue=queue)
            yield (
                Transition("recv", slot, sender, payload, 0, endpoint.queue, queue),
                update_endpoint(state, slot, received),
            )
        else:
            waiting_anywhere = {
                waiter
                for candidate in state.endpoints
                for waiter in candidate.waiters
            }
            for receiver in TASKS:
                if receiver in waiting_anywhere:
                    continue
                blocked = replace(endpoint, waiters=endpoint.waiters + (receiver,))
                yield (
                    Transition("recv_block", slot, receiver, 0, receiver, (), ()),
                    update_endpoint(state, slot, blocked),
                )


def main() -> int:
    initial = State(tuple(Endpoint() for _ in range(SLOT_COUNT)))
    work = deque([initial])
    visited = {initial}
    transition_count = 0

    while work:
        state = work.popleft()
        check_state(state)
        for transition, successor in successors(state):
            transition_count += 1
            check_transition(transition)
            check_state(successor)
            if successor not in visited:
                visited.add(successor)
                work.append(successor)

    print(
        "MODEL_IPC_BLOCKING: PASS "
        f"states={len(visited)} transitions={transition_count} "
        f"slots={SLOT_COUNT} depth={QUEUE_DEPTH} generation_max={MAX_GENERATION}"
    )
    print("MODEL_IPC_REFINEMENT_TO_CPP: UNPROVEN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
