#!/usr/bin/env python3
"""Offline, read-only structural inspection of NVIDIA NvPresent64.dll.

The script intentionally uses only the Python standard library.  It does not
load the DLL and it never writes a modified NVIDIA image.  Its output is meant
to be committed as analysis evidence while the driver binary stays outside the
repository.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import struct
from dataclasses import asdict, dataclass
from pathlib import Path
from typing import Iterable


FATBIN_MAGIC = b"\x50\xed\x55\xba"
ELF_MAGIC = b"\x7fELF"
EXECUTE = 0x20000000
WRITE = 0x80000000

KNOWN_KERNELS = (
    "depth_to_space_fp8",
    "warp_coarse_kernel",
    "depth_to_space",
    "downscale_kernel",
    "conv_fused_fp8",
    "conv_proj1_fp8",
    "conv_proj2_fp8",
    "conv_out1_fp8",
    "conv_out2_fp8",
    "conv_out3_fp8",
    "conv_fused",
    "conv_proj1",
    "conv_proj2",
    "conv_out1",
    "conv_out2",
    "conv_out3",
    "main_kernel",
    "attn1_fp8",
    "attn2_fp8",
    "conv1_fp8",
    "conv2_fp8",
    "conv3_fp8",
    "conv4_fp8",
    "conv5_fp8",
    "conv6_fp8",
    "conv7_fp8",
    "conv8_fp8",
    "attn1",
    "attn2",
    "conv1",
    "conv2",
    "conv3",
    "conv4",
    "conv5",
    "conv6",
    "conv7",
    "conv8",
)


@dataclass(frozen=True)
class Section:
    name: str
    rva: int
    virtual_size: int
    raw_offset: int
    raw_size: int
    characteristics: int


@dataclass(frozen=True)
class Cubin:
    fatbin_index: int
    fatbin_file_offset: int
    entry_index: int
    architecture: int
    size: int
    kernel: str
    fp8: bool
    elf_flags: str | None


class PE64:
    def __init__(self, path: Path):
        self.path = path
        self.data = path.read_bytes()
        if len(self.data) < 0x40:
            raise ValueError("file is too small to be a PE image")
        self.pe_offset = struct.unpack_from("<I", self.data, 0x3C)[0]
        if self.data[self.pe_offset : self.pe_offset + 4] != b"PE\0\0":
            raise ValueError("not a PE image")
        count = struct.unpack_from("<H", self.data, self.pe_offset + 6)[0]
        optional_size = struct.unpack_from("<H", self.data, self.pe_offset + 20)[0]
        self.optional_offset = self.pe_offset + 24
        if struct.unpack_from("<H", self.data, self.optional_offset)[0] != 0x20B:
            raise ValueError("not a PE32+ image")
        table = self.optional_offset + optional_size
        self.sections: list[Section] = []
        for index in range(count):
            offset = table + 40 * index
            name = self.data[offset : offset + 8].rstrip(b"\0").decode("ascii", "replace")
            virtual_size, rva, raw_size, raw_offset = struct.unpack_from(
                "<IIII", self.data, offset + 8
            )
            characteristics = struct.unpack_from("<I", self.data, offset + 36)[0]
            self.sections.append(
                Section(name, rva, virtual_size, raw_offset, raw_size, characteristics)
            )

    def rva_to_offset(self, rva: int) -> int:
        for section in self.sections:
            span = max(section.virtual_size, section.raw_size)
            if section.rva <= rva < section.rva + span:
                return section.raw_offset + rva - section.rva
        raise ValueError(f"unmapped RVA 0x{rva:x}")

    def offset_to_rva(self, offset: int) -> int:
        for section in self.sections:
            if section.raw_offset <= offset < section.raw_offset + section.raw_size:
                return section.rva + offset - section.raw_offset
        raise ValueError(f"unmapped file offset 0x{offset:x}")

    def executable_ranges(self) -> Iterable[tuple[int, bytes]]:
        for section in self.sections:
            if section.characteristics & EXECUTE:
                yield section.rva, self.data[
                    section.raw_offset : section.raw_offset + section.raw_size
                ]

    def exports(self) -> dict[str, int]:
        export_rva = struct.unpack_from("<I", self.data, self.optional_offset + 112)[0]
        if not export_rva:
            return {}
        offset = self.rva_to_offset(export_rva)
        fields = struct.unpack_from("<IIHHIIIIIII", self.data, offset)
        _, _, _, _, _, _, function_count, name_count, eat_rva, names_rva, ordinals_rva = fields
        eat = self.rva_to_offset(eat_rva)
        names = self.rva_to_offset(names_rva)
        ordinals = self.rva_to_offset(ordinals_rva)
        result: dict[str, int] = {}
        for index in range(name_count):
            name_rva = struct.unpack_from("<I", self.data, names + index * 4)[0]
            name_offset = self.rva_to_offset(name_rva)
            end = self.data.index(0, name_offset)
            name = self.data[name_offset:end].decode("ascii", "replace")
            ordinal = struct.unpack_from("<H", self.data, ordinals + index * 2)[0]
            if ordinal >= function_count:
                raise ValueError(f"bad export ordinal for {name}")
            result[name] = struct.unpack_from("<I", self.data, eat + ordinal * 4)[0]
        return result


def all_offsets(blob: bytes, needle: bytes) -> Iterable[int]:
    cursor = 0
    while True:
        cursor = blob.find(needle, cursor)
        if cursor < 0:
            return
        yield cursor
        cursor += 1


def find_gate(pe: PE64) -> list[dict[str, int]]:
    """Find cmp [rcx+14h], 3 followed by SETGE SIL in one function tail."""
    matches: list[dict[str, int]] = []
    for section_rva, blob in pe.executable_ranges():
        for offset in all_offsets(blob, bytes.fromhex("83 79 14 03")):
            for delta in range(4, min(40, len(blob) - offset)):
                tail = blob[offset + delta : offset + delta + 4]
                if tail == bytes.fromhex("40 0f 9d c6"):
                    matches.append(
                        {
                            "cmp_instruction_rva": section_rva + offset,
                            "cmp_immediate_rva": section_rva + offset + 3,
                            "setge_rva": section_rva + offset + delta,
                        }
                    )
                    break
    return matches


def find_config(pe: PE64, init_rva: int | None) -> int | None:
    if init_rva is None:
        return None
    start = pe.rva_to_offset(init_rva)
    for relative in range(96):
        if pe.data[start + relative : start + relative + 3] != b"\x48\x8d\x0d":
            continue
        displacement = struct.unpack_from("<i", pe.data, start + relative + 3)[0]
        target = init_rva + relative + 7 + displacement
        for section in pe.sections:
            if (
                section.characteristics & WRITE
                and section.rva <= target
                and target + 0x12A6 <= section.rva + section.virtual_size
            ):
                return target
    return None


def find_unique_rva(pe: PE64, needle: bytes) -> int | None:
    matches: list[int] = []
    for section_rva, blob in pe.executable_ranges():
        matches.extend(section_rva + offset for offset in all_offsets(blob, needle))
    return matches[0] if len(matches) == 1 else None


def kernel_name(payload: bytes) -> str:
    for name in KNOWN_KERNELS:
        pattern = rb"(?<![A-Za-z0-9_])" + re.escape(name.encode()) + rb"(?![A-Za-z0-9_])"
        if re.search(pattern, payload):
            return name
    return "unknown"


def parse_fatbins(data: bytes) -> tuple[list[dict[str, object]], list[Cubin], int]:
    fatbins: list[dict[str, object]] = []
    cubins: list[Cubin] = []
    ptx_entries = 0
    cursor = 0
    while True:
        cursor = data.find(FATBIN_MAGIC, cursor)
        if cursor < 0:
            break
        if cursor + 16 > len(data):
            break
        header_size = struct.unpack_from("<H", data, cursor + 6)[0]
        payload_size = struct.unpack_from("<Q", data, cursor + 8)[0]
        end = cursor + header_size + payload_size
        if not (0x10 <= header_size <= 0x100 and end <= len(data)):
            cursor += 4
            continue

        fatbin_index = len(fatbins) + 1
        entry_offset = cursor + header_size
        entry_index = 0
        arches: list[int] = []
        names: list[str] = []
        container_cubins: list[Cubin] = []
        container_ptx_entries = 0
        valid = True
        while entry_offset < end:
            if entry_offset + 0x20 > end:
                valid = False
                break
            kind = struct.unpack_from("<H", data, entry_offset)[0]
            entry_header = struct.unpack_from("<I", data, entry_offset + 4)[0]
            data_size = struct.unpack_from("<I", data, entry_offset + 8)[0]
            architecture = struct.unpack_from("<I", data, entry_offset + 0x1C)[0]
            data_offset = entry_offset + entry_header
            if (
                not 0x20 <= entry_header <= 0x400
                or data_offset > end
                or data_size > end - data_offset
            ):
                valid = False
                break
            entry_index += 1
            payload = data[data_offset : data_offset + data_size]
            if kind == 1:
                container_ptx_entries += 1
            if kind == 2:
                name = kernel_name(payload)
                flags = (
                    f"0x{struct.unpack_from('<I', payload, 0x30)[0]:08x}"
                    if len(payload) >= 0x34 and payload.startswith(ELF_MAGIC)
                    else None
                )
                container_cubins.append(
                    Cubin(
                        fatbin_index,
                        cursor,
                        entry_index,
                        architecture,
                        data_size,
                        name,
                        name.endswith("_fp8"),
                        flags,
                    )
                )
                arches.append(architecture)
                names.append(name)
            next_offset = (data_offset + data_size + 7) & ~7
            if next_offset <= entry_offset or next_offset > end:
                valid = False
                break
            entry_offset = next_offset
        if valid and entry_offset == end:
            cubins.extend(container_cubins)
            ptx_entries += container_ptx_entries
            fatbins.append(
                {
                    "index": fatbin_index,
                    "file_offset": f"0x{cursor:x}",
                    "size": header_size + payload_size,
                    "architectures": arches,
                    "kernels": sorted(set(names)),
                }
            )
        cursor += 4
    return fatbins, cubins, ptx_entries


def find_ascii(data: bytes, pattern: bytes) -> str | None:
    match = re.search(pattern, data)
    return match.group().decode("ascii", "replace") if match else None


def inspect(path: Path) -> dict[str, object]:
    pe = PE64(path)
    exports = pe.exports()
    init_rva = exports.get("NVP_Init_D3D")
    config_rva = find_config(pe, init_rva)
    fatbins, cubins, ptx_entries = parse_fatbins(pe.data)
    architectures: dict[str, int] = {}
    for cubin in cubins:
        key = f"sm_{cubin.architecture}"
        architectures[key] = architectures.get(key, 0) + 1

    fp16_sm89 = [c for c in cubins if c.architecture == 89 and not c.fp8]
    fp8_sm89 = [c for c in cubins if c.architecture == 89 and c.fp8]
    flags = sorted({c.elf_flags for c in cubins if c.architecture == 89 and c.elf_flags})

    # Stable byte signatures in 617.14.  A missing or non-unique result is
    # reported rather than guessed.
    arch_getter_padding = find_unique_rva(
        pe, bytes.fromhex("cc cc cc cc 8b 41 14 c3 cc cc cc cc cc cc cc cc")
    )
    arch_getter = arch_getter_padding + 4 if arch_getter_padding is not None else None
    precision_selector_cmp = find_unique_rva(
        pe, bytes.fromhex("83 bd 50 04 00 00 03 7c")
    )
    cuda_resolver = find_unique_rva(
        pe,
        bytes.fromhex(
            "48 83 ec 28 45 33 c9 48 8d 15 22 00 00 00 45 33 c0 48 8d 0d"
        ),
    )
    config_formula = find_unique_rva(
        pe,
        bytes.fromhex(
            "85 6f 48 74 0a 40 38 77 4c 75 04 33 c0 eb 07 0f b6 87 e9 00 00 00"
        ),
    )

    return {
        "file": {
            "name": path.name,
            "size": len(pe.data),
            "sha256": hashlib.sha256(pe.data).hexdigest(),
            "driver_build_string": find_ascii(pe.data, rb"DVSReal r[^\x00\r\n]+"),
            "pdb_path": find_ascii(pe.data, rb"[A-Za-z]:\\[^\x00]+NvPresent64\.pdb"),
        },
        "exports": {name: f"0x{rva:x}" for name, rva in sorted(exports.items())},
        "initialization": {
            "nvp_init_d3d_rva": f"0x{init_rva:x}" if init_rva is not None else None,
            "config_rva": f"0x{config_rva:x}" if config_rva is not None else None,
            "gate_candidates": [
                {key: f"0x{value:x}" for key, value in match.items()}
                for match in find_gate(pe)
            ],
            "arch_getter_rva": f"0x{arch_getter:x}" if arch_getter is not None else None,
            "precision_selector_cmp_rva": (
                f"0x{precision_selector_cmp:x}" if precision_selector_cmp is not None else None
            ),
            "cuda_resolver_rva": f"0x{cuda_resolver:x}" if cuda_resolver is not None else None,
            "config_formula_rva": f"0x{config_formula:x}" if config_formula is not None else None,
        },
        "fatbins": {
            "count": len(fatbins),
            "ptx_entries": ptx_entries,
            "cubin_architectures": dict(sorted(architectures.items())),
            "sm89_fp16_kernels": len(fp16_sm89),
            "sm89_fp8_kernels": len(fp8_sm89),
            "sm89_elf_flags": flags,
            "containers": fatbins,
            "cubins": [asdict(cubin) for cubin in cubins],
        },
    }


def print_text(report: dict[str, object]) -> None:
    file_info = report["file"]
    init = report["initialization"]
    fatbins = report["fatbins"]
    print(f"file={file_info['name']}")
    print(f"size={file_info['size']}")
    print(f"sha256={file_info['sha256']}")
    print(f"driver_build={file_info['driver_build_string']}")
    print(f"NVP_Init_D3D={init['nvp_init_d3d_rva']}")
    print(f"config={init['config_rva']}")
    print(f"gate_candidates={len(init['gate_candidates'])}")
    for gate in init["gate_candidates"]:
        print(
            "  gate "
            f"cmp={gate['cmp_instruction_rva']} "
            f"imm={gate['cmp_immediate_rva']} setge={gate['setge_rva']}"
        )
    print(f"arch_getter={init['arch_getter_rva']}")
    print(f"precision_selector_cmp={init['precision_selector_cmp_rva']}")
    print(f"cuda_resolver={init['cuda_resolver_rva']}")
    print(f"config_formula={init['config_formula_rva']}")
    print(
        f"fatbins={fatbins['count']} ptx_entries={fatbins['ptx_entries']} "
        f"architectures={fatbins['cubin_architectures']}"
    )
    print(
        f"sm89_fp16={fatbins['sm89_fp16_kernels']} "
        f"sm89_fp8={fatbins['sm89_fp8_kernels']} "
        f"sm89_elf_flags={fatbins['sm89_elf_flags']}"
    )
    for container in fatbins["containers"]:
        print(
            f"  #{container['index']:02d} {container['file_offset']} "
            f"arches={container['architectures']} kernels={','.join(container['kernels'])}"
        )


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("dll", type=Path, help="path to NvPresent64.dll")
    parser.add_argument("--json", action="store_true", help="emit machine-readable JSON")
    parser.add_argument(
        "--output",
        type=Path,
        help="write the selected output to this file instead of stdout",
    )
    parser.add_argument(
        "--expect-sha256",
        help="fail unless the inspected DLL has this SHA-256 (case-insensitive)",
    )
    args = parser.parse_args()
    report = inspect(args.dll)
    actual = report["file"]["sha256"]
    if args.expect_sha256 and actual.lower() != args.expect_sha256.lower():
        raise SystemExit(f"SHA-256 mismatch: expected {args.expect_sha256}, got {actual}")
    if args.json:
        rendered = json.dumps(report, indent=2, sort_keys=False) + "\n"
        if args.output:
            args.output.parent.mkdir(parents=True, exist_ok=True)
            args.output.write_text(rendered, encoding="utf-8", newline="\n")
        else:
            print(rendered, end="")
    else:
        if args.output:
            raise SystemExit("--output currently requires --json")
        print_text(report)


if __name__ == "__main__":
    main()
