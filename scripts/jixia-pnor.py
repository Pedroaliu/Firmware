#!/usr/bin/env python3

from __future__ import annotations

import argparse
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from tools.pnor.ffs import build_image, inspect_image  # noqa: E402


def parse_input(value: str) -> tuple[str, Path]:
    key, separator, path = value.partition("=")
    if not separator or not key or not path:
        raise argparse.ArgumentTypeError("input must be KEY=PATH")
    return key, Path(path)


def command_pack(args: argparse.Namespace) -> int:
    sources = dict(args.input)
    result = build_image(args.manifest, args.output, sources)

    print(f"pnor={args.output}")
    print(f"pnor_size={result.image_size}")
    print(f"block_size=0x{result.block_size:x}")
    print(f"toc_offset=0x{result.toc_offset:x}")
    print(f"toc_size=0x{result.toc_size:x}")
    for partition in result.partitions:
        print(
            f"partition={partition.name} "
            f"offset=0x{partition.offset:x} "
            f"size=0x{partition.allocated_size:x} "
            f"actual={partition.actual_size}"
        )
    return 0


def command_info(args: argparse.Namespace) -> int:
    result = inspect_image(args.image, args.toc_offset)
    print("FFS v1")
    print(f"image={args.image}")
    print(f"image_size={result.image_size}")
    print(f"block_size=0x{result.block_size:x}")
    print(f"toc_offset=0x{result.toc_offset:x}")
    print(f"toc_size=0x{result.toc_size:x}")
    print("name             offset       allocated    actual       flags")
    for partition in result.partitions:
        print(
            f"{partition.name:<16} "
            f"0x{partition.offset:08x} "
            f"0x{partition.allocated_size:08x} "
            f"0x{partition.actual_size:08x} "
            f"misc=0x{partition.misc_flags:02x}"
        )
    return 0


def main() -> int:
    parser = argparse.ArgumentParser(description="Jixia PNOR/FFS image tool")
    subparsers = parser.add_subparsers(dest="command", required=True)

    pack = subparsers.add_parser("pack", help="build an FFS PNOR image")
    pack.add_argument("--manifest", type=Path, required=True)
    pack.add_argument("--output", type=Path, required=True)
    pack.add_argument("--input", action="append", type=parse_input, default=[])
    pack.set_defaults(func=command_pack)

    info = subparsers.add_parser("info", help="validate and print an FFS PNOR image")
    info.add_argument("--image", type=Path, required=True)
    info.add_argument("--toc-offset", type=lambda value: int(value, 0), default=0x1000)
    info.set_defaults(func=command_info)

    args = parser.parse_args()
    return int(args.func(args))


if __name__ == "__main__":
    raise SystemExit(main())
