#!/usr/bin/env python3

"""Exhaustive safety model for the Jixia task/runqueue state contract."""

from __future__ import annotations

from collections import deque
from dataclasses import dataclass
from typing import Iterable


HART_COUNT = 2
TASK_COUNT = 3

READY = "ready"
RUNNING = "running"
BLOCKED = "blocked"
ENDED = "ended"
NO_TASK = -1


@dataclass(frozen=True)
class State:
    task_state: tuple[str, ...]
    current: tuple[int, ...]
    runqueue: tuple[int, ...]
    waiting: frozenset[int]


def replace_tuple(values: tuple, index: int, value) -> tuple:
    result = list(values)
    result[index] = value
    return tuple(result)


def check_state(state: State) -> None:
    assert len(state.task_state) == TASK_COUNT
    assert len(state.current) == HART_COUNT
    assert len(set(state.runqueue)) == len(state.runqueue)
    running = [task for task in state.current if task != NO_TASK]
    assert len(set(running)) == len(running)
    assert not (set(state.runqueue) & state.waiting)
    assert not (set(state.runqueue) & set(running))
    assert not (state.waiting & set(running))

    for task in range(TASK_COUNT):
        memberships = (
            state.runqueue.count(task)
            + running.count(task)
            + (1 if task in state.waiting else 0)
        )
        task_state = state.task_state[task]
        if task_state == ENDED:
            assert memberships == 0
        else:
            assert memberships == 1

        if task_state == READY:
            assert task in state.runqueue
        elif task_state == RUNNING:
            assert task in running
        elif task_state == BLOCKED:
            assert task in state.waiting
        else:
            assert task_state == ENDED


def successors(state: State) -> Iterable[State]:
    # Dispatch always removes the FIFO head before publishing RUNNING.
    if state.runqueue:
        selected = state.runqueue[0]
        for hart in range(HART_COUNT):
            if state.current[hart] == NO_TASK:
                yield State(
                    replace_tuple(state.task_state, selected, RUNNING),
                    replace_tuple(state.current, hart, selected),
                    state.runqueue[1:],
                    state.waiting,
                )

    for hart, task in enumerate(state.current):
        if task == NO_TASK:
            continue

        # Timer/yield publication: READY and FIFO membership appear in the
        # same abstract transition.
        yield State(
            replace_tuple(state.task_state, task, READY),
            replace_tuple(state.current, hart, NO_TASK),
            state.runqueue + (task,),
            state.waiting,
        )

        # A blocking operation removes the current identity and publishes its
        # wait membership in the same transition.
        yield State(
            replace_tuple(state.task_state, task, BLOCKED),
            replace_tuple(state.current, hart, NO_TASK),
            state.runqueue,
            state.waiting | {task},
        )

        yield State(
            replace_tuple(state.task_state, task, ENDED),
            replace_tuple(state.current, hart, NO_TASK),
            state.runqueue,
            state.waiting,
        )

    for task in state.waiting:
        yield State(
            replace_tuple(state.task_state, task, READY),
            state.current,
            state.runqueue + (task,),
            state.waiting - {task},
        )


def main() -> int:
    initial = State(
        task_state=tuple(READY for _ in range(TASK_COUNT)),
        current=tuple(NO_TASK for _ in range(HART_COUNT)),
        runqueue=tuple(range(TASK_COUNT)),
        waiting=frozenset(),
    )
    work = deque([initial])
    visited = {initial}
    transition_count = 0

    while work:
        state = work.popleft()
        check_state(state)
        for successor in successors(state):
            transition_count += 1
            check_state(successor)
            if successor not in visited:
                visited.add(successor)
                work.append(successor)

    print(
        "MODEL_SCHEDULER_MEMBERSHIP: PASS "
        f"states={len(visited)} transitions={transition_count} "
        f"harts={HART_COUNT} tasks={TASK_COUNT}"
    )
    print("MODEL_SCHEDULER_STARVATION_FREEDOM: UNPROVEN")
    print("MODEL_SCHEDULER_REFINEMENT_TO_CPP: UNPROVEN")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
