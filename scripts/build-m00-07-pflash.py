#!/usr/bin/env python3

"""Build the M00-07 QEMU pflash image through the generic Jixia FFS packer."""

from __future__ import annotations

import argparse
import hashlib
import re
import sys
from pathlib import Path

ROOT_DIR = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT_DIR))

from tools.pnor.ffs import build_image, inspect_image  # noqa: E402

_REQUIRED_LAYOUT_CONSTANTS = (
    "JIXIA_QEMU_PFLASH_SIZE",
    "JIXIA_PFLASH_STAGE0_LIMIT",
    "JIXIA_PFLASH_TOC_OFFSET",
    "JIXIA_PFLASH_TOC_SIZE",
    "JIXIA_PFLASH_DATA_OFFSET",
    "JIXIA_PFLASH_BLOCK_SIZE",
    "JIXIA_PFLASH_BLOCK_COUNT",
)


def parse_layout(path: Path) -> dict[str, int]:
    text = path.read_text(encoding="utf-8")
    values: dict[str, int] = {}

    for name in _REQUIRED_LAYOUT_CONSTANTS:
        match = re.search(
            rf"^\s*#define\s+{re.escape(name)}\s+([^\s/]+)",
            text,
            flags=re.MULTILINE,
        )
        if match is None:
            raise ValueError(f"missing layout constant {name} in {path}")
        values[name] = int(match.group(1), 0)

    return values


def build_image_compat(
    stage0_path: Path,
    base_path: Path,
    output_path: Path,
    layout_path: Path,
    manifest_path: Path,
    extended_path: Path | None,
) -> None:
    layout = parse_layout(layout_path)
    if stage0_path.stat().st_size > layout["JIXIA_PFLASH_STAGE0_LIMIT"]:
        raise ValueError("Stage0 image exceeds fixed XIP bootstrap slot")

    sources = {
        "stage0": stage0_path,
        "base": base_path,
    }
    if extended_path is not None:
        sources["extended"] = extended_path

    result = build_image(manifest_path, output_path, sources)

    expected_geometry = {
        "size": layout["JIXIA_QEMU_PFLASH_SIZE"],
        "block_size": layout["JIXIA_PFLASH_BLOCK_SIZE"],
        "toc_offset": layout["JIXIA_PFLASH_TOC_OFFSET"],
        "toc_size": layout["JIXIA_PFLASH_TOC_SIZE"],
    }
    actual_geometry = {
        "size": result.image_size,
        "block_size": result.block_size,
        "toc_offset": result.toc_offset,
        "toc_size": result.toc_size,
    }
    if actual_geometry != expected_geometry:
        raise ValueError(
            f"PNOR manifest/platform layout drift: {actual_geometry} != {expected_geometry}"
        )
    if result.image_size // result.block_size != layout["JIXIA_PFLASH_BLOCK_COUNT"]:
        raise ValueError("PNOR block-count mismatch")

    inspected = inspect_image(output_path, result.toc_offset)
    base = inspected.partition("JXBASE")
    extended = inspected.partition("JXEXT")
    if base is None or base.actual_size == 0:
        raise ValueError("generated FFS image has no JXBASE partition")

    image = output_path.read_bytes()
    digest = hashlib.sha256(image).hexdigest()

    print(f"pflash={output_path}")
    print(f"pflash_size={len(image)}")
    print(f"stage0_size={stage0_path.stat().st_size}")
    print(f"ffs_toc_offset=0x{result.toc_offset:x}")
    print(f"ffs_toc_size=0x{result.toc_size:x}")
    print(f"base_offset=0x{base.offset:x}")
    print(f"base_size={base.actual_size}")
    print("base_load=0x80000000")
    print("base_entry=0x80000000")
    print(f"extended_offset=0x{0 if extended is None else extended.offset:x}")
    print(f"extended_size={0 if extended is None else extended.actual_size}")
    for partition in inspected.partitions:
        print(
            f"partition={partition.name} "
            f"offset=0x{partition.offset:x} "
            f"size=0x{partition.allocated_size:x} "
            f"actual={partition.actual_size}"
        )
    print(f"sha256={digest}")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--stage0", type=Path, required=True)
    parser.add_argument("--base", type=Path, required=True)
    parser.add_argument("--extended", type=Path)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--layout-header",
        type=Path,
        default=Path("boot/qemu_virt/pflash_layout.h"),
    )
    parser.add_argument(
        "--manifest",
        type=Path,
        default=ROOT_DIR / "pnor/qemu_virt.toml",
    )
    args = parser.parse_args()

    build_image_compat(
        args.stage0,
        args.base,
        args.output,
        args.layout_header,
        args.manifest,
        args.extended,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
