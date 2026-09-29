# SilkRuntime

SilkRuntime 是 Nintendo Switch 版《Hollow Knight: Silksong》的原生运行时项目。
当前运行时只负责皮肤功能：编译 `subsdk9`，生成与目标游戏匹配的正式
`main.npdm`，并提供 ELF 和符号清单给 [SilkPorter](https://github.com/nice2cu1/SilkPorter) 生成 ExeFS IPS 补丁。

资源转换、皮肤目录生成和最终 SD 卡包组装不属于 SilkRuntime，而由同级的
[SilkPorter](https://github.com/nice2cu1/SilkPorter) 项目负责。

## 当前兼容目标

- Title ID：`010013C00E930000`
- 当前支持的游戏 Build ID：`FC9EA4CCC955D5799F37752B2D730B31`
- 已验证成功的游戏版本：`ver. 1.0.30000`
- 已验证成功的 NS 系统版本：`22.5.0`
- 已验证成功的 Atmosphère（AMS）版本：`1.11.2`
- 当前构建模式：`SILKMODLOADER_RUNTIME_SKIN`

## SilkRuntime 负责什么

### 1. 构建 `subsdk9`

源代码经过 devkitA64 编译、链接得到：

```text
source/**/*.cpp + source/**/*.s
        ↓ 编译、链接
output/SilkModLoader.elf
        ↓ elf2nso
output/deploy/subsdk9
```

`output/SilkModLoader.elf` 是地址验证、调试和 IPS 生成的依据；
`output/deploy/subsdk9` 才是要放入游戏 ExeFS 覆盖目录的运行时模块。

### 2. 构建正式 `main.npdm`

`main.npdm` 是目标游戏 ExeFS 需要的正式文件名。它由 NPDM JSON 输入经过
`npdmtool` 生成：

```text
misc/npdm-json/skyline.json
        ↓ npdmtool
output/main.npdm
```

该文件描述目标 Title ID、地址空间、服务权限和运行时所需的内核能力。
它属于游戏的 `main.npdm` 覆盖文件。

### 3. 提供 IPS 生成输入

[SilkRuntime](https://github.com/nice2cu1/SilkRuntime) 不负责生成最终的 `exefs_patches/SilkModLoader/*.ips`。它只提供：

- 当前构建的 `output/SilkModLoader.elf`；
- 当前链接得到的 `output/build/SilkModLoader.lst`；
- 与这两个文件配套的 `output/deploy/subsdk9`。

[SilkPorter](https://github.com/nice2cu1/SilkPorter) staging 阶段读取这些文件和目标游戏 `main` 的文本段，重新计算当前
ELF 布局下的跳转指令，并生成包含多个调用点的完整 IPS32 补丁。这样可以避免把
旧 ELF 的地址补丁误用于新构建。

`tools/switch/generate_exefs_tk2d_prelaunch_patch.py` 只用于单个 TK2D 调用点的
诊断或手工实验。

## 最终输出

所有 SilkRuntime 的公开构建产物都位于 `output/` 下。构建完成后，至少应看到：

```text
SilkRuntime/
└─ output/
   ├─ main.npdm
   ├─ SilkModLoader.elf
   ├─ deploy/
   │  └─ subsdk9
   └─ build/
      └─ SilkModLoader.lst
```

`output/build/` 还会包含目标文件、依赖文件和链接映射；上面的树只列出后续流程
需要确认的文件。

## 构建前提

推荐在 Windows 的 WSL2 Ubuntu 中使用固定的 devkitA64 容器构建：

```text
docker.io/devkitpro/devkita64@sha256:1fc388c3a0d34bd2045a6dadcb1020e069d5f876a187fd705de14b4440c00282
```

当前验证过的主要组件版本：

| 软件包 | 版本 |
| --- | --- |
| devkitA64 | r29.2-1 |
| devkita64-gcc | 15.2.0-7 |
| devkita64-binutils | 2.45.1-2 |
| devkita64-newlib | 4.6.0.20260123-4 |
| libnx | 4.12.0-1 |
| switch-libpng | 1.6.48-1 |
| switch-zlib | 1.3.1-1 |
| switch-tools | 1.13.1-1 |

使用容器包装脚本时还需要 WSL2、Podman 和可用的 `sudo` 权限。若已经在
devkitPro shell 内，则不需要 Podman，但必须能找到 `make`、`npdmtool`、
`elf2nso` 和 devkitA64 工具链。

## 推荐构建步骤：WSL2 + 固定容器

以下步骤从包含 `SilkRuntime` 的工作区根目录开始。

### 第 1 步：进入 WSL 并确认项目位置

Windows PowerShell 中执行：

```powershell
wsl.exe -- bash -lc 'cd /mnt/e/SilksongSwitchMod && pwd && test -f SilkRuntime/Makefile'
```

如果项目不在 `E:\SilksongSwitchMod`，把命令中的 `/mnt/e/SilksongSwitchMod`
替换为对应的 WSL 挂载路径。

### 第 2 步：执行清理构建

```powershell
wsl.exe -- bash -lc 'cd /mnt/e/SilksongSwitchMod && bash SilkRuntime/scripts/build_runtime_skin_wsl.sh'
```

这个脚本会依次完成以下操作：

1. 使用固定 digest 的 `devkita64` 容器；
2. 将 `SilkRuntime` 挂载为容器内的 `/work`；
3. 执行 `make clean TARGET=SilkModLoader`，清理旧的 `output/` 和旧兼容路径；
4. 执行带有 `SILKMODLOADER_RUNTIME_SKIN` 宏的并行构建；
5. 先生成 `output/SilkModLoader.elf`，再通过 `elf2nso` 生成
   `output/deploy/subsdk9`；
6. 使用 `npdmtool` 将 `misc/npdm-json/skyline.json` 转换为
   `output/main.npdm`；
7. 校验 `main.npdm` 的固定 SHA-256，并确认没有生成
   `output/SilkModLoader.npdm`。

脚本等价于在 devkitPro 环境中执行：

```sh
cd SilkRuntime
make clean TARGET=SilkModLoader
make -j2 TARGET=SilkModLoader CXX_FLAGS=-DSILKMODLOADER_RUNTIME_SKIN
```

### 第 3 步：检查四个交付文件

在 PowerShell 中执行：

```powershell
$runtimeOutput = 'E:\SilksongSwitchMod\SilkRuntime\output'
Get-Item `
  "$runtimeOutput\main.npdm", `
  "$runtimeOutput\SilkModLoader.elf", `
  "$runtimeOutput\deploy\subsdk9", `
  "$runtimeOutput\build\SilkModLoader.lst"
Get-FileHash `
  "$runtimeOutput\main.npdm", `
  "$runtimeOutput\SilkModLoader.elf", `
  "$runtimeOutput\deploy\subsdk9" -Algorithm SHA256
```

当前一次已验证构建的参考值为：

| 文件 | 大小 | SHA-256 |
| --- | ---: | --- |
| `output/main.npdm` | 1612 bytes | `27B2DE6A0C7324A8E4141CC6482281067C4CB6D2D3444983412CDAF6783542DE` |
| `output/SilkModLoader.elf` | 1542784 bytes | `50AACDBC4BAB9A6611F8000FEF48EBD284F56CB2CDB1DF3D937A5F79293F21B2` |
| `output/deploy/subsdk9` | 159063 bytes | `02CF43ACB5A6EAA327C5BDC677FFE66294909B6E24D185A7F603951B2F20F866` |
| `output/build/SilkModLoader.lst` | 35078 bytes | `E2B890E206406A84D185C8792C5E4942FB78DFDCAFAFBA754ADA74EB55F2ED81` |

哈希会随源代码、工具链或构建参数变化；参考值用于确认“当前这次构建”和
配套 IPS 是否来自同一批输出，不应替代实际文件校验。

## 直接使用已配置的 devkitPro 环境

如果机器已经进入 devkitPro shell，可按下面的顺序执行，不需要使用包装脚本：

### 第 1 步：进入项目并清理

```sh
cd /path/to/SilkRuntime
make clean TARGET=SilkModLoader
```

### 第 2 步：编译运行时和 NPDM

```sh
make -j2 TARGET=SilkModLoader CXX_FLAGS=-DSILKMODLOADER_RUNTIME_SKIN
```

此命令不是只编译一个 `.nso`：Makefile 会同时建立 C/C++/汇编依赖、链接 ELF、
转换 `subsdk9`，并调用 `npdmtool` 生成 `output/main.npdm`。

### 第 3 步：确认没有错误的 NPDM 文件

```sh
test -f output/main.npdm
test -f output/deploy/subsdk9
test -f output/SilkModLoader.elf
test -f output/build/SilkModLoader.lst
test ! -e output/SilkModLoader.npdm
```

如果构建目录中出现 `SilkModLoader.npdm`，说明使用了旧 Makefile 或错误的
NPDM 规则，不应把该文件复制进最终包。

## 交给 [SilkPorter](https://github.com/nice2cu1/SilkPorter) 的文件

完成 SilkRuntime 构建后，把下列文件作为同一批构建结果交给
[SilkPorter](https://github.com/nice2cu1/SilkPorter)：

```text
SilkRuntime/output/main.npdm
SilkRuntime/output/deploy/subsdk9
SilkRuntime/output/SilkModLoader.elf
SilkRuntime/output/build/SilkModLoader.lst
```

[SilkPorter](https://github.com/nice2cu1/SilkPorter) 会用 ELF 和符号清单生成当前 Build ID 的完整 IPS32 补丁，再把
`main.npdm`、`subsdk9`、IPS 和 `Mods/Skin` 资源组装成最终包。不要把旧构建的
ELF、符号表、`subsdk9` 或 IPS 与本次 `main.npdm` 混搭。

## 运行时资源约定

当前 Loader 固定读取：

```text
romfs/SilkModLoader/Mods/Skin/
```

Loader 启动时会枚举 `Skin/` 的直接子目录，并要求恰好存在一个任意命名的皮肤
子目录；随后把这个子目录作为资源根目录读取。因此其内部必须是 [SilkPorter](https://github.com/nice2cu1/SilkPorter)
生成的资源结构。没有皮肤子目录或同时存在多个皮肤子目录时，Loader 会跳过皮肤
替换。当前 Loader 不读取 `active.txt`；这个文件不应出现在最终包中。

## 已加载纹理发现

SkinLoader 在 Unity 主线程定期枚举已加载的 Texture2D，分帧处理预先绑定在
Prefab/Sprite/材质中以及后续加载的纹理。它与现有 Sprite、Material 观察入口
共用精确名称匹配和对象身份去重。已收到 Switch 的 discovery 应用成功日志及用户
画面确认。缓存使用 native 地址和 Instance ID、线性探测及前后两轮记录，减少冲突
与重复文件访问。扫描摘要仅在首次、有实际应用或异常时输出；本次缓存优化待真机复核。

扫描与 GCHandle 生命周期见 [loaded_texture_scanner.cpp](source/skin/loaded_texture_scanner.cpp)，
二进制地址绑定见 [offsets.hpp](source/program/offsets.hpp)。GDB SVC 中使用 `[SkinScan]`
及 `observer=discovery` 识别新路径。这项改动需要配套 SilkPorter 从当前 ELF 重新生成 IPS32。
枚举和单次 LoadImage 不能拆分，实际帧耗时仍需真机确认。

## 发布与许可证

发布运行时二进制时，应同时记录源码版本、构建参数、工具链版本、目标游戏
Build ID 和四个输出文件的 SHA-256。第三方组件和来源见：

- [许可证全文](LICENSE)
- [第三方声明](THIRD_PARTY_NOTICES.md)
- [源码与依赖清单](licenses/source-inventory.json)
