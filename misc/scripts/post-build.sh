#!/bin/bash
# SPDX-License-Identifier: GPL-2.0-only
# 来源于 exlaunch 提交 f9f4b0dd07b68f97958cb9c79228bbca22ca80d5。
# 暂存 subsdk9，并在 SilkRuntime/output 中保留规范的 main.npdm。
# 参见 THIRD_PARTY_NOTICES.md 和 licenses/source-inventory.json。
set -e

# 保持部署文件名为字面值。当前工具链下，Makefile 的 BINARY_NAME 注释会使导出值
# 末尾多出一个空格。
INPUT_NSO=${OUTPUT}.nso
OUT_NSO=${OUT}/subsdk9

# 清理旧构建。
rm -rf ${OUT}

# 创建输出目录。
mkdir ${OUT}

# 将构建结果复制到 output/deploy。
mv "${INPUT_NSO}" "${OUT_NSO}"
# main.npdm 由外层构建目标根据经过验证的 Skyline/H1e 清单生成。
# 它会保留在 SilkRuntime/output 中；SilkPorter 会将这个精确文件复制到最终 ExeFS 包。

echo "已暂存……${OUT_NSO}"

# 如果定义了用户路径，则复制 ELF。
if [ ! -z $ELF_EXTRACT ]; then
    cp "${OUTPUT}.elf" "$ELF_EXTRACT"
fi
