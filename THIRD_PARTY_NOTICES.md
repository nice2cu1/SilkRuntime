# 第三方声明

本文记录当前 SilkRuntime 源码快照中的第三方代码、构建输入和仅参考项目。第三方
作者、版权和许可证不因项目整体采用 GPL-2.0-only 而被重新归属于 SilkRuntime；源
文件中的原有声明继续有效。

## 项目整体许可与 exlaunch 基础

SilkRuntime 基于 exlaunch 固定提交
[`f9f4b0dd07b68f97958cb9c79228bbca22ca80d5`](https://github.com/shadowninja108/exlaunch/tree/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5)
修改。该提交的仓库许可证是完整 GNU GPL version 2 文本，保留源文件头部也写明
“version 2”，没有“or any later version”授权。因此本项目对包含 exlaunch 派生代码的
整体工程标注为 `GPL-2.0-only`；完整文本见 [LICENSE](LICENSE)，副本见
[licenses/exlaunch-GPL-2.0.txt](licenses/exlaunch-GPL-2.0.txt)。

固定提交、修改状态和当前源码哈希见
[licenses/source-inventory.json](licenses/source-inventory.json)。

## 随源码分发的组件

| 组件 | 当前证据与位置 | 许可证 | 用途 |
| --- | --- | --- | --- |
| [exlaunch](https://github.com/shadowninja108/exlaunch/tree/f9f4b0dd07b68f97958cb9c79228bbca22ca80d5) | 固定提交；`source/`、`misc/`、构建规则及配置中保留并修改的代码 | [GPL-2.0-only](licenses/exlaunch-GPL-2.0.txt) | 原生模块初始化、模块检查、Hook 基础设施和构建框架 |
| [Atmosphère-NX](https://github.com/Atmosphere-NX/Atmosphere) | `source/common.hpp`、`source/nn/` 及带有 Atmosphère-NX 版权头的文件 | GPL-2.0-only；以各源文件头部为准 | Switch 运行时类型和底层支持代码 |
| [libnx](https://github.com/switchbrew/libnx) 派生子集 | `source/lib/nx/`；文件保留 `libnx Authors` 版权标记，并由固定 exlaunch 快照带入 | [ISC](licenses/libnx-ISC.txt) | SVC、缓存、内存和结果码支持 |
| [And64InlineHook](https://github.com/Rprop/And64InlineHook) | `source/lib/hook/nx64/hook_impl.cpp` 中保留 MIT 许可块 | [MIT](licenses/And64InlineHook-MIT.txt) | AArch64 inline hook 实现 |
| oss-rtld / Thog | `source/rtld/` 及其 [`LICENSE.txt`](source/rtld/LICENSE.txt) | [原始 ISC 风格许可](licenses/oss-rtld-Thog.txt) | 运行时加载器代码；是否进入某个二进制取决于构建配置 |

`source/lib/nx/` 的当前文件与固定 exlaunch 快照中的对应文件一致，但本仓库没有
建立该历史子集对应的独立 libnx 提交；不能把当前安装的 libnx 版本倒推为其历史来源。
构建环境中的 libnx 4.12.0-1 是外部头文件和构建规则输入，构建环境和版本记录见
[README.md](README.md) 的“从源码构建”部分。

## 构建与链接输入

`misc/mk/common.mk` 显式链接 libpng 和 zlib。当前记录的构建包及许可证副本如下：

| 输入 | 记录版本 | 许可证 |
| --- | --- | --- |
| libpng | 1.6.48-1 | [PNG Reference Library License](licenses/libpng.txt) |
| zlib | 1.3.1-1 | [zlib license](licenses/zlib.txt)；另保留 [devkitPro 分发副本](licenses/zlib-devkitpro.txt) |
| libnx | 4.12.0-1 | [ISC](licenses/libnx-ISC.txt)；提供构建规则和头文件 |

devkitA64、GCC、binutils、newlib 和 GNU Make 仅作为构建环境工具，不随本源码目录
分发。若发布具体二进制，必须根据该版本的链接映射确认实际进入二进制的 GCC
runtime/newlib 成员，并随发布物提供对应声明；本文件不把未核实的归档成员列为已
确认内容。

## 仅参考项目

- [Skyline](https://github.com/skyline-dev/skyline)：用于历史 Native Module 和诊断研究；当前源码不包含其代码，也不链接其库。其许可证为 [MIT](https://github.com/skyline-dev/skyline/blob/2eb226fa9e4ab023dc00cb0db10ad4ec50ac4fa3/LICENSE)。
- [Il2CppDumper](https://github.com/Perfare/Il2CppDumper)：用于元数据布局参考；当前源码不包含、执行或编译其代码。参考提交为 `4741d46ba9cd6159c5d853eb9d6fc48b4bfa2b1a`，其许可证为 [MIT](https://github.com/Perfare/Il2CppDumper/blob/4741d46ba9cd6159c5d853eb9d6fc48b4bfa2b1a/LICENSE)。

游戏文件、PC 皮肤包、历史诊断二进制和本地第三方检出目录是开发输入，不属于本项目
源码分发，也不因出现在同一工作区而获得 SilkRuntime 许可证。
