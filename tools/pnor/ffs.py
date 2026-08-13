#!/usr/bin/env python3

"""OpenPOWER-compatible FFS v1 PNOR image helpers for Jixia.

FFS here means FSP Flash Structure: a block-oriented firmware partition table,
not a POSIX filesystem. The on-flash header and entries are big-endian and use
XOR checksums compatible with OpenPOWER libffs.
"""

from __future__ import annotations

import struct
import tomllib
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping

FFS_MAGIC = 0x50415254
FFS_VERSION = 1
FFS_HEADER_SIZE = 48
FFS_ENTRY_SIZE = 128
FFS_PART_NAME_MAX = 15
FFS_PID_TOPLEVEL = 0xFFFFFFFF
FFS_TYPE_DATA = 1
FFS_TYPE_PARTITION = 3

FFS_INTEG_ECC = 0x8000
FFS_VERCHECK_SHA512V = 0x80
FFS_VERCHECK_SHA512EC = 0x40

FFS_MISCFLAGS = {
    "preserved": 0x80,
    "readonly": 0x40,
    "backup": 0x20,
    "reprovision": 0x10,
    "volatile": 0x08,
    "clear_ecc": 0x04,
    "golden": 0x01,
}


@dataclass(frozen=True)
class Partition:
    name: str
    offset: int
    allocated_size: int
    actual_size: int
    source_key: str | None
    payload: bytes
    flags: int = 0
    data_integrity: int = 0
    version_check: int = 0
    misc_flags: int = 0
    partition_type: int = FFS_TYPE_DATA


@dataclass(frozen=True)
class ImageResult:
    image_size: int
    block_size: int
    toc_offset: int
    toc_size: int
    partitions: tuple[Partition, ...]

    def partition(self, name: str) -> Partition | None:
        for partition in self.partitions:
            if partition.name == name:
                return partition
        return None


def _align_up(value: int, alignment: int) -> int:
    if alignment <= 0 or (alignment & (alignment - 1)) != 0:
        raise ValueError(f"alignment must be a power of two, got {alignment:#x}")
    return (value + alignment - 1) & ~(alignment - 1)


def _xor_words(data: bytes | bytearray) -> int:
    if len(data) % 4 != 0:
        raise ValueError("FFS checksum input must be a multiple of four bytes")

    checksum = 0
    for (word,) in struct.iter_unpack(">I", data):
        checksum ^= word
    return checksum


def _name_bytes(name: str) -> bytes:
    encoded = name.encode("ascii")
    if not encoded or len(encoded) > FFS_PART_NAME_MAX:
        raise ValueError(f"FFS partition name must be 1..{FFS_PART_NAME_MAX} ASCII bytes: {name!r}")
    return encoded + b"\x00" * (FFS_PART_NAME_MAX + 1 - len(encoded))


def _entry_bytes(partition: Partition, block_size: int, entry_id: int) -> bytes:
    if partition.offset % block_size != 0 or partition.allocated_size % block_size != 0:
        raise ValueError(f"partition {partition.name} is not block aligned")
    if partition.actual_size > partition.allocated_size:
        raise ValueError(f"partition {partition.name} actual size exceeds allocation")

    entry = bytearray(FFS_ENTRY_SIZE)
    entry[0:16] = _name_bytes(partition.name)

    struct.pack_into(
        ">7I",
        entry,
        16,
        partition.offset // block_size,
        partition.allocated_size // block_size,
        FFS_PID_TOPLEVEL,
        entry_id,
        partition.partition_type,
        partition.flags,
        partition.actual_size,
    )

    entry[60] = 0
    entry[61] = 0
    struct.pack_into(">H", entry, 62, partition.data_integrity)
    entry[64] = partition.version_check
    entry[65] = partition.misc_flags

    checksum = _xor_words(entry)
    struct.pack_into(">I", entry, 124, checksum)
    if _xor_words(entry) != 0:
        raise AssertionError("FFS entry checksum construction failed")
    return bytes(entry)


