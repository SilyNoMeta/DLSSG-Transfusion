# 兼容性

*[English](COMPATIBILITY.md) · [Français](COMPATIBILITY.fr.md)*

这些说明记录测试和观察结果，并非兼容性认证。游戏更新、其他驱动或其他模组都可能改变结果。

## 显卡

| 显卡 | 帧生成 | Smooth Motion | Neural Rendering |
| :--- | :--- | :--- | :--- |
| RTX 40 | 支持。已在 RTX 4090 上测试 DirectX 12 和 Vulkan。 | 由 NVIDIA 提供 | 支持 |
| RTX 30 | 支持。已在 RTX 3070 Ti Laptop 上测试 DirectX 12。 | 支持（需使用下文列出的驱动） | 支持 |
| RTX 20 | 实验性：尚未在真实 RTX 20 显卡上验证。 | 不支持 | 实验性 |

RTX Encore 面向 RTX 20、30 和 40。不支持 GTX 显卡及其他厂商的显卡。

## 图形 API

| 功能 | DirectX 11 | DirectX 12 | Vulkan |
| :--- | :---: | :---: | :---: |
| X2 至 X6 帧生成 | — | ✅ | ✅（RTX 20 和 RTX 30 上为实验性） |
| UI assist | — | ✅ | — |
| Smooth Motion（RTX 30） | ✅ | ✅ | ✅ |
| Neural Rendering | 🧪 | 🧪 | 🧪 |
| 菜单和叠加层 | ✅ | ✅ | ✅ |

不支持 DirectX 9 和 OpenGL 游戏。

## DLSS 帧生成版本

帧生成支持游戏的以下 `nvngx_dlssg.dll` 版本：

`310.1.0` · `310.2.0` · `310.2.1` · `310.3.0` · `310.4.0` · `310.5.0` · `310.5.2` · `310.5.3` · `310.6.0` ·
`310.7.0` · `310.7.128` · `310.7.129` · `310.8.0` · `310.9.0` · `310.9.1`

- 其他版本保持原样，游戏继续使用自己的帧生成。
- 所有显卡推荐使用 **310.9.0 或 310.9.1**，以获得 X2 以上的最佳画质；RTX 20 必须使用其中之一。
  **Fast frame generation** 需要 310.9.1。
- 游戏的其他 DLSS 文件（超分辨率、光线重建等）可以使用任意版本。

## Smooth Motion 驱动

RTX 30 上的 Smooth Motion 仅接受 NVIDIA 驱动 **617.42**、**617.14**、**616.92** 和 **616.64**。
只有 **617.14** 已在游戏中验证。见 [Smooth Motion](SMOOTH-MOTION.zh-CN.md)。

## Neural Rendering 文件

Neural Rendering 需要你自行提供 NVIDIA 的 `nvngx_dlssnr.dll` **310.8.0**。
见 [Neural Rendering](NEURAL-RENDERING.zh-CN.md)。

## 游戏说明

| 游戏 | API | 说明 |
| :--- | :---: | :--- |
| Cyberpunk 2077 | DX12 | 在 RTX 3070 Ti Laptop 上测试了帧生成。可作为 `version.dll` 或 `.asi` 插件运行；见[安装](INSTALLATION.zh-CN.md#cyberpunk-2077)。 |
| Black Myth: Wukong（黑神话：悟空） | DX12 | 在 RTX 4090 上测试了帧生成。用户确认 NR 运行。 |
| No Man's Sky | Vulkan | 在 RTX 4090 上测试了帧生成。**请关闭 Automatic UI recomposition**：它在此游戏的镜头移动中造成双重轮廓。用户确认了 1080p 和 1440p 下 NR 与帧生成 X2 一起运行。 |
| DOOM: The Dark Ages | Vulkan | 在 RTX 4090 上测试了帧生成。无论模组设置如何，游戏都保留自己的界面处理选择。 |
| Starfield | DX12 | 开启 UI assist 时可运行；根据用户报告修复了读取存档时的崩溃。 |
| The Witcher 3 | DX12 | 游戏构建 25575366 和 25646871 支持兼容的光线追踪毛发；见[菜单和叠加层](MENU-AND-OVERLAY.zh-CN.md#其他控制)。 |
| Manor Lords | DX12 | 在 RTX 30、驱动 617.14 上观察到 Smooth Motion 运行。 |
| Shadows of Doubt | DX11 | 在 RTX 30、驱动 617.14 上观察到 Smooth Motion 运行。用户确认 NR 运行。 |
| Enshrouded | Vulkan | 在 RTX 30、驱动 617.14 上观察到 Smooth Motion 运行。用户确认 NR 运行。 |
| Bodycam、Palworld、Portal with RTX、Star Wars Zero Company | — | 用户确认 NR 运行。 |

欢迎通过 [issue](https://github.com/SilyNoMeta/rtx-encore/issues) 报告其他游戏中的观察结果：
游戏、图形 API、显卡、驱动，以及哪些功能可用或不可用。

## 其他工具

| 工具 | 说明 |
| :--- | :--- |
| ReShade、Special K | 保留它们自己的 `dxgi.dll`，给 RTX Encore 选择其他名称。菜单不需要 ReShade。 |
| DXVK | 不要把 RTX Encore 命名为 `dxgi.dll`：DXVK 提供该文件。菜单会绘制在它的 Vulkan 输出中。 |
| OptiScaler | 有用于特定配置的专用选项：见[帧生成](FRAME-GENERATION.zh-CN.md#兼容性选项)。 |
| Cyber Engine Tweaks、RED4ext | 保留它们自己的文件名。Cyber Engine Tweaks 有可选的实验性面板。 |
| NVIDIA Profile Inspector | 在 **Game decides** 模式中遵循它设置的倍率。 |

## 反作弊和在线游戏

RTX Encore 在游戏进程内加载。带有反作弊保护的游戏可能拒绝启动或标记账号。请勿在这些游戏中使用。
