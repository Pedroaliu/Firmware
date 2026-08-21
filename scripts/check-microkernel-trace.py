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
    "ipc_send_begin": {"ipc_send_enqueue", "ipc_send_reject"},
    "ipc_recv_begin": {"ipc_recv_dequeue", "ipc_recv_reject"},
    "runqueue_insert_begin": {"runqueue_insert_publish", "runqueue_insert_reject"},
    "runqueue_remove_begin": {"runqueue_remove_select", "runqueue_remove_empty"},
}

LINEARIZATION_LOCKSETS = {
    "ipc_create_publish": 0x3,
    "ipc_destroy_dead": 0x3,
    "ipc_send_enqueue": 0x2,
    "ipc_recv_dequeue": 0x2,
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


def check_ipc_fifo(records: list[Record]) -> None:
    queues: dict[int, collections.deque[tuple[int, int]]] = {}
    dead_handles: set[int] = set()

    for record in records:
        if record.event == "ipc_create_publish":
            require(record.object not in queues, f"handle published twice: {record.object:#x}")
            queues[record.object] = collections.deque()
        elif record.event == "ipc_destroy_dead":
            require(record.object in queues, f"destroy of unpublished handle: {record.object:#x}")
            queues[record.object].clear()
            dead_handles.add(record.object)
        elif record.event == "ipc_send_enqueue":
            require(record.object in queues, f"enqueue to unpublished handle: {record.object:#x}")
            require(record.object not in dead_handles, f"enqueue after destroy: {record.object:#x}")
            queue = queues[record.object]
            queue.append((record.subject, record.arg0))
            require(record.arg1 == len(queue), f"enqueue count mismatch: {record}")
            require(len(queue) <= 16, f"queue depth exceeded: {record.object:#x}")
        elif record.event == "ipc_recv_dequeue":
            require(record.object in queues, f"dequeue from unpublished handle: {record.object:#x}")
            require(record.object not in dead_handles, f"dequeue after destroy: {record.object:#x}")
            queue = queues[record.object]
            require(bool(queue), f"dequeue from empty model queue: {record.object:#x}")
            expected = queue.popleft()
            require(
                expected == (record.subject, record.arg0),
                f"FIFO mismatch: expected={expected} observed={(record.subject, record.arg0)}",
            )
            require(record.arg1 == len(queue), f"dequeue count mismatch: {record}")


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
    parser.add_argument("log", type=Path)
    args = parser.parse_args()

    records, dropped = parse_log(args.log)
    records.sort(key=lambda record: record.seq)
    check_sequence(records, dropped)
    check_operations(records)
    check_ipc_fifo(records)
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
