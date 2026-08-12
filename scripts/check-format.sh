#!/usr/bin/env bash

set -euo pipefail

readonly ROOT_DIR="$(
    cd "$(dirname "${BASH_SOURCE[0]}")/.."
    pwd
)"

if ! command -v clang-format >/dev/null 2>&1; then
    echo "Missing command: clang-format" >&2
    echo "Run: bash scripts/setup-dev-env.sh" >&2
    exit 2
fi

if ! command -v python3 >/dev/null 2>&1; then
    echo "Missing command: python3" >&2
    exit 2
fi

python3 - "${ROOT_DIR}" "$@" <<'PY'
import argparse
import difflib
import os
import re
import subprocess
import sys
from collections import defaultdict

ROOT = sys.argv[1]
ARGS = sys.argv[2:]
os.chdir(ROOT)

SOURCE_SUFFIXES = (
    ".c", ".cc", ".cpp", ".cxx",
    ".h", ".hh", ".hpp", ".hxx",
)

parser = argparse.ArgumentParser(
    description="Check clang-format on changed C/C++ lines without reformatting legacy untouched code."
)
mode = parser.add_mutually_exclusive_group()
mode.add_argument("--cached", action="store_true", help="check staged changes")
mode.add_argument("--base", metavar="REF", help="check committed changes since REF")
mode.add_argument("--all", action="store_true", help="check every tracked C/C++ file")
parser.add_argument("--fix", action="store_true", help="format the selected lines in place")
args = parser.parse_args(ARGS)


def run(command, *, capture=False, check=True):
    return subprocess.run(
        command,
        text=True,
        stdout=subprocess.PIPE if capture else None,
        stderr=subprocess.PIPE if capture else None,
        check=check,
    )


def is_source(path):
    return path.endswith(SOURCE_SUFFIXES)


def tracked_sources():
    result = run(["git", "ls-files"], capture=True)
    return sorted(path for path in result.stdout.splitlines() if is_source(path))


def diff_text():
    command = [
        "git", "diff", "--unified=0", "--no-color", "--diff-filter=ACMR",
    ]

    if args.cached:
        command.append("--cached")
    elif args.base:
        command.extend([args.base, "HEAD"])
    else:
        # HEAD includes both staged and unstaged modifications to tracked files.
        command.append("HEAD")

    return run(command, capture=True).stdout


def parse_changed_ranges(patch):
    ranges = defaultdict(list)
    current = None
    hunk = re.compile(r"^@@ -\d+(?:,\d+)? \+(\d+)(?:,(\d+))? @@")

    for line in patch.splitlines():
        if line.startswith("+++ "):
            target = line[4:]
            if target == "/dev/null":
                current = None
            elif target.startswith("b/"):
                current = target[2:]
            else:
                current = target
            continue

        match = hunk.match(line)
        if match is None or current is None or not is_source(current):
            continue

        start = int(match.group(1))
        count = int(match.group(2) or "1")
        if count == 0:
            continue
        ranges[current].append((start, start + count - 1))

    return ranges


def merge_ranges(items):
    merged = []
    for start, end in sorted(items):
        if merged and start <= merged[-1][1] + 1:
            merged[-1] = (merged[-1][0], max(merged[-1][1], end))
        else:
            merged.append((start, end))
    return merged


def add_untracked_sources(ranges):
    result = run(
        ["git", "ls-files", "--others", "--exclude-standard"],
        capture=True,
    )
    for path in result.stdout.splitlines():
        if is_source(path):
            # None means check the whole new file.
            ranges[path] = None


def clang_format_command(path, ranges, *, write=False):
    command = ["clang-format", "--style=file"]
    if write:
        command.append("-i")
    if ranges is not None:
        for start, end in merge_ranges(ranges):
            command.append(f"--lines={start}:{end}")
    command.append(path)
    return command


def print_repair_diff(path, ranges):
    with open(path, "r", encoding="utf-8") as source:
        original = source.read()

    formatted = run(
        clang_format_command(path, ranges),
        capture=True,
        check=False,
    )
    if formatted.returncode != 0:
        return

    diff = difflib.unified_diff(
        original.splitlines(keepends=True),
        formatted.stdout.splitlines(keepends=True),
        fromfile=path,
        tofile=f"{path} (clang-format)",
    )
    text = "".join(diff)
    if text:
        print(text, file=sys.stderr, end="" if text.endswith("\n") else "\n")


def check_one(path, ranges):
    if args.fix:
        result = run(
            clang_format_command(path, ranges, write=True),
            capture=True,
            check=False,
        )
    else:
        command = clang_format_command(path, ranges)
        command[2:2] = ["--dry-run", "--Werror"]
        result = run(command, capture=True, check=False)

    if result.returncode != 0:
        if result.stdout:
            sys.stdout.write(result.stdout)
        if result.stderr:
            sys.stderr.write(result.stderr)
        if not args.fix:
            print_repair_diff(path, ranges)
        return False
    return True


if args.all:
    selected = {path: None for path in tracked_sources()}
else:
    selected = parse_changed_ranges(diff_text())
    if not args.cached and not args.base:
        add_untracked_sources(selected)

if not selected:
    print("clang-format: no relevant C/C++ changes")
    raise SystemExit(0)

failed = []
for path, ranges in sorted(selected.items()):
    if not os.path.isfile(path):
        continue
    if not check_one(path, ranges):
        failed.append(path)

if failed:
    print("clang-format: FAIL", file=sys.stderr)
    print("Files needing formatting:", file=sys.stderr)
    for path in failed:
        print(f"  {path}", file=sys.stderr)
    print(
        "Fix changed lines with: bash scripts/check-format.sh [same mode] --fix",
        file=sys.stderr,
    )
    raise SystemExit(1)

verb = "formatted" if args.fix else "checked"
print(f"clang-format: PASS ({verb} {len(selected)} file(s))")
PY
