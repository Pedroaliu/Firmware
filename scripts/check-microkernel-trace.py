#!/usr/bin/env python3

"""Independent checker for JIXIA_VERIFY_TRACE records emitted by QEMU."""

from __future__ import annotations

import argparse
import collections
import re
import sys
from dataclasses import dataclass
from pathlib import Path


TRACE_PREFIX = "JIXIA_VERIFY_TRACE: "
HART_PREFIX = "JIXIA_VERIFY_TRACE_HART: "

BEGIN_TO_TERMINALS = {
    "ipc_create_begin": {"ipc_create_publish", "ipc_create_reject"},
    "ipc_destroy_begin": {"ipc_destroy_dead", "ipc_destroy_reject"},
    "ipc_send_begin": {"ipc_send_enqueue", "ipc_send_wake", "ipc_send_reject"},
    "ipc_recv_begin": {"ipc_recv_dequeue", "ipc_recv_wait_enqueue", "ipc_recv_reject"},
    "runqueue_insert_begin": {"runqueue_insert_publish", "runqueue_insert_reject"},
    "runqueue_remove_begin": {"runqueue_remove_select", "runqueue_remove_empty"},
}

LINEARIZATION_LOCKSETS = {
    "ipc_create_publish": 0x3,
    "ipc_destroy_dead": 0x3,
    "ipc_destroy_wake": 0x3,
    "ipc_send_enqueue": 0x2,
    "ipc_send_wake": 0x2,
    "ipc_recv_dequeue": 0x2,
    "ipc_recv_wait_enqueue": 0x2,
    "runqueue_insert_publish": 0x8,
    "runqueue_remove_select": 0x8,
}

BEGIN_LOCKSETS = {
    "runqueue_remove_begin": 0x8,
}


@dataclass(frozen=True)
class Record:
    seq: int
    time: int
    hart: int
    op: int
    event: str
    lockset: int
    object: int
    subject: int
    arg0: int
    arg1: int


def parse_fields(text: str) -> dict[str, str]:
    fields: dict[str, str] = {}
    for token in text.strip().split():
        if "=" not in token:
            raise ValueError(f"malformed trace token: {token!r}")
        key, value = token.split("=", 1)
        fields[key] = value
    return fields


def parse_log(path: Path) -> tuple[list[Record], int]:
    records: list[Record] = []
    dropped = 0
    saw_begin = False
    saw_end = False

    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        if line.startswith("JIXIA_VERIFY_TRACE_BEGIN: "):
            saw_begin = True
        elif line.startswith("JIXIA_VERIFY_TRACE_END: "):
            saw_end = True
        elif line.startswith(TRACE_PREFIX):
            fields = parse_fields(line[len(TRACE_PREFIX) :])
            records.append(
                Record(
                    seq=int(fields["seq"], 0),
                    time=int(fields["time"], 0),
                    hart=int(fields["hart"], 0),
                    op=int(fields["op"], 0),
                    event=fields["event"],
                    lockset=int(fields["lockset"], 0),
                    object=int(fields["object"], 0),
                    subject=int(fields["subject"], 0),
                    arg0=int(fields["arg0"], 0),
                    arg1=int(fields["arg1"], 0),
                )
            )
        elif line.startswith(HART_PREFIX):
            fields = parse_fields(line[len(HART_PREFIX) :])
            dropped += int(fields["dropped"], 0)

    if not saw_begin or not saw_end:
        raise ValueError("trace begin/end marker missing")
    if not records:
        raise ValueError("trace contains no records")
    return records, dropped


def require(condition: bool, message: str) -> None:
    if not condition:
        raise AssertionError(message)


def check_sequence(records: list[Record], dropped: int) -> None:
    require(dropped == 0, f"trace overflowed: dropped={dropped}")
    sequences = [record.seq for record in records]
    require(len(sequences) == len(set(sequences)), "duplicate global sequence")
    require(sequences == list(range(sequences[0], sequences[-1] + 1)), "trace sequence gap")


def check_operations(records: list[Record]) -> None:
    begins: dict[int, str] = {}
    terminals: collections.defaultdict[int, list[str]] = collections.defaultdict(list)

    for record in records:
        if record.event in BEGIN_TO_TERMINALS:
            expected_begin_lockset = BEGIN_LOCKSETS.get(record.event, 0)
            require(
                record.lockset == expected_begin_lockset,
                f"wrong begin lockset for {record.event}: {record.lockset:#x}",
            )
            require(record.op not in begins, f"duplicate operation begin: op={record.op}")
            begins[record.op] = record.event
        elif record.event in {
            event for events in BEGIN_TO_TERMINALS.values() for event in events
        }:
            terminals[record.op].append(record.event)

        expected_lockset = LINEARIZATION_LOCKSETS.get(record.event)
        if expected_lockset is not None:
            require(
                record.lockset == expected_lockset,
                f"wrong lockset for {record.event}: {record.lockset:#x}",
            )

    for operation, begin_event in begins.items():
        observed = terminals[operation]
        require(len(observed) == 1, f"operation {operation} has terminals {observed}")
        require(
            observed[0] in BEGIN_TO_TERMINALS[begin_event],
            f"operation {operation}: {begin_event} -> {observed[0]}",
        )

    for operation in terminals:
        require(operation in begins, f"terminal without begin: op={operation}")


