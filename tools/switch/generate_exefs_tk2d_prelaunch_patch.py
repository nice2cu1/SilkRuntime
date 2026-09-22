"""生成范围受限的 Atmosphère ExeFS IPS32 启动前补丁。

补丁会将一个绑定 Build ID 的 tk2d Init 调用重定向到现有的 subsdk9 桥接函数。
Atmosphère 会在 NSO 执行前应用补丁，因此游戏进程不需要 dmnt、调试器或 exlaunch
的进程内存别名。

本工具不会修改原始 NSO。它会将解压后的文本段与期望指令进行验证，并且只写入
用户指定的补丁文件。
"""

from __future__ import annotations

import argparse
import struct
from pathlib import Path


BUILD_ID = "FC9EA4CCC955D5799F37752B2D730B31"
CALL_SITE_RVA = 0x5B582A8
EXPECTED_INSTRUCTION = 0x9400021E
NSO_HEADER_SIZE = 0x100


def encode_bl(from_address: int, to_address: int) -> int:
    delta = to_address - from_address
    if from_address == 0 or to_address == 0:
        raise ValueError("调用点和 Hook 地址不能为零")
    if (from_address | to_address) & 0x3:
        raise ValueError("调用点和 Hook 地址必须按 4 字节对齐")
    if delta < -(1 << 27) or delta >= (1 << 27):
        raise ValueError(f"BL 目标超出范围：delta={delta}")
    if delta & 0x3:
        raise ValueError(f"BL 目标没有按指令对齐：delta={delta}")
    return 0x94000000 | ((delta >> 2) & 0x03FFFFFF)


def make_ips32(offset: int, instruction: int) -> bytes:
    if not 0 <= offset <= 0xFFFFFFFF:
        raise ValueError(f"IPS32 偏移超出范围：0x{offset:x}")
    return (
        b"IPS32"
        + offset.to_bytes(4, "big")
        + (4).to_bytes(2, "big")
        + struct.pack("<I", instruction)
        + b"EEOF"
    )


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-id", required=True)
    parser.add_argument("--module-delta", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--hook-offset", required=True, type=lambda value: int(value, 0))
    parser.add_argument("--text", required=True, type=Path)
    parser.add_argument("--output", required=True, type=Path)
    args = parser.parse_args()

    build_id = args.build_id.upper()
    if build_id != BUILD_ID:
        raise SystemExit(f"不支持的 Build ID：{build_id}")

    text = args.text.read_bytes()
    if CALL_SITE_RVA + 4 > len(text):
        raise SystemExit("解压后的 main 文本段短于调用点 RVA")
    actual = struct.unpack_from("<I", text, CALL_SITE_RVA)[0]
    if actual != EXPECTED_INSTRUCTION:
        raise SystemExit(
            "调用点指令不匹配："
            f"期望 0x{EXPECTED_INSTRUCTION:08x}，实际为 0x{actual:08x}"
        )

    target = args.module_delta + args.hook_offset
    instruction = encode_bl(CALL_SITE_RVA, target)
    patch_offset = NSO_HEADER_SIZE + CALL_SITE_RVA
    patch = make_ips32(patch_offset, instruction)

    # 确认补丁文件偏移可以映射回 Atmosphère NSO 补丁器使用的解压文本段 RVA。
    mapped_text_offset = patch_offset - NSO_HEADER_SIZE
    if mapped_text_offset != CALL_SITE_RVA:
        raise AssertionError("NSO 头部调整改变了调用点 RVA")

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(patch)
    print(f"Build ID：{build_id}")
    print(f"调用点 RVA：0x{CALL_SITE_RVA:x}")
    print(f"IPS32 文件偏移：0x{patch_offset:x}")
    print(f"模块增量：0x{args.module_delta:x}")
    print(f"Hook 偏移：0x{args.hook_offset:x}")
    print(f"相对于 main 的 BL 目标：0x{target:x}")
    print(f"原始指令：0x{actual:08x}")
    print(f"补丁指令：0x{instruction:08x}")
    print(f"已写入：{args.output}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
