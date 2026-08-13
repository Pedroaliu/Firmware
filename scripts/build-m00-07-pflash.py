#!/usr/bin/env python3

"""Build the M00-07 QEMU virt pflash0 image.

The layout constants are parsed from boot/qemu_virt/pflash_layout.h so Stage0
assembly and the host image builder consume one source of truth.
"""

from __future__ import annotations

import argparse
import hashlib
import re
import struct
from pathlib import Path


_PAGE_SIZE = 4096

_REQUIRED_LAYOUT_CONSTANTS = (
    "JIXIA_QEMU_PFLASH_SIZE",
    "JIXIA_PFLASH_STAGE0_LIMIT",
    "JIXIA_PFLASH_HEADER_OFFSET",
    "JIXIA_PFLASH_HEADER_SIZE",
    "JIXIA_PFLASH_BASE_IMAGE_OFFSET",
    "JIXIA_PFLASH_EXTENDED_IMAGE_OFFSET",
    "JIXIA_PFLASH_HEADER_MAGIC",
    "JIXIA_PFLASH_HEADER_VERSION",
    "JIXIA_CONTAINED_BASE_ADDRESS",
    "JIXIA_CONTAINED_ENTRY_ADDRESS",
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


def pad_to_page(payload: bytes) -> bytes:
    if not payload:
        raise ValueError("Extended image is empty")

    padded_size = ((len(payload) + _PAGE_SIZE - 1) // _PAGE_SIZE) * _PAGE_SIZE
    return payload + (b"\xff" * (padded_size - len(payload)))


def build_image(
    stage0_path: Path,
    base_path: Path,
    output_path: Path,
    layout_path: Path,
    extended_path: Path | None,
) -> None:
    layout = parse_layout(layout_path)

    flash_size = layout["JIXIA_QEMU_PFLASH_SIZE"]
    stage0_limit = layout["JIXIA_PFLASH_STAGE0_LIMIT"]
    header_offset = layout["JIXIA_PFLASH_HEADER_OFFSET"]
    header_size = layout["JIXIA_PFLASH_HEADER_SIZE"]
    base_offset = layout["JIXIA_PFLASH_BASE_IMAGE_OFFSET"]
    extended_offset = layout["JIXIA_PFLASH_EXTENDED_IMAGE_OFFSET"]

    stage0 = stage0_path.read_bytes()
    base = base_path.read_bytes()
    extended = b"" if extended_path is None else pad_to_page(extended_path.read_bytes())

    if not stage0:
        raise ValueError("Stage0 image is empty")
    if len(stage0) > stage0_limit:
        raise ValueError(
            f"Stage0 is {len(stage0)} bytes, exceeds {stage0_limit} byte slot"
        )
    if not base:
        raise ValueError("Jixia Base image is empty")
    if header_size != 64:
        raise ValueError(f"v1 header must be 64 bytes, got {header_size}")
    if header_offset < stage0_limit:
        raise ValueError("flash header overlaps Stage0 slot")
    if base_offset < header_offset + header_size:
        raise ValueError("Base image overlaps flash header")
    if base_offset + len(base) > flash_size:
        raise ValueError("Base image does not fit in pflash0")

    if extended:
        if extended_offset < base_offset + len(base):
            raise ValueError("Extended image overlaps Base image")
        if extended_offset + len(extended) > flash_size:
            raise ValueError("Extended image does not fit in pflash0")
        header_extended_offset = extended_offset
        header_extended_size = len(extended)
    else:
        header_extended_offset = 0
        header_extended_size = 0

    header = struct.pack(
        "<QIIQQQQQQ",
        layout["JIXIA_PFLASH_HEADER_MAGIC"],
        layout["JIXIA_PFLASH_HEADER_VERSION"],
        header_size,
        base_offset,
        len(base),
        layout["JIXIA_CONTAINED_BASE_ADDRESS"],
        layout["JIXIA_CONTAINED_ENTRY_ADDRESS"],
        header_extended_offset,
        header_extended_size,
    )
    if len(header) != header_size:
        raise AssertionError("packed JixiaFlashHeader size mismatch")

    image = bytearray(b"\xff" * flash_size)
    image[0 : len(stage0)] = stage0
    image[header_offset : header_offset + header_size] = header
    image[base_offset : base_offset + len(base)] = base
    if extended:
        image[extended_offset : extended_offset + len(extended)] = extended

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(image)

    digest = hashlib.sha256(image).hexdigest()
    print(f"pflash={output_path}")
    print(f"pflash_size={len(image)}")
    print(f"stage0_size={len(stage0)}")
    print(f"header_offset=0x{header_offset:x}")
    print(f"base_offset=0x{base_offset:x}")
    print(f"base_size={len(base)}")
    print(f"base_load=0x{layout['JIXIA_CONTAINED_BASE_ADDRESS']:x}")
    print(f"base_entry=0x{layout['JIXIA_CONTAINED_ENTRY_ADDRESS']:x}")
    print(f"extended_offset=0x{header_extended_offset:x}")
    print(f"extended_size={header_extended_size}")
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
    args = parser.parse_args()

    build_image(
        args.stage0,
        args.base,
        args.output,
        args.layout_header,
        args.extended,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