@dataclass
class EndpointState:
    messages: collections.deque[tuple[int, int]]
    waiters: collections.deque[tuple[int, int]]
    dead: bool = False


def check_ipc(records: list[Record], require_blocking: bool, require_cross_hart: bool) -> None:
    endpoints: dict[int, EndpointState] = {}
    waiting_tasks: set[int] = set()
    # tid -> (wake sequence, wake hart, operation, handle, result a0, sender a5)
    pending_wakes: dict[int, tuple[int, int, int, int, int, int]] = {}
    result_published: dict[int, int] = {}
    send_begins: dict[int, tuple[int, int]] = {}
    block_harts: set[int] = set()
    wake_harts: set[int] = set()
    cross_hart_wakes = 0

    for record in records:
        if record.event == "ipc_create_publish":
            require(record.object not in endpoints, f"handle published twice: {record.object:#x}")
            endpoints[record.object] = EndpointState(collections.deque(), collections.deque())
        elif record.event == "ipc_destroy_dead":
            require(record.object in endpoints, f"destroy of unpublished handle: {record.object:#x}")
            endpoint = endpoints[record.object]
            require(not endpoint.dead, f"handle destroyed twice: {record.object:#x}")
            endpoint.messages.clear()
            endpoint.dead = True
        elif record.event == "ipc_send_enqueue":
            require(record.object in endpoints, f"enqueue to unpublished handle: {record.object:#x}")
            endpoint = endpoints[record.object]
            require(not endpoint.dead, f"enqueue after destroy: {record.object:#x}")
            require(not endpoint.waiters, f"message enqueued while waiters exist: {record.object:#x}")
            endpoint.messages.append((record.subject, record.arg0))
            require(record.arg1 == len(endpoint.messages), f"enqueue count mismatch: {record}")
            require(len(endpoint.messages) <= 16, f"queue depth exceeded: {record.object:#x}")
        elif record.event == "ipc_send_begin":
            send_begins[record.op] = (record.subject, record.arg0)
        elif record.event == "ipc_recv_dequeue":
            require(record.object in endpoints, f"dequeue from unpublished handle: {record.object:#x}")
            endpoint = endpoints[record.object]
            require(not endpoint.dead, f"dequeue after destroy: {record.object:#x}")
            require(bool(endpoint.messages), f"dequeue from empty model queue: {record.object:#x}")
            require(not endpoint.waiters, f"dequeue while waiters exist: {record.object:#x}")
            expected = endpoint.messages.popleft()
            require(
                expected == (record.subject, record.arg0),
                f"FIFO mismatch: expected={expected} observed={(record.subject, record.arg0)}",
            )
            require(record.arg1 == len(endpoint.messages), f"dequeue count mismatch: {record}")
        elif record.event == "ipc_recv_wait_enqueue":
            require(record.object in endpoints, f"wait on unpublished handle: {record.object:#x}")
            endpoint = endpoints[record.object]
            require(not endpoint.dead, f"wait after destroy: {record.object:#x}")
            require(not endpoint.messages, f"receiver blocked with pending message: {record.object:#x}")
            require(record.subject not in waiting_tasks, f"task waits twice: tid={record.subject}")
            require(record.arg0 == ord("M"), f"waiter published in wrong state: {record}")
            endpoint.waiters.append((record.subject, record.hart))
            waiting_tasks.add(record.subject)
            block_harts.add(record.hart)
            require(record.arg1 == len(endpoint.waiters), f"waiter count mismatch: {record}")
        elif record.event in {"ipc_send_wake", "ipc_destroy_wake"}:
            require(record.object in endpoints, f"wake on unpublished handle: {record.object:#x}")
            endpoint = endpoints[record.object]
            if record.event == "ipc_send_wake":
                require(not endpoint.dead, f"send wake after destroy: {record.object:#x}")
                require(not endpoint.messages, f"send wake with pending message: {record.object:#x}")
                require(record.op in send_begins, f"send wake without send begin: op={record.op}")
                sender, first_word = send_begins[record.op]
                require(record.arg0 == first_word, f"send wake payload mismatch: {record}")
                result_a0 = 0
                result_sender = sender
            else:
                require(endpoint.dead, f"destroy wake before DEAD publication: {record.object:#x}")
                require(not endpoint.messages, f"destroy wake with pending message: {record.object:#x}")
                result_a0 = (1 << 64) - 43
                result_sender = 0
                require(record.arg0 == result_a0, f"destroy wake must return -EIDRM: {record}")
            require(bool(endpoint.waiters), f"wake from empty waiter FIFO: {record.object:#x}")
            expected_tid, block_hart = endpoint.waiters.popleft()
            require(expected_tid == record.subject, f"waiter FIFO mismatch: {record}")
            require(record.subject in waiting_tasks, f"waiter membership lost: tid={record.subject}")
            waiting_tasks.remove(record.subject)
            require(record.subject not in pending_wakes, f"task woken twice: tid={record.subject}")
            pending_wakes[record.subject] = (
                record.seq,
                record.hart,
                record.op,
                record.object,
                result_a0,
                result_sender,
            )
            wake_harts.add(record.hart)
            if block_hart != record.hart:
                cross_hart_wakes += 1
            require(record.arg1 == len(endpoint.waiters), f"wake count mismatch: {record}")
        elif record.event == "ipc_recv_result_publish":
            require(
                record.lockset in {0x2, 0x3},
                f"wrong lockset for receive result publication: {record.lockset:#x}",
            )
            require(record.subject in pending_wakes, f"result without wake: tid={record.subject}")
            wake_seq, _, wake_op, wake_handle, result_a0, result_sender = pending_wakes[
                record.subject
            ]
            require(wake_seq < record.seq, f"result precedes wake: tid={record.subject}")
            require(record.op == wake_op, f"result operation differs from wake: {record}")
            require(record.object == wake_handle, f"result handle differs from wake: {record}")
            require(record.arg0 == result_a0, f"receive result a0 mismatch: {record}")
            require(record.arg1 == result_sender, f"receive result sender mismatch: {record}")
            require(record.subject not in result_published, f"result published twice: tid={record.subject}")
            result_published[record.subject] = record.seq
        elif record.event == "runqueue_insert_begin" and record.subject in pending_wakes:
            require(record.arg0 == ord("M"), f"woken task lost blocked state before READY: {record}")
        elif record.event == "runqueue_insert_publish" and record.subject in pending_wakes:
            require(
                record.subject in result_published,
                f"task READY before receive result: tid={record.subject}",
            )
            require(
                result_published[record.subject] < record.seq,
                f"receive result does not precede READY: tid={record.subject}",
            )
            require(record.arg0 == ord("r"), f"woken task published in wrong state: {record}")
            del pending_wakes[record.subject]
            del result_published[record.subject]

    require(not pending_wakes, f"wake without READY publication: tids={sorted(pending_wakes)}")
    require(not result_published, f"orphan receive results: tids={sorted(result_published)}")
    if require_blocking:
        require(bool(block_harts), "blocking IPC trace contains no blocked receiver")
        require(bool(wake_harts), "blocking IPC trace contains no wake")
        require(not waiting_tasks, f"blocked receivers left behind: tids={sorted(waiting_tasks)}")
    if require_cross_hart:
        require(cross_hart_wakes > 0, "no cross-hart block/wake pair observed")


