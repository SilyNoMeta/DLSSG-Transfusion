# 帧生成

*[English](FRAME-GENERATION.md) · [Français](FRAME-GENERATION.fr.md)*

RTX Encore 为 RTX 20、RTX 30 和 RTX 40 显卡带来 X2 至 X6 的 DLSS 多帧生成。

## 要求

- 游戏必须提供 **DLSS 帧生成**，并运行在 DirectX 12 或 Vulkan 下。RTX Encore 扩展的是游戏已有的帧生成；
  它无法为没有帧生成的游戏添加这一功能。对于这类游戏，请参阅 [Smooth Motion](SMOOTH-MOTION.zh-CN.md)。
- 游戏的帧生成文件 `nvngx_dlssg.dll` 须为受支持的版本：见
  [兼容性](COMPATIBILITY.zh-CN.md#dlss-帧生成版本)。310.9.1 版本效果最好。
- 在游戏设置中开启帧生成。

## 模式

在菜单（**Frames** 选项卡）中或用快捷键选择模式。

| 模式 | 作用 |
| :--- | :--- |
| **Game decides**（默认） | 由游戏或 NVIDIA Profile Inspector 决定倍率。 |
| **Fixed** | 始终使用你选择的倍率：X2、X3、X4、X5 或 X6。 |
| **Dynamic** | 倍率随帧率变化以达到目标。目标是显示游戏的显示器的刷新率，或你设定的数值。 |

倍率是指游戏每渲染一帧所显示的帧数：X3 表示每个渲染帧之后显示两个生成帧。

- **X5 和 X6 是实验性的。** 它们需要大量显存，而且倍率越高延迟越大。
- 除非开启 **Allow 5x and 6x**，否则动态模式最高为 X4。
- 在 Vulkan 游戏中，动态模式根据实测帧率和目标在 X2 到 X6 之间选择。
- 8 GB 显存的显卡请从 X2 或 X3 开始，或在高/中纹理质量下使用 X4。

## 快捷键

| 快捷键 | 作用 |
| :--- | :--- |
| `Ctrl + Alt + 2…6` | 固定倍率 X2 至 X6 |
| `Ctrl + Alt + PageUp / PageDown` | 提高或降低倍率（切换到固定模式） |
| `Ctrl + Alt + D` | 在固定模式和动态模式之间切换 |
| `Ctrl + Alt + ↑` 或 `+` | 将动态模式的目标提高 5 FPS（按住 `Shift`：1 FPS） |
| `Ctrl + Alt + ↓` 或 `-` | 将动态模式的目标降低 5 FPS（按住 `Shift`：1 FPS） |
| `Ctrl + Alt + G` | 回到 **Game decides** |

快捷键会保存新的选择并立即应用。可以在菜单（**System** 选项卡）中修改快捷键，或逐个停用。

## 画质

**Frames** 选项卡的 **Quality** 分组包含作用于生成帧的选项。它们默认开启，并在游戏启动时读取。

- **Anti-tearing / anti-ghosting**：保护生成帧中的细小几何体（栅栏、电线、植被）和移动阴影。
- **Protection tuning**：*Refined*（默认）或 *Classic*。如果某个游戏出现 Refined 无法消除的瑕疵，再尝试 Classic。
- **High-multiplier quality**：让 X3 到 X6 的生成帧保持干净。请保持开启。
- **Fast frame generation**：下文所述的性能选项。请保持开启。

### Fast frame generation

开启 **Fast frame generation** 后，每个生成帧占用的 GPU 时间更少，而画面保持不变。在 RTX 3070 Ti Laptop、1080p 下实测：

| 倍率 | 选项关闭 | 选项开启 | 差异 |
| :---: | ---: | ---: | ---: |
| X2 | 2.33 ms | 1.57 ms | −33 % |
| X6 | 6.38 ms | 5.23 ms | −18 % |

这些数据来自单一系统，并不代表所有显卡或游戏都能达到。该选项需要 `nvngx_dlssg.dll` 310.9.1。

## 游戏界面

当游戏界面与 3D 场景分开处理时，生成帧最为干净。**Frames** 选项卡中有三个开关：

| 设置 | 默认 | 作用 |
| :--- | :---: | :--- |
| **Automatic UI recomposition** | 开启 | 只要游戏提供了所需内容，就使用界面单独处理。如果生成帧在运动中出现扭曲或重影，请关闭它。 |
| **UI assist (D3D12)** | 开启 | 在没有提供无界面场景和界面层的 DirectX 12 游戏中，由 RTX Encore 构建它们。游戏自身提供的内容始终优先。 |
| **Force UI recomposition** | 关闭 | 即使游戏没有要求，也请求单独处理。用于测试。 |

针对特定游戏的建议见[兼容性](COMPATIBILITY.zh-CN.md#游戏说明)。

## 兼容性选项

位于 **System** 选项卡：

| 设置 | 默认 | 作用 |
| :--- | :---: | :--- |
| **Disable menu detection** | 关闭 | 关闭时：帧生成会在菜单和加载画面中暂停，可避免在这些地方崩溃。请保持关闭。 |
| **Force NVIDIA OTA models** | 关闭 | 使用 NVIDIA 应用下载的帧生成模型。需重启游戏生效。 |
| **OptiScaler flip metering bypass** | 关闭 | 仅用于需要它的 OptiScaler 配置。需重启游戏生效。 |
| **Graphics card series** | auto | 除非显卡未被识别，否则保持 auto。 |

## 读懂叠加层

| 示例 | 含义 |
| :--- | :--- |
| `74 fps` | 游戏的帧率；未观察到帧生成 |
| `74/37 fps 2x` | 显示帧率 / 渲染帧率，以及观察到的倍率 |
| `296/74 fps 2x+sm` | 帧生成与 Smooth Motion 同时工作 |

如果本次会话中没有检测到 DLSS 帧生成，选择倍率并不能凭空产生帧生成：菜单会给出提示。

## 限制

- 对 RTX 20 的支持是实验性的：尚未在真实的 RTX 20 显卡上验证。
- RTX 20 和 RTX 30 上的 Vulkan 支持是实验性的。
- 帧生成提高的是显示帧率，而不是游戏的响应速度：倍率越高延迟越大，而且渲染帧率本身足够高时效果更好。
