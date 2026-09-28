from __future__ import annotations

import struct
import sys
import unittest
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))

from analyze_nvpresent import FATBIN_MAGIC, kernel_name, parse_fatbins  # noqa: E402


def elf(kernel: str, flags: int) -> bytes:
    payload = bytearray(0x80)
    payload[:4] = b"\x7fELF"
    struct.pack_into("<I", payload, 0x30, flags)
    payload[0x40 : 0x40 + len(kernel) + 1] = kernel.encode() + b"\0"
    return bytes(payload)


def entry(architecture: int, payload: bytes, kind: int = 2) -> bytes:
    header = bytearray(0x20)
    struct.pack_into("<H", header, 0, kind)
    struct.pack_into("<I", header, 4, len(header))
    struct.pack_into("<I", header, 8, len(payload))
    struct.pack_into("<I", header, 0x1C, architecture)
    result = bytes(header) + payload
    return result + b"\0" * ((-len(result)) & 7)


def fatbin(*entries: bytes) -> bytes:
    payload = b"".join(entries)
    header = bytearray(0x10)
    header[:4] = FATBIN_MAGIC
    struct.pack_into("<H", header, 6, len(header))
    struct.pack_into("<Q", header, 8, len(payload))
    return bytes(header) + payload


class AnalyzeNvPresentTests(unittest.TestCase):
    def test_kernel_name_prefers_fp8_variant(self) -> None:
        self.assertEqual(kernel_name(b".text.conv1_fp8\0conv1_fp8\0"), "conv1_fp8")

    def test_fatbin_inventory_separates_fp16_and_fp8(self) -> None:
        image = fatbin(
            entry(120, elf("conv1", 0x06007804)),
            entry(89, elf("conv1", 0x06005904)),
        ) + fatbin(
            entry(120, elf("attn1_fp8", 0x06007804)),
            entry(89, elf("attn1_fp8", 0x06005904)),
        )
        containers, cubins, ptx = parse_fatbins(image)
        self.assertEqual(len(containers), 2)
        self.assertEqual(len(cubins), 4)
        self.assertEqual(ptx, 0)
        sm89 = [c for c in cubins if c.architecture == 89]
        self.assertEqual([c.kernel for c in sm89], ["conv1", "attn1_fp8"])
        self.assertEqual([c.fp8 for c in sm89], [False, True])
        self.assertEqual({c.elf_flags for c in sm89}, {"0x06005904"})

    def test_ptx_is_counted_without_becoming_a_cubin(self) -> None:
        containers, cubins, ptx = parse_fatbins(fatbin(entry(89, b".version 8.0", kind=1)))
        self.assertEqual(len(containers), 1)
        self.assertEqual(cubins, [])
        self.assertEqual(ptx, 1)

    def test_truncated_container_is_rejected(self) -> None:
        image = fatbin(entry(89, elf("main_kernel", 0x06005904)))[:-3]
        containers, cubins, ptx = parse_fatbins(image)
        self.assertEqual(containers, [])
        self.assertEqual(cubins, [])
        self.assertEqual(ptx, 0)

    def test_partially_valid_container_does_not_leak_results(self) -> None:
        good = entry(89, elf("main_kernel", 0x06005904))
        bad = bytearray(0x20)
        struct.pack_into("<H", bad, 0, 2)
        struct.pack_into("<I", bad, 4, 0x20)
        struct.pack_into("<I", bad, 8, 0x1000)
        struct.pack_into("<I", bad, 0x1C, 89)
        image = fatbin(good, bytes(bad))
        containers, cubins, ptx = parse_fatbins(image)
        self.assertEqual(containers, [])
        self.assertEqual(cubins, [])
        self.assertEqual(ptx, 0)


if __name__ == "__main__":
    unittest.main()