def _header_bytes(
    *,
    toc_size: int,
    entry_count: int,
    block_size: int,
    block_count: int,
) -> bytes:
    header = bytearray(FFS_HEADER_SIZE)
    struct.pack_into(
        ">7I",
        header,
        0,
        FFS_MAGIC,
        FFS_VERSION,
        toc_size // block_size,
        FFS_ENTRY_SIZE,
        entry_count,
        block_size,
        block_count,
    )
    checksum = _xor_words(header)
    struct.pack_into(">I", header, 44, checksum)
    if _xor_words(header) != 0:
        raise AssertionError("FFS header checksum construction failed")
    return bytes(header)


def _misc_flags(values: list[str]) -> int:
    result = 0
    for value in values:
        try:
            result |= FFS_MISCFLAGS[value]
        except KeyError as exc:
            raise ValueError(f"unknown FFS misc flag {value!r}") from exc
    return result


def _payload_for(spec: Mapping[str, object], sources: Mapping[str, Path], fill_byte: int) -> tuple[bytes, str | None]:
    source_key = spec.get("source")
    if source_key is None:
        return b"", None
    if not isinstance(source_key, str):
        raise ValueError("partition source must be a string")

    source = sources.get(source_key)
    if source is None:
        if bool(spec.get("optional", False)):
            return b"", source_key
        raise ValueError(f"missing required partition input {source_key!r}")

    payload = source.read_bytes()
    if not payload:
        raise ValueError(f"partition input {source} is empty")

    pad_to = int(spec.get("pad_to", 1))
    if pad_to > 1:
        padded_size = _align_up(len(payload), pad_to)
        payload += bytes([fill_byte]) * (padded_size - len(payload))
    return payload, source_key


def build_image(manifest_path: Path, output_path: Path, sources: Mapping[str, Path]) -> ImageResult:
    manifest = tomllib.loads(manifest_path.read_text(encoding="utf-8"))
    image_spec = manifest["image"]

    image_size = int(image_spec["size"])
    block_size = int(image_spec["block_size"])
    toc_offset = int(image_spec["toc_offset"])
    toc_size = int(image_spec["toc_size"])
    data_offset = int(image_spec["data_offset"])
    fill_byte = int(image_spec.get("fill", 0xFF))

    if image_size <= 0 or image_size % block_size != 0:
        raise ValueError("image size must be a positive multiple of block size")
    if toc_offset % block_size != 0 or toc_size % block_size != 0:
        raise ValueError("TOC offset and size must be block aligned")
    if toc_offset + toc_size > image_size:
        raise ValueError("TOC exceeds image")
    if data_offset < toc_offset + toc_size or data_offset % block_size != 0:
        raise ValueError("data_offset must be block aligned and follow the TOC")
    if not 0 <= fill_byte <= 0xFF:
        raise ValueError("fill must fit in one byte")

    partitions: list[Partition] = []
    intervals: list[tuple[int, int, str]] = [(toc_offset, toc_offset + toc_size, "part")]
    cursor = data_offset

    for raw_spec in manifest.get("partition", []):
        spec = dict(raw_spec)
        name = str(spec["name"])
        payload, source_key = _payload_for(spec, sources, fill_byte)
        if source_key is not None and source_key not in sources and bool(spec.get("optional", False)):
            continue

        alignment = int(spec.get("align", block_size))
        offset_value = spec.get("offset")
        if offset_value is None:
            offset = _align_up(cursor, alignment)
        else:
            offset = int(offset_value)

        if offset % block_size != 0:
            raise ValueError(f"partition {name} offset must be block aligned")

        requested_size = spec.get("size")
        minimum_size = max(len(payload), 1)
        if requested_size is None:
            allocated_size = _align_up(minimum_size, block_size)
        else:
            allocated_size = int(requested_size)
            if allocated_size % block_size != 0 or allocated_size < len(payload):
                raise ValueError(f"partition {name} has invalid reserved size")

        end = offset + allocated_size
        if end <= offset or end > image_size:
            raise ValueError(f"partition {name} exceeds PNOR image")

        for other_start, other_end, other_name in intervals:
            if offset < other_end and other_start < end:
                raise ValueError(f"partition {name} overlaps {other_name}")

        misc_flags = _misc_flags(list(spec.get("misc_flags", [])))
        data_integrity = FFS_INTEG_ECC if bool(spec.get("ecc", False)) else 0
        version_check = 0
        if bool(spec.get("sha512_version", False)):
            version_check |= FFS_VERCHECK_SHA512V
        if bool(spec.get("sha512_per_ec", False)):
            version_check |= FFS_VERCHECK_SHA512EC

        partition = Partition(
            name=name,
            offset=offset,
            allocated_size=allocated_size,
            actual_size=len(payload),
            source_key=source_key,
            payload=payload,
            misc_flags=misc_flags,
            data_integrity=data_integrity,
            version_check=version_check,
        )
        partitions.append(partition)
        intervals.append((offset, end, name))
        cursor = max(cursor, end)

    toc_actual_size = FFS_HEADER_SIZE + (len(partitions) + 1) * FFS_ENTRY_SIZE
    if toc_actual_size > toc_size:
        raise ValueError("FFS entries exceed reserved TOC partition")

    toc_partition = Partition(
        name="part",
        offset=toc_offset,
        allocated_size=toc_size,
        actual_size=toc_actual_size,
        source_key=None,
        payload=b"",
        misc_flags=FFS_MISCFLAGS["readonly"],
        partition_type=FFS_TYPE_PARTITION,
    )
    all_partitions = [toc_partition, *partitions]

    header = _header_bytes(
        toc_size=toc_size,
        entry_count=len(all_partitions),
        block_size=block_size,
        block_count=image_size // block_size,
    )
    entries = b"".join(
        _entry_bytes(partition, block_size, index + 1)
        for index, partition in enumerate(all_partitions)
    )

    image = bytearray(bytes([fill_byte]) * image_size)
    image[toc_offset : toc_offset + len(header)] = header
    image[toc_offset + len(header) : toc_offset + len(header) + len(entries)] = entries

    for partition in partitions:
        image[partition.offset : partition.offset + len(partition.payload)] = partition.payload

    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(image)

    return ImageResult(
        image_size=image_size,
        block_size=block_size,
        toc_offset=toc_offset,
        toc_size=toc_size,
        partitions=tuple(all_partitions),
    )


