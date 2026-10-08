# Neural Rendering

*[English](NEURAL-RENDERING.md) · [Français](NEURAL-RENDERING.fr.md)*

DLSS Neural Rendering（NR）增强 DLSS 超分辨率或 DLAA 生成的画面。RTX Encore 让它在 RTX 20、RTX 30
和 RTX 40 显卡上的 DirectX 11、DirectX 12 和 Vulkan 游戏中运行。

> [!NOTE]
> Neural Rendering 是实验性功能，默认关闭。它需要大量 GPU 时间：见[性能开销](#性能开销)。

各项功能的作用不同：**DLSS 超分辨率**重建游戏画面，**Neural Rendering**改变画面的呈现，**帧生成**添加中间帧。
开启 NR 不会自动开启帧生成。

## RTX Encore 带来的改进

RTX Encore 为 **Ampere（RTX 30）**自行开发 NR 性能选项，同时支持 RTX 40，并为 RTX 20 提供实验性支持。
这些工作覆盖 NVIDIA 引擎及我们集成的 OpenDLSS-NR：更快的处理、独立的 NR 工作分辨率，以及在下方悟空对比中
将 NR 时间降低 **48 %** 的**超快模式**。

| 改进 | 作用 |
| :--- | :--- |
| **三代显卡** | 在 DirectX 11、DirectX 12 和 Vulkan 游戏中支持 RTX 40、RTX 30，以及实验性的 RTX 20。 |
| **处理更快，画面一致** | NVIDIA 引擎在 RTX 20 和 RTX 30 上默认启用优化，本地参考测试中的输出一致。性能已在 RTX 30 上测量；真实 RTX 20 显卡的验证仍待完成。 |
| **Fast 精度** | NVIDIA 引擎在 RTX 30 上实测 NR 处理时间约减少 17 %，画面有轻微差异。Open 也有自己的 RTX 30 Fast 精度，默认关闭。 |
| **独立的 NR 分辨率** | NR 可在低于屏幕分辨率的尺寸上工作，同时 DLSS 保留自己的渲染分辨率和重建细节。Performance 和 Ultra Performance 减少 NR 工作量，无需降低 DLSS 设置。 |
| **更多 RTX 30 Open 性能设置** | 可选的 **More speed for more VRAM** 以额外显存换取更短的 NR 时间，画面不变；**Fast projection** 以轻微画面差异换取速度。见[性能设置](#rtx-30-性能设置)。 |
| **降低 Open 内存占用** | RTX 30 路径的本地测试中，Open NR 引擎分配的内存约减少 41–43 %，输出一致。见[内存占用](#open-内存占用)。 |
| **超快模式** | RTX Encore 为 Open 开发的 **Ultra-fast mode**：悟空对比中 NR 时间从 18.7 降至 9.8 ms，显示帧率从 73.8 提升至 94.9 FPS。见[超快模式](#超快模式)。 |
| **与帧生成配合** | NR 作用于游戏渲染的帧，不对生成帧执行 NR。 |
| **多遍处理，各有风格** | 一至四遍，每一遍可使用不同的画面风格。 |
| **HDR 游戏** | 支持 HDR，并提供亮度设置。 |
| **后台准备** | NR 准备中或缺少必要的游戏输入时，保留 DLSS 原本生成的画面。 |

需要额外显存的选项默认关闭。

## 要求

- NVIDIA 的 NR 文件 **`nvngx_dlssnr.dll` 310.8.0**。RTX Encore 不包含它：需要自行提供，并放在模组文件旁边，
  通常是游戏可执行文件所在的目录。其他版本均不接受。
- 游戏已开启 **DLSS 超分辨率或 DLAA**。NR 应用于其输出。
- RTX 40 或 RTX 30 显卡。RTX 20 支持是实验性的。

## 开启方法

1. 把 `nvngx_dlssnr.dll` 放在模组文件旁边。
2. 在游戏中开启 DLSS 或 DLAA。
3. 打开菜单（**Insert**），进入 **Image** 选项卡，开启 **Neural Rendering**。

NR 在后台准备：最初几帧可能保留 DLSS 原本的画面。菜单顶部的 **Neural Rendering** 标签显示 NR 是否实际运行，
以及未运行时的原因。

## 主要设置

| 设置 | 默认 | 作用 |
| :--- | :---: | :--- |
| **Resolution** | Render | NR 的工作尺寸：DLSS 渲染尺寸、完整输出尺寸，或输出的 Quality（67 %）、Balanced（58 %）、Performance（50 %）、Ultra Performance（33 %），也可自定义比例。**这是主要的性能调节手段**：尺寸越小越快。 |
| **Strength** | 100 % | 效果强度，0 至 200 %。 |
| **Style** | Default | 画面风格：Default、Natural 或 Cinematic。 |
| **Passes** | 1 | NR 可以对自己的输出再次处理，最多四遍。每遍都会消耗 GPU 时间和显存，更多遍不保证画面更好。 |
| **Precision** | Exact | NVIDIA 引擎的 RTX 20 和 RTX 30：Fast 减少 NR 时间，在 RTX 30 上实测约 17 %，画面有轻微差异。Open 有独立的 RTX 30 Exact/Fast 选项。需重启游戏生效。 |

### RTX 30 性能设置

**Image → Neural Rendering → Performance** 分组提供 RTX Encore 为降低 NR 开销开发的选项。
各项选项的取舍不同，收益百分比不能直接相加。

| 设置 | 引擎 | 默认 | 收益与取舍 |
| :--- | :--- | :---: | :--- |
| **Precision：Fast** | RTX 30 上的 NVIDIA 或 Open | Exact | 处理更快，画面略有差异。上述 17 % 测量针对 NVIDIA 引擎，不适用于 Open。需重启。 |
| **More speed for more VRAM** | RTX 30 上的 Open | 关闭 | NR 时间更短、画面不变；在 1440p、DLSS Balanced 下约多占 160 MB 显存。显存接近用满时请保持关闭。需重启。 |
| **Fast projection** | RTX 30 上的 Open | 开启 | 更快的处理，画面有轻微差异。需重启。 |
| **Avoid NR padding** | NVIDIA 或 Open | 关闭 | 允许 NR 使用略小的相邻尺寸，每个方向最多减少 2 %。可能改变 NR 画面，不改变 DLSS 渲染尺寸。实时生效。 |
| **NR frames in flight** | 支持 Reflex 的 DirectX 12 游戏中的 NVIDIA 引擎 | 0 | 将待完成的 NR 帧数限制为 1 或 2；帧生成启用时至少使用 2。实时生效，游戏内性能影响仍待验证。 |
| **Ultra-fast mode** | Open，一遍 NR | 关闭 | 在下方对比中，NR 处理时间约减半，但 NR 层更新较晚，运动时可能出现画面瑕疵。需重启。 |

比较性能时，请保持一遍处理、相同场景、输出分辨率和帧生成倍率。先将 **NR Resolution** 降至 Performance
或 Ultra Performance，同时保留你偏好的 DLSS 设置，再比较 Fast 精度。要试用超快模式，请选择 Open 和标有
**RTX 30** 的后端，开启 **Ultra-fast mode**，然后重启。一次只改一个选项；除了 FPS，还要检查运动中的
人物以及刚显露的表面。

**Ultra Performance** 是 NR 分辨率预设（输出宽度和高度的 33 %）。**Ultra-fast mode** 是 Open 超快模式。
两者可以一起使用，但选择其中一项不会自动开启另一项。

### 处理遍数和风格

多遍处理时，每遍可以有自己的风格：**Global**（跟随 **Style** 设置）、**Default**、**Natural** 或 **Cinematic**。
例如，Style 设为 Cinematic、第一遍设为 Natural、第二遍设为 Global，得到的顺序是 Natural，再 Cinematic。
**Tone after pass one**（默认 0）避免每一遍都重复叠加色调修正。

请先使用一遍，并将其作为比较基准。多遍处理尚未在游戏中验证。

### 微调

主要设置下方可展开的分组包含局部色调、局部结构、人物遮罩及其独立的皮肤结构设置、HDR 游戏中提供给 NR 的亮度，
以及 NVIDIA 模型配置（推荐 **Automatic**）。菜单中每项都有说明。

## 引擎

| 引擎 | 状态 | 说明 |
| :--- | :--- | :--- |
| **NVIDIA**（默认） | 实验性 | 运行 NVIDIA 的 NR。 |
| **Open** | 高度实验性 | NR 的开放实现（[OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR)）。可能出现崩溃、闪烁、画面瑕疵和兼容性问题。仍需要模组文件旁边的 `nvngx_dlssnr.dll`。 |

切换引擎需要重启游戏，不会自动从一个引擎切换到另一个。使用 **Open** 时，菜单要求选择适合显卡的后端以及运行方式，
各项都有说明。Open 不支持 RTX 20。其 RTX 30 路径和性能选项属于 RTX Encore 的 Ampere 优化工作；不保证与
NVIDIA 引擎生成相同画面。

## 性能开销

NR 增加每个渲染帧的 GPU 工作量。在 RTX 3070 Ti Laptop、2560×1440、DLSS 58 %、帧生成 X3 下，
《黑神话：悟空》内置基准测试测得：

| 配置 | 平均 FPS | 最低 5 % FPS |
| :--- | ---: | ---: |
| NR 关闭 | 146 | 127 |
| NR 开启，Precision Exact | 76 | 71 |
| NR 开启，Precision Fast | 82 | 76 |
| NR 开启，Precision Fast，Resolution Performance | 90 | 83 |

这些是使用 NVIDIA NR 引擎、**帧生成 X3 的显示 FPS**。在此测试中，从默认设置改为 Precision Fast 和
Resolution Performance，显示帧率提高 18 %（76 至 90 FPS）。这不是游戏 FPS 提高 17 % 的测量：17 % 指 NR
处理时间的减少。使用帧生成时，NR 作用于游戏渲染的帧，不作用于生成帧。

**Performance** 分组还有两项实验性选项：**Avoid NR padding**，以及 DirectX 12 游戏的 **NR frames in flight**。
菜单提供各自说明。最大的收益来自下方的超快模式。

## 超快模式

**Ultra-fast mode** 是 RTX Encore 为 Open 引擎开发的超快模式。它将 NR 工作分散到两个渲染帧中，
并在更新之间让 NR 层跟随场景。这通过不同的时间更新方式减少每帧 NR 工作量，而不只是降低分辨率。

在 **RTX 3070 Ti Laptop（Ampere）**、帧生成 X3 下，《黑神话：悟空》内置基准测试测得
（**每种配置各运行一次**）：

| 项目 | Open 引擎 | Open 引擎，超快模式 |
| :--- | ---: | ---: |
| 每个渲染帧的 NR 时间 | 18.7 ms | 9.8 ms |
| 显示帧率 | 73.8 FPS | 94.9 FPS |

在此对比中，**NR 时间减少 48 %**，**显示 FPS 增加 29 %**。该模式分散工作，避免完整 NR 开销的一帧和完全不执行 NR
工作的一帧交替出现。启动和历史重置时仍可能开销更高；此结果不保证所有游戏都有收益或平稳的帧时间。
这些已有测量不是新发布构建的基准测试；本页未记录确切的构建标识。

- **开启方法**：**Image** 选项卡选择 **Open** 引擎，然后在 **Performance** 分组开启 **Ultra-fast mode**。
  重启游戏。只支持一遍 NR，可用于 DirectX 11、DirectX 12 和 Vulkan 游戏；已在《黑神话：悟空》、Palworld 和
  Shadows of Doubt 中观察到运行。
- **画面变化**：NR 层的更新延后一帧。快速运动时可能滞后或拖影，刚出现的表面会稍后才得到 NR 效果。
- **两项实时调节**：**Ghost tolerance** 越低，运动后的重影越少，但更多表面暂时等待 NR；**Fill tolerance**
  控制刚出现的表面借用周围相似表面的 NR 效果的程度，0 关闭填充。
- 多占用几 MB 显存。

此模式属于 Open 引擎，该引擎仍是高度实验性的：见[引擎](#引擎)。

## Open 内存占用

我们的 Ampere 优化也降低了 Open 的内存占用。RTX 30 路径已有的本地自动化测试，在 **More speed for more VRAM** 关闭时，
记录了以下属于 NR 引擎的 GPU 内存分配：

| NR 工作尺寸 | 优化前 | 优化后 |
| :--- | ---: | ---: |
| 1472×828 | 601 MiB | 345 MiB |
| 1920×1080 | 846 MiB | 501 MiB |

即 **NR 内存约减少 41–43 %**，参考对比中的输出一致。这些数据统计 Open 引擎自身的分配，不是游戏的总显存占用；
它们是本地自动化测试结果，不是游戏内 FPS 测量，也不代表 NVIDIA 引擎或帧生成的内存使用。
开启 **More speed for more VRAM** 会再次增加显存占用，以换取更短的 NR 时间。

## 如果不起作用

| 现象 | 首先检查 |
| :--- | :--- |
| 无法开启开关 | 模组文件旁边缺少 `nvngx_dlssnr.dll`，或版本不是 310.8.0。 |
| 标签显示等待 DLSS | 在游戏中开启 DLSS 或 DLAA，并进入 3D 场景。 |
| 最初几帧没有 NR | NR 正在后台准备，这是正常现象。 |
| 画面保持 DLSS 原样 | 阅读菜单显示的原因：游戏没有提供 NR 所需输入时，画面保持不变。 |
| 设置立即恢复 | 设置文件未能保存：检查是否只读或被编辑器锁定。 |
| 帧率下降 | NR 开销很大。降低 **Resolution**，并在相同场景中比较。 |

报告问题时，请提供游戏及其图形 API、所选引擎，以及菜单报告（**System** 选项卡，**Copy**）。
