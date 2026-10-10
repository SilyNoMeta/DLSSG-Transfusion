# 菜单和叠加层

*[English](MENU-AND-OVERLAY.md) · [Français](MENU-AND-OVERLAY.fr.md)*

RTX Encore 在 DirectX 11、DirectX 12 和 Vulkan 游戏中绘制自己的菜单和叠加层，无需安装其他软件。

## 菜单

在游戏窗口中按 **Insert** 打开或关闭菜单。首次启动时菜单会自动打开一次。快捷键可以在菜单中修改。

每个页面顶部都有四个状态标签：**Upscaling**、**Frame Generation**、**Smooth Motion** 和 **Neural Rendering**。
每个标签通过彩色圆点和一个数值显示相应功能的状态；悬停查看详情，点击进入设置。持续存在的问题会显示在标签下方的
卡片中，并给出处理方法。

| 选项卡 | 内容 |
| :--- | :--- |
| **Frames** | 帧生成及其画质和界面选项、Smooth Motion、渲染帧率限制、V-Sync |
| **Image** | DLSS 渲染分辨率、Neural Rendering |
| **Overlay** | 叠加层预览、位置、文字大小和显示项目 |
| **System** | 快捷键、兼容性、诊断和会话详情，以及用于报告问题的 **Copy** 按钮 |

使用提示：

- 每次修改都会立即保存到 `rtx-encore.jsonc`，保留文件中的注释。
- 大部分设置实时生效。名称后的 `*` 表示该设置在游戏启动时读取；修改后变为橙色，菜单底部会列出等待重启的修改。
- 已修改的设置名称更亮，并带有重置按钮。
- 每部分显示主要设置，其余设置位于可展开的分组中，例如 **Fine tuning**、**Performance** 和 **Diagnostics**。
- 拖动标题栏移动菜单，拖动右下角调整大小。位置和尺寸会被记住。
- 打开菜单时如果仍按着鼠标按钮，请先松开再点击。

### 菜单在独立窗口中打开时

少数游戏无法安全地在画面内绘制菜单。此时设置会在独立窗口中打开，叠加层则变为游戏上方的一小块可穿透点击的文字。
建议使用无边框窗口模式。设置及其效果相同。

## 叠加层

在 **Overlay** 选项卡中开启，或按 `Ctrl + Alt + O`。`Ctrl + Alt + P` 将其移至下一个角落。

| 示例 | 含义 |
| :--- | :--- |
| `74 fps` | 游戏帧率；未观察到帧生成 |
| `74/37 fps 2x` | 显示帧率 / 渲染帧率，以及观察到的倍率 |
| `148/74 fps sm` | 使用 Smooth Motion 的估算输出 / 游戏帧率 |
| `296/74 fps 2x+sm` | 帧生成 X2，再加 Smooth Motion |

可选项目均默认关闭：

| 项目 | 显示内容 |
| :--- | :--- |
| Neural Rendering | 状态和工作尺寸 |
| Frame pacing | 帧时间：平均值、第 99 百分位和抖动 |
| GPU | 负载、温度、功耗和频率 |
| VRAM | 显卡已用 / 总显存，包含所有程序 |
| Versions | 游戏加载的 DLSS、DLSS 帧生成和 Streamline 版本 |
| Interface | 是否启用界面单独处理，以及无界面场景和界面层的来源 |

同一选项卡还可以设置位置（四个角落）和文字大小（10 至 32 px）。

## 其他控制

| 设置 | 选项卡 | 作用 |
| :--- | :--- | :--- |
| **Render resolution** | Image | 指定游戏在 DLSS 处理前的渲染分辨率：DLAA、Quality、Balanced、Performance、Ultra Performance，或 50 至 100 % 的自定义比例。游戏必须已使用 DLSS。菜单显示实际观察到的分辨率，以及游戏是否遵从请求；出现问题时改回 Game。Unreal Engine 4 和 5 游戏会在可行时实时调整分辨率比例。 |
| **Render-frame limit (FPS)** | Frames | 游戏支持 NVIDIA Reflex 时，通过它限制游戏渲染的帧率。0 跟随游戏。不计生成帧，在 Dynamic 模式下暂停。 |
| **Request V-Sync off** | Frames | 请求 DirectX 游戏关闭 V-Sync。NVIDIA 控制面板强制设置的优先级更高。对 Vulkan 游戏无效。 |
| **Compatible ray-traced hair** | System | The Witcher 3（DirectX 12，游戏构建 25575366 和 25646871）：使游戏的光线追踪毛发可用于 RTX 20、30 和 40。游戏自身的毛发设置仍决定是否渲染毛发。尚未在 RTX 20 和 RTX 30 上进行游戏内验证。需重启游戏。 |

## 快捷键

默认快捷键见[帧生成](FRAME-GENERATION.zh-CN.md#快捷键)。在 **System** 选项卡中，每个操作可以绑定多个组合键，
也可以不绑定。快捷键仅在游戏窗口获得焦点时生效。

## 日志

会话日志写入模组文件旁边的 `rtx-encore-logs` 文件夹；默认保留最近三份（**Session logs kept**，1 至 100）。
**Log performance** 将本次会话的帧率和帧时间写入同一文件夹中的 CSV 文件。

异常日志单独保留，数量与常规日志相同。每个进程使用独立文件；仍在使用的日志不会删除。
