import os
import re
import struct
import sys

def lz4_decompress_block(src: bytes, dst_len: int) -> bytes:
    """Pure-Python LZ4 block decompressor with zero external dependencies."""
    dst = bytearray()
    i = 0
    src_len = len(src)
    while i < src_len and len(dst) < dst_len:
        token = src[i]
        i += 1
        lit_len = token >> 4
        if lit_len == 15:
            while i < src_len:
                s = src[i]
                i += 1
                lit_len += s
                if s != 255:
                    break
        dst.extend(src[i : i + lit_len])
        i += lit_len
        if len(dst) >= dst_len or i >= src_len:
            break
        offset = struct.unpack("<H", src[i : i + 2])[0]
        i += 2
        match_len = (token & 0x0F) + 4
        if match_len == 19:
            while i < src_len:
                s = src[i]
                i += 1
                match_len += s
                if s != 255:
                    break
        for _ in range(match_len):
            dst.append(dst[-offset])
    return bytes(dst)

def extract_entry_name(ptx_bytes: bytes) -> str:
    """Finds the .entry function name inside the PTX text."""
    try:
        text = ptx_bytes[:4096].decode("utf-8", errors="ignore")
        match = re.search(r"\.entry\s+([A-Za-z0-9_]+)", text)
        if match:
            return match.group(1)
    except Exception:
        pass
    return "unknown"

def dump_kernels(dll_path: str, output_dir: str):
    if not os.path.isfile(dll_path):
        print(f"Error: File not found: {dll_path}")
        return

    print(f"Scanning: {dll_path}")
    with open(dll_path, "rb") as f:
        data = f.read()

    os.makedirs(output_dir, exist_ok=True)
    magic = b"\x50\xED\x55\xBA"  # 0xBA55ED50
    pos = 0
    fatbin_idx = 0
    dumped_count = 0

    while True:
        pos = data.find(magic, pos)
        if pos == -1:
            break

        declared_size = struct.unpack("<Q", data[pos + 8 : pos + 16])[0]
        curr = pos + 16
        end = pos + 16 + declared_size

        while curr + 64 <= end and curr + 64 <= len(data):
            kind = struct.unpack("<H", data[curr : curr + 2])[0]
            hdr = struct.unpack("<I", data[curr + 4 : curr + 8])[0]
            payload = struct.unpack("<Q", data[curr + 8 : curr + 16])[0]
            comp_size = struct.unpack("<I", data[curr + 16 : curr + 20])[0]
            arch = struct.unpack("<I", data[curr + 28 : curr + 32])[0]
            raw_size = struct.unpack("<Q", data[curr + 56 : curr + 64])[0]

            if kind == 1 and raw_size > 0:  # kind == 1 is PTX
                comp_data = data[curr + hdr : curr + hdr + comp_size]
                try:
                    ptx_bytes = lz4_decompress_block(comp_data, raw_size)
                    entry_name = extract_entry_name(ptx_bytes)
                    arch_label = f"sm_{arch}"
                    if arch == 89:
                        arch_label = "sm89_Ada"
                    elif arch == 120:
                        arch_label = "sm120_Blackwell"

                    file_name = f"{dumped_count:03d}_{arch_label}_{entry_name}.ptx"
                    out_path = os.path.join(output_dir, file_name)
                    with open(out_path, "wb") as out_f:
                        out_f.write(ptx_bytes)

                    print(f"[{dumped_count:03d}] {arch_label.ljust(16)} -> {entry_name} ({len(ptx_bytes)} bytes)")
                    dumped_count += 1
                except Exception as e:
                    print(f"Error decompressing PTX at {hex(curr)}: {e}")

            curr += hdr + payload

        fatbin_idx += 1
        pos += 4

    print(f"\nSuccessfully dumped {dumped_count} PTX kernels to: {output_dir}")

if __name__ == "__main__":
    target_dll = (
        sys.argv[1]
        if len(sys.argv) > 1
        else r"d:\Coding\DLSSGUnlock\DLSS DLL 310.9.0 + Streamline 2.14\nvngx_dlssg.dll"
    )
    out_dir = sys.argv[2] if len(sys.argv) > 2 else r"d:\Coding\DLSSGUnlock\kernel_dumps"
    dump_kernels(target_dll, out_dir)
