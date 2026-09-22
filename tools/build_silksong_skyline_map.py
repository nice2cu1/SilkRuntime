#!/usr/bin/env python3
"""根据真实 Silksong NSO 的 PLT 生成 Skyline 链接器映射。

映射只包含具有 JUMP_SLOT 重定位的导入符号。在 AArch64 上，重建的 PLT 包含两个
解析器槽位，之后每个 .rela.plt 条目对应一个 16 字节槽位；工具会根据 ELF 节区
大小检查这一关系，而不是猜测某个游戏专用常量。
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


ELF64 = "<16sHHIQQQIHHHHHH"
SHDR64 = "<IIQQQQIIQQ"
SYMENT64 = "<IBBHQQ"
RELA64 = "<QQq"
SHT_DYNSYM = 11
SHT_RELA = 4
R_AARCH64_JUMP_SLOT = 0x402
SHN_UNDEF = 0


def read_cstr(blob: bytes, offset: int) -> str:
    if offset < 0 or offset >= len(blob):
        raise ValueError(f"字符串偏移 {offset:#x} 超出字符串表")
    end = blob.find(b"\0", offset)
    if end < 0:
        raise ValueError(f"偏移 {offset:#x} 处的字符串没有结束符")
    return blob[offset:end].decode("utf-8")


def parse_sections(data: bytes) -> dict[str, tuple[int, int, int, int, int, int]]:
    header = struct.unpack_from(ELF64, data, 0)
    _, _, _, _, _, _, shoff, _, _, _, _, shentsize, shnum, shstrndx = header
    if shentsize != struct.calcsize(SHDR64):
        raise ValueError("ELF64 节区头尺寸异常")
    raw = [struct.unpack_from(SHDR64, data, shoff + i * shentsize) for i in range(shnum)]
    shstr = raw[shstrndx]
    shstr_blob = data[shstr[4] : shstr[4] + shstr[5]]
    sections: dict[str, tuple[int, int, int, int, int, int]] = {}
    for sh in raw:
        name = read_cstr(shstr_blob, sh[0]) if sh[0] else ""
        sections[name] = (sh[1], sh[3], sh[4], sh[5], sh[6], sh[9])
    return sections


def make_map(elf_path: Path, output_path: Path) -> dict[str, int]:
    data = elf_path.read_bytes()
    if data[:4] != b"\x7fELF" or data[4] != 2 or data[5] != 1:
        raise ValueError(f"{elf_path} 不是小端 ELF64 文件")

    sections = parse_sections(data)
    dynstr_type, dynstr_addr, dynstr_off, dynstr_size, _, _ = sections[".dynstr"]
    if dynstr_type != 3:
        raise ValueError(".dynstr 不是字符串表")
    dynstr = data[dynstr_off : dynstr_off + dynstr_size]

    _, _, dynsym_off, dynsym_size, dynsym_link, dynsym_entsize = sections[".dynsym"]
    if dynsym_link == 0 or dynsym_entsize != struct.calcsize(SYMENT64):
        raise ValueError("不支持的 .dynsym 布局")
    symbols: dict[int, tuple[str, int, int]] = {}
    for index in range(dynsym_size // dynsym_entsize):
        st_name, st_info, _, st_shndx, st_value, _ = struct.unpack_from(
            SYMENT64, data, dynsym_off + index * dynsym_entsize
        )
        symbols[index] = (read_cstr(dynstr, st_name), st_info & 0x0F, st_shndx)

    _, plt_addr, _, plt_size, _, plt_entsize = sections[".plt"]
    if plt_entsize != 16:
        raise ValueError(f"AArch64 PLT 条目尺寸异常：{plt_entsize}")
    _, _, rela_off, rela_size, _, rela_entsize = sections[".rela.plt"]
    if rela_entsize != struct.calcsize(RELA64):
        raise ValueError(".rela.plt 条目尺寸异常")

    reloc_count = rela_size // rela_entsize
    expected_plt_size = (2 + reloc_count) * plt_entsize
    if plt_size != expected_plt_size:
        raise ValueError(
            f"PLT/重定位不匹配：PLT 为 {plt_size:#x}，"
            f"{reloc_count} 个重定位应为 {expected_plt_size:#x}"
        )

    result: dict[str, int] = {}
    for index in range(reloc_count):
        r_offset, r_info, _ = struct.unpack_from(RELA64, data, rela_off + index * rela_entsize)
        if (r_info & 0xFFFFFFFF) != R_AARCH64_JUMP_SLOT:
            continue
        symbol_index = r_info >> 32
        name, _, shndx = symbols[symbol_index]
        if not name or shndx != SHN_UNDEF:
            continue
        plt_slot = plt_addr + (2 + index) * plt_entsize
        if name in result and result[name] != plt_slot:
            raise ValueError(f"导入符号对应了不同的 PLT 槽位：{name}")
        result[name] = plt_slot

    output_path.parent.mkdir(parents=True, exist_ok=True)
    lines = [
        "/* 根据真实 Silksong main NSO 生成；请勿手动编辑。 */",
        "OUTPUT_FORMAT(elf64-littleaarch64)",
        "OUTPUT_ARCH(aarch64)",
        "",
    ]
    for name, address in sorted(result.items()):
        if any(ch in name for ch in " @()"):
            raise ValueError(f"不支持的链接器符号写法：{name!r}")
        lines.append(f"{name} = 0x{address:x};")
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return result


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("elf", type=Path, help="根据 Silksong main NSO 重建的 ELF")
    parser.add_argument("output", type=Path, help="要创建的链接器脚本")
    args = parser.parse_args()
    symbols = make_map(args.elf, args.output)
    print(f"已生成 {args.output}，包含 {len(symbols)} 个导入 PLT 符号")


if __name__ == "__main__":
    main()