def inspect_image(image_path: Path, toc_offset: int) -> ImageResult:
    image = image_path.read_bytes()
    if toc_offset < 0 or toc_offset + FFS_HEADER_SIZE > len(image):
        raise ValueError("TOC offset is outside image")

    header = image[toc_offset : toc_offset + FFS_HEADER_SIZE]
    if _xor_words(header) != 0:
        raise ValueError("bad FFS header checksum")

    magic, version, table_blocks, entry_size, entry_count, block_size, block_count = struct.unpack_from(
        ">7I", header, 0
    )
    if magic != FFS_MAGIC or version != FFS_VERSION or entry_size != FFS_ENTRY_SIZE:
        raise ValueError("unsupported FFS header")
    if block_count * block_size != len(image):
        raise ValueError("FFS geometry does not match image size")

    toc_size = table_blocks * block_size
    if FFS_HEADER_SIZE + entry_count * FFS_ENTRY_SIZE > toc_size:
        raise ValueError("FFS entry array exceeds TOC")

    partitions: list[Partition] = []
    for index in range(entry_count):
        start = toc_offset + FFS_HEADER_SIZE + index * FFS_ENTRY_SIZE
        entry = image[start : start + FFS_ENTRY_SIZE]
        if len(entry) != FFS_ENTRY_SIZE or _xor_words(entry) != 0:
            raise ValueError(f"bad FFS checksum for entry {index}")

        name = entry[0:16].split(b"\x00", 1)[0].decode("ascii")
        base, size, _pid, _id, partition_type, flags, actual = struct.unpack_from(">7I", entry, 16)
        data_integrity = struct.unpack_from(">H", entry, 62)[0]
        version_check = entry[64]
        misc_flags = entry[65]

        offset = base * block_size
        allocated_size = size * block_size
        if offset + allocated_size > len(image) or actual > allocated_size:
            raise ValueError(f"invalid FFS range for {name}")

        partitions.append(
            Partition(
                name=name,
                offset=offset,
                allocated_size=allocated_size,
                actual_size=actual,
                source_key=None,
                payload=b"",
                flags=flags,
                data_integrity=data_integrity,
                version_check=version_check,
                misc_flags=misc_flags,
                partition_type=partition_type,
            )
        )

    return ImageResult(
        image_size=len(image),
        block_size=block_size,
        toc_offset=toc_offset,
        toc_size=toc_size,
        partitions=tuple(partitions),
    )