def check_runqueues(records: list[Record]) -> None:
    queues: collections.defaultdict[int, collections.deque[int]] = collections.defaultdict(
        collections.deque
    )
    task_queue: dict[int, int] = {}

    for record in records:
        if record.event == "runqueue_insert_publish":
            require(record.subject not in task_queue, f"task queued twice: tid={record.subject}")
            queue = queues[record.object]
            queue.append(record.subject)
            task_queue[record.subject] = record.object
            require(record.arg1 == len(queue), f"runqueue insert size mismatch: {record}")
        elif record.event == "runqueue_remove_select":
            queue = queues[record.object]
            require(bool(queue), f"remove from empty trace queue: {record.object:#x}")
            selected = queue.popleft()
            require(selected == record.subject, f"runqueue FIFO mismatch: {record}")
            require(
                task_queue.pop(selected, None) == record.object,
                f"task membership lost: {record}",
            )
            require(record.arg1 == len(queue), f"runqueue remove size mismatch: {record}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--expected-harts", type=int)
    parser.add_argument("--require-blocking-ipc", action="store_true")
    parser.add_argument("--require-cross-hart-wake", action="store_true")
    parser.add_argument("log", type=Path)
    args = parser.parse_args()

    records, dropped = parse_log(args.log)
    records.sort(key=lambda record: record.seq)
    check_sequence(records, dropped)
    check_operations(records)
    check_ipc(records, args.require_blocking_ipc, args.require_cross_hart_wake)
    check_runqueues(records)

    observed_harts = {record.hart for record in records}
    if args.expected_harts is not None:
        require(args.expected_harts > 0, "expected hart count must be positive")
        require(
            observed_harts == set(range(args.expected_harts)),
            f"hart participation mismatch: expected={list(range(args.expected_harts))} "
            f"observed={sorted(observed_harts)}",
        )

    print(
        "JIXIA_VERIFY_TRACE_CHECK: PASS "
        f"records={len(records)} harts={len(observed_harts)} first_seq={records[0].seq} "
        f"last_seq={records[-1].seq}"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (AssertionError, KeyError, ValueError) as error:
        print(f"JIXIA_VERIFY_TRACE_CHECK: FAIL: {error}", file=sys.stderr)
        raise SystemExit(1)
