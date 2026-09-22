#!/usr/bin/env bash
set -euo pipefail

# 构建需要显式启用的全 TK2D Collection 运行时皮肤候选包。源代码默认值仍然是
# 已知安全的模块探测；在这里传入参数可以复现实机实验构建，而不改变默认行为。
SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd -- "${SCRIPT_DIR}/.." && pwd)"
IMAGE=docker.io/devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
EXPECTED_NPDM_SHA256=27b2de6a0c7324a8e4141cc6482281067c4cb6d2d3444983412cdaf6783542de

sudo podman run --rm \
  -v "${ROOT}:/work" \
  -w /work \
  "${IMAGE}" \
  sh -lc "set -eu
    make clean TARGET=SilkModLoader && make -j2 TARGET=SilkModLoader CXX_FLAGS=-DSILKMODLOADER_RUNTIME_SKIN \
      RUNTIME_NPDM=/work/output/main.npdm
    actual=\$(sha256sum /work/output/main.npdm | cut -d' ' -f1)
    test \"\${actual}\" = \"${EXPECTED_NPDM_SHA256}\"
    test ! -e /work/output/SilkModLoader.npdm"

echo "SilkRuntime 运行时皮肤候选包和经过验证的 main.npdm 已构建完成"
