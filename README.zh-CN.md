<div align="center">

# RTX Encore

**为 GeForce RTX 20、RTX 30 和 RTX 40 带来最高 X6 的帧生成、\
Smooth Motion 和 Neural Rendering。**

只需把一个文件放在游戏旁边，自带菜单，无需安装其他任何东西。

[![最新版本](https://img.shields.io/github/v/release/SilyNoMeta/rtx-encore?include_prereleases&style=for-the-badge&label=Latest&color=76b900)](https://github.com/SilyNoMeta/rtx-encore/releases)
[![下载量](https://img.shields.io/github/downloads/SilyNoMeta/rtx-encore/total?style=for-the-badge&color=2f81f7)](https://github.com/SilyNoMeta/rtx-encore/releases)

[![显卡](https://img.shields.io/badge/GPUs-RTX%2020%20%7C%2030%20%7C%2040-76b900?style=flat-square&logo=nvidia&logoColor=white)](docs/COMPATIBILITY.zh-CN.md)
[![图形 API](https://img.shields.io/badge/APIs-DirectX%2011%20%7C%2012%20%7C%20Vulkan-0078d4?style=flat-square)](docs/COMPATIBILITY.zh-CN.md)

**[下载](https://github.com/SilyNoMeta/rtx-encore/releases)** ·
**[安装](docs/INSTALLATION.zh-CN.md)** ·
**[English](README.md)** ·
**[Français](README.fr.md)**

</div>

> [!IMPORTANT]
> RTX Encore 是一个独立的实验性模组，与 NVIDIA 没有关联，也未获得 NVIDIA 的认可。
> 它在游戏进程内加载：请勿在联机游戏或受反作弊保护的游戏中使用，否则可能被拦截，或导致账号被标记。
> 替换任何文件之前请先备份。

## 功能

| | 功能 | 简介 |
| :---: | :--- | :--- |
| 🎞️ | **[X2 至 X6 帧生成](docs/FRAME-GENERATION.zh-CN.md)** | 在自带 DLSS 帧生成的游戏中，为 RTX 20、30 和 40 提供 DLSS 多帧生成。可选固定倍率、带目标帧率的动态模式，或交给游戏决定。 |
| 🛡️ | **更干净的生成帧** | 防止细小几何体（栅栏、电线、植被）撕裂，减少移动阴影中的瑕疵；界面和文字保持清晰。 |
| ⚡ | **更轻量的帧生成** | 一个性能选项，在不改变画面的前提下降低每个生成帧的 GPU 开销。 |
| 🌀 | **[RTX 30 上的 Smooth Motion](docs/SMOOTH-MOTION.zh-CN.md)** | NVIDIA 驱动的插帧功能，适用于 DirectX 11、DirectX 12 和 Vulkan 游戏，包括没有 DLSS 的游戏。默认关闭。 |
| ✨ | **[Neural Rendering](docs/NEURAL-RENDERING.zh-CN.md)** | 在 RTX 20、30 和 40 上运行 NR，并提供 RTX Encore 自行开发的 Ampere 性能选项：优化处理、Fast 精度、独立的 NR 分辨率和 Open 超快模式。支持一至四遍处理、每遍独立风格以及 HDR。实验性功能，默认关闭。 |
| 🎛️ | **[内置菜单和叠加层](docs/MENU-AND-OVERLAY.zh-CN.md)** | 在游戏中按 **Insert**：所有设置、每项功能的状态，以及显示帧率、倍率、帧时间稳定性、GPU 和显存的叠加层。支持 DirectX 11、DirectX 12 和 Vulkan。 |
| 🔍 | **DLSS 渲染分辨率** | 在已使用 DLSS 的游戏中选择 DLAA、质量、平衡、性能、超级性能或自定义比例。 |
| 🧩 | **一个通用文件** | `rtx-encore.dll` 可改成游戏会加载的文件名（`version.dll`、`dinput8.dll`、`winmm.dll`、`dxgi.dll` 以及另外十五个），也可作为 `.asi` 插件运行。 |

## 面向 Ampere 优化的 Neural Rendering

**让 RTX 30 上的 NR 更实用，是 RTX Encore 开发的重点之一。** 我们的工作包括在本地参考测试中保持输出一致的
加速处理、可选的 Fast 精度、独立于 DLSS 的 NR 分辨率、降低 Open 内存占用，以及为我们集成的 OpenDLSS-NR 开发的性能选项。

Open 的**超快模式** **Ultra-fast mode** 在 RTX 3070 Ti Laptop、帧生成 X3 的《黑神话：悟空》内置
基准测试中，将 NR 时间从 **18.7 ms 降至 9.8 ms（−48 %）**，显示帧率从 **73.8 FPS 提升至 94.9 FPS（+29 %）**。
这是已有对比，每种配置各运行一次，并不保证所有情况下的收益。该模式是实验性的，只支持一遍 NR；运动时可能出现
NR 效果滞后或拖影。

**Ultra Performance 分辨率**是另一项独立选项：在保留你偏好的 DLSS 渲染设置时，降低 NR 的工作尺寸。
Open 的其他 RTX 30 设置也可以用轻微的画面差异或额外显存换取更短的 NR 时间。
**[查看测量结果、设置和取舍](docs/NEURAL-RENDERING.zh-CN.md)。**

## 支持的硬件

| 显卡 | 帧生成 | Smooth Motion | Neural Rendering |
| :--- | :---: | :---: | :---: |
| RTX 40 | ✅ | 由 NVIDIA 提供 | ✅ |
| RTX 30 | ✅ | ✅ | ✅ |
| RTX 20 | 🧪 实验性 | — | 🧪 实验性 |

RTX Encore 面向 RTX 20、30 和 40；RTX 50 显卡的帧生成和 Smooth Motion 由 NVIDIA 提供。
详细信息、图形 API、驱动和游戏说明见 **[兼容性](docs/COMPATIBILITY.zh-CN.md)**。

## 快速开始

1. 下载 **[最新版本](https://github.com/SilyNoMeta/rtx-encore/releases)** 并解压。
2. 把 `rtx-encore.dll` 复制到游戏可执行文件旁边，并重命名为 `version.dll`。
3. 启动游戏。首次启动时菜单会自动打开一次；按 **Insert** 可再次打开。
4. 在游戏设置中开启 DLSS 帧生成。如果之前已经开启，请先关闭再重新开启。

其他文件名、`.asi` 插件、更新和卸载见 **[安装](docs/INSTALLATION.zh-CN.md)**。

> [!TIP]
> 建议先使用默认设置：由游戏或 NVIDIA Profile Inspector 决定倍率。8 GB 显存的显卡建议从 X2 或 X3 开始
> （或在高/中纹理质量下使用 X4）。

## 操作

| 快捷键 | 作用 |
| :--- | :--- |
| `Insert` | 打开或关闭菜单 |
| `Ctrl + Alt + 2…6` | 固定倍率 X2 至 X6 |
| `Ctrl + Alt + PageUp / PageDown` | 提高或降低倍率 |
| `Ctrl + Alt + D` | 在固定模式和动态模式之间切换 |
| `Ctrl + Alt + ↑ / ↓` | 将动态模式的目标帧率提高或降低 5 FPS（按住 `Shift`：1 FPS） |
| `Ctrl + Alt + G` | 把选择权交还给游戏 |
| `Ctrl + Alt + O` / `P` | 显示或隐藏叠加层 / 把它移到下一个角落 |

所有快捷键都可以在菜单中修改。只有游戏窗口处于焦点时它们才会生效。

## 文档

| 指南 | English | Français | 中文 |
| :--- | :---: | :---: | :---: |
| 安装、更新、卸载 | [Open](docs/INSTALLATION.md) | [Ouvrir](docs/INSTALLATION.fr.md) | [打开](docs/INSTALLATION.zh-CN.md) |
| 帧生成 | [Open](docs/FRAME-GENERATION.md) | [Ouvrir](docs/FRAME-GENERATION.fr.md) | [打开](docs/FRAME-GENERATION.zh-CN.md) |
| RTX 30 上的 Smooth Motion | [Open](docs/SMOOTH-MOTION.md) | [Ouvrir](docs/SMOOTH-MOTION.fr.md) | [打开](docs/SMOOTH-MOTION.zh-CN.md) |
| Neural Rendering | [Open](docs/NEURAL-RENDERING.md) | [Ouvrir](docs/NEURAL-RENDERING.fr.md) | [打开](docs/NEURAL-RENDERING.zh-CN.md) |
| 菜单和叠加层 | [Open](docs/MENU-AND-OVERLAY.md) | [Ouvrir](docs/MENU-AND-OVERLAY.fr.md) | [打开](docs/MENU-AND-OVERLAY.zh-CN.md) |
| 设置文件 | [Open](docs/SETTINGS.md) | [Ouvrir](docs/SETTINGS.fr.md) | [打开](docs/SETTINGS.zh-CN.md) |
| 兼容性 | [Open](docs/COMPATIBILITY.md) | [Ouvrir](docs/COMPATIBILITY.fr.md) | [打开](docs/COMPATIBILITY.zh-CN.md) |
| 故障排除 | [Open](docs/TROUBLESHOOTING.md) | [Ouvrir](docs/TROUBLESHOOTING.fr.md) | [打开](docs/TROUBLESHOOTING.zh-CN.md) |
| 更新日志 | [Open](CHANGELOG.md) | [Ouvrir](CHANGELOG.fr.md) | [打开](CHANGELOG.zh-CN.md) |

## 发布版本

RTX Encore 以可直接使用的发布包形式在本页面提供；其源代码不公开。GitHub 会显示每个可下载压缩包的 SHA-256：
安装前请与发布说明中的值进行比对。

版本号从 1.0.0 开始。此前以 DLSSG-Transfusion 名称发布的版本（直到 `v1.4.5.3-rtx20-30-40`）已不再提供；
[从这些版本升级](docs/INSTALLATION.zh-CN.md#从旧名称升级)会保留你的设置。

## 报告问题

请提交 [issue](https://github.com/SilyNoMeta/rtx-encore/issues)，并注明游戏及其图形 API、显卡和驱动版本，
以及菜单为你复制的报告（**System** 选项卡，**Copy** 按钮）。
[故障排除](docs/TROUBLESHOOTING.zh-CN.md)列出了首先应检查的事项。

## 致谢

RTX Encore 最初是 TonyJoaca 的 [DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) 的延续，
此后已发展为一个独立的项目。它同样受益于以下项目的工作：

- [dashdogy/RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock)：帧生成研究、菜单渲染和输入处理；
- [mavismmg/MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx)：帧生成画质研究；
- [sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86)：RTX 20 和 RTX 30 上的帧生成研究；
- [maanHimself/OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR)：开放的 Neural Rendering 引擎；
- 所有测试版本并报告问题的人：有好几项修复正是因为这些报告才得以完成。

发布包中包含的组件及其许可证列在 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) 中。

## 免责声明

RTX Encore 按“原样”提供，不附带任何担保：使用风险由你自行承担。画质、稳定性和性能会因游戏、游戏版本、
驱动和硬件而异。高倍率会增加延迟和显存占用。

发布包中包含的组件保留其各自的许可证：见 [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)，分发发布包的
任何副本时都必须附带该文件。NVIDIA、GeForce、RTX 和 DLSS 是 NVIDIA Corporation 的商标。NVIDIA 软件和游戏仍受其
各自条款的约束；发布包中不包含其中任何一项。
