#!/usr/bin/env python3
"""只读的 Nintendo Switch NSO 段提取器。

本工具只提取段而不应用重定位，用于离线检查用户提供的 ExeFS，并保持原始 NSO
文件不变。
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path


def u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def lz4_block_decode(src: bytes, expected_size: int) -> bytes:
    """解码 NSO 段使用的原始 LZ4 块格式。"""
    out = bytearray()
    pos = 0
    while pos < len(src):
        token = src[pos]
        pos += 1

        literal_length = token >> 4
        if literal_length == 15:
            while True:
                if pos >= len(src):
                    raise ValueError("LZ4 字面量长度超出输入范围")
                extra = src[pos]
                pos += 1
                literal_length += extra
                if extra != 255:
                    break

        end = pos + literal_length
        if end > len(src):
            raise ValueError("LZ4 字面量超出输入范围")
        out.extend(src[pos:end])
        pos = end

        # 最后一段序列可能只包含字面量。
        if pos == len(src):
            break
        if pos + 2 > len(src):
            raise ValueError("LZ4 匹配偏移被截断")
        match_offset = src[pos] | (src[pos + 1] << 8)
        pos += 2
        if match_offset == 0 or match_offset > len(out):
            raise ValueError("LZ4 匹配偏移无效")

        match_length = (token & 0x0F) + 4
        if (token & 0x0F) == 15:
            while True:
                if pos >= len(src):
                    raise ValueError("LZ4 匹配长度超出输入范围")
                extra = src[pos]
                pos += 1
                match_length += extra
                if extra != 255:
                    break

        match_start = len(out) - match_offset
        for i in range(match_length):
            out.append(out[match_start + i])

    if len(out) != expected_size:
        raise ValueError(
            f"LZ4 尺寸不匹配：解码得到 {len(out)} 字节，期望 {expected_size} 字节"
        )
    return bytes(out)


def segment(data: bytes, file_offset: int, compressed_size: int,
            decompressed_size: int, compressed: bool) -> bytes:
    raw = data[file_offset:file_offset + compressed_size]
    if len(raw) != compressed_size:
        raise ValueError("NSO 段超出文件范围")
    if compressed:
        return lz4_block_decode(raw, decompressed_size)
    if len(raw) != decompressed_size:
        raise ValueError("未压缩 NSO 段尺寸不匹配")
    return raw


def extract(input_path: Path, output_dir: Path) -> dict:
    data = input_path.read_bytes()
    if data[:4] != b"NSO0":
        raise ValueError(f"{input_path} 不是 NSO0 文件")
    flags = u32(data, 0x0C)
    segments = {
        "text": (u32(data, 0x10), u32(data, 0x14), u32(data, 0x18), 0x01, 0x60),
        "rodata": (u32(data, 0x20), u32(data, 0x24), u32(data, 0x28), 0x02, 0x64),
        "data": (u32(data, 0x30), u32(data, 0x34), u32(data, 0x38), 0x04, 0x68),
    }
    output_dir.mkdir(parents=True, exist_ok=True)
    manifest = {
        "input": str(input_path),
        "magic": "NSO0",
        "flags": flags,
        "buildId": data[0x40:0x60].hex(),
        "moduleOffset": u32(data, 0x1C),
        "bssSize": u32(data, 0x3C),
        "segments": {},
    }
    for name, (file_offset, memory_offset, size, flag, comp_size_offset) in segments.items():
        compressed_size = u32(data, comp_size_offset)
        decoded = segment(data, file_offset, compressed_size, size, bool(flags & flag))
        (output_dir / f"{name}.bin").write_bytes(decoded)
        manifest["segments"][name] = {
            "fileOffset": file_offset,
            "memoryOffset": memory_offset,
            "decompressedSize": size,
            "compressedSize": compressed_size,
            "compressed": bool(flags & flag),
        }
    (output_dir / "manifest.json").write_text(
        json.dumps(manifest, indent=2) + "\n", encoding="utf-8"
    )
    return manifest


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    print(json.dumps(extract(args.input, args.output), indent=2))


if __name__ == "__main__":
    main()
