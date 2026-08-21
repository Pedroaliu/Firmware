#!/usr/bin/env python3

"""Mutation tests proving that the offline trace checker fails closed."""

from __future__ import annotations

import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
CHECKER = ROOT / "scripts" / "check-microkernel-trace.py"


VALID_RECORDS = [
    "seq=1 time=1 hart=0 op=1 event=ipc_create_begin lockset=0 "
    "object=0 subject=1 arg0=0 arg1=0",
    "seq=2 time=2 hart=0 op=1 event=ipc_create_publish lockset=3 "
    "object=4294967296 subject=1 arg0=1 arg1=0",
    "seq=3 time=3 hart=1 op=3 event=ipc_send_begin lockset=0 "
    "object=4294967296 subject=2 arg0=17 arg1=0",
    "seq=4 time=4 hart=1 op=3 event=ipc_send_enqueue lockset=2 "
    "object=4294967296 subject=2 arg0=17 arg1=1",
    "seq=5 time=5 hart=0 op=5 event=ipc_recv_begin lockset=0 "
    "object=4294967296 subject=0 arg0=0 arg1=0",
    "seq=6 time=6 hart=0 op=5 event=ipc_recv_dequeue lockset=2 "
    "object=4294967296 subject=2 arg0=17 arg1=0",
    "seq=7 time=7 hart=0 op=7 event=runqueue_insert_begin lockset=0 "
    "object=170 subject=9 arg0=66 arg1=0",
    "seq=8 time=8 hart=0 op=7 event=runqueue_insert_publish lockset=8 "
    "object=170 subject=9 arg0=114 arg1=1",
    "seq=9 time=9 hart=1 op=9 event=runqueue_remove_begin lockset=8 "
    "object=170 subject=9 arg0=114 arg1=1",
    "seq=10 time=10 hart=1 op=9 event=runqueue_remove_select lockset=8 "
    "object=170 subject=9 arg0=114 arg1=0",
    "seq=11 time=11 hart=0 op=11 event=ipc_destroy_begin lockset=0 "
    "object=4294967296 subject=1 arg0=0 arg1=0",
    "seq=12 time=12 hart=0 op=11 event=ipc_destroy_dead lockset=3 "
    "object=4294967296 subject=1 arg0=2 arg1=0",
]


def render(records: list[str], dropped: int = 0) -> str:
    lines = ["JIXIA_VERIFY_TRACE_BEGIN: seed=1 records_per_hart=64"]
    lines.extend(f"JIXIA_VERIFY_TRACE: {record}" for record in records)
    lines.append(f"JIXIA_VERIFY_TRACE_HART: hart=0 records=12 dropped={dropped}")
    lines.append("JIXIA_VERIFY_TRACE_END: sequence=12")
    return "\n".join(lines) + "\n"


def run_case(
    name: str,
    contents: str,
    should_pass: bool,
    extra_args: list[str] | None = None,
) -> None:
    with tempfile.TemporaryDirectory(prefix="jixia-trace-check-") as directory:
        path = Path(directory) / f"{name}.log"
        path.write_text(contents, encoding="utf-8")
        result = subprocess.run(
            [sys.executable, str(CHECKER), *(extra_args or []), str(path)],
            check=False,
            capture_output=True,
            text=True,
        )

    observed_pass = result.returncode == 0
    if observed_pass != should_pass:
        raise AssertionError(
            f"case {name}: expected pass={should_pass}, rc={result.returncode}, "
            f"stdout={result.stdout!r}, stderr={result.stderr!r}"
        )


def replace(records: list[str], index: int, old: str, new: str) -> list[str]:
    mutated = records.copy()
    if old not in mutated[index]:
        raise AssertionError(f"mutation token {old!r} missing in record {index}")
    mutated[index] = mutated[index].replace(old, new, 1)
    return mutated


def main() -> int:
    run_case("valid", render(VALID_RECORDS), True, ["--expected-harts", "2"])
    run_case("hart-participation", render(VALID_RECORDS), False, ["--expected-harts", "3"])
    run_case("overflow", render(VALID_RECORDS, dropped=1), False)

    sequence_gap = replace(VALID_RECORDS, 5, "seq=6", "seq=60")
    run_case("sequence-gap", render(sequence_gap), False)

    fifo_mismatch = replace(VALID_RECORDS, 5, "arg0=17", "arg0=99")
    run_case("fifo-mismatch", render(fifo_mismatch), False)

    count_mismatch = replace(VALID_RECORDS, 3, "arg1=1", "arg1=2")
    run_case("count-mismatch", render(count_mismatch), False)

    wrong_lockset = replace(VALID_RECORDS, 3, "lockset=2", "lockset=0")
    run_case("wrong-lockset", render(wrong_lockset), False)

    duplicate_terminal = VALID_RECORDS.copy()
    duplicate_terminal[5] = duplicate_terminal[5].replace("op=5", "op=3", 1)
    run_case("duplicate-terminal", render(duplicate_terminal), False)

    print("JIXIA_VERIFY_TRACE_CHECKER_SELFTEST: PASS cases=8 mutations=7")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
