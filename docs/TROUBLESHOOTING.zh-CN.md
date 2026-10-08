# 故障排除

*[English](TROUBLESHOOTING.md) · [Français](TROUBLESHOOTING.fr.md)*

## 首先检查

1. **模组已加载吗？** 加载后，模组文件旁边会出现 `rtx-encore-logs` 文件夹，**Insert** 会打开菜单。
   两者都没有出现时，游戏没有加载文件。
2. **是否只有一份？** 在同一游戏中以两个名称安装两份模组，会造成崩溃和异常行为。
3. **菜单显示什么？** 顶部四个标签显示各功能状态；持续存在的问题会显示在卡片中，并附处理方法。

## 模组没有加载

| 检查项 | 处理方法 |
| :--- | :--- |
| 文件夹不正确 | 文件必须放在实际运行游戏的可执行文件旁边。Unreal Engine 游戏通常是 `<Game>\Binaries\Win64\`。 |
| 游戏不加载此名称 | 依次尝试 `version.dll`、`dinput8.dll`、`winmm.dll`、`dxgi.dll`。 |
| 名称被其他模组占用 | 保留该模组的文件，为 RTX Encore 选择其他名称。 |
| 改名后的文件被拒绝 | 使用 `alternative-proxies` 中已命名的副本。 |
| 游戏有反作弊 | 它可能阻止加载。请勿在这些游戏中使用 RTX Encore。 |

## 帧生成

| 现象 | 首先检查 |
| :--- | :--- |
| 游戏没有 Frame Generation 选项 | RTX Encore 扩展游戏已有的 DLSS 帧生成，不能凭空添加。其他游戏可参阅 [Smooth Motion](SMOOTH-MOTION.zh-CN.md)。 |
| 有选项，但呈灰色 | 检查 DLSS 是否开启、游戏是否使用 DirectX 12 或 Vulkan，以及模组是否加载。 |
| 倍率停留在 X2 | 在游戏中关闭再开启帧生成，或重启。检查菜单模式：Game decides 模式由游戏决定。 |
| Dynamic 模式达不到 X5 或 X6 | 开启 **Allow 5x and 6x**。 |
| 此游戏版本没有任何变化 | 游戏的 `nvngx_dlssg.dll` 可能是不支持的版本：见[兼容性](COMPATIBILITY.zh-CN.md#dlss-帧生成版本)。 |
| 运动时有双重轮廓或扭曲帧 | 关闭 **Automatic UI recomposition**。 |
| 栅栏、植被或阴影出现瑕疵 | 保持 **Anti-tearing / anti-ghosting** 开启，并尝试另一种 **Protection tuning**。 |
| 高倍率下卡顿或崩溃 | 显存可能已满：降低倍率或纹理质量。 |
| 在菜单或加载画面中崩溃 | 保持 **Disable menu detection** 关闭。 |

## Smooth Motion 和 Neural Rendering

参阅各自页面末尾：[Smooth Motion](SMOOTH-MOTION.zh-CN.md#如果不起作用)、
[Neural Rendering](NEURAL-RENDERING.zh-CN.md#如果不起作用)。

## 菜单和设置

| 现象 | 首先检查 |
| :--- | :--- |
| **Insert** 无反应 | 模组未加载（见上文），或快捷键已修改：检查 `rtx-encore.jsonc` 的 `menuState`。 |
| 菜单在独立窗口中打开 | 少数游戏中这是预期行为：见[菜单和叠加层](MENU-AND-OVERLAY.zh-CN.md#菜单在独立窗口中打开时)。 |
| 点击无效 | 松开所有鼠标按钮后再点击。无边框窗口模式可能有帮助。 |
| 设置立即恢复 | 设置文件未能保存：确认文件不是只读，也未被编辑器锁定。 |
| 设置没有效果 | 标有 `*` 的设置在游戏启动时读取：请重启游戏。 |
| 快捷键无反应 | 仅在游戏窗口获得焦点时生效，并且 `disableKeybinds` 必须为 `false`。 |

## 报告问题

请提交 [issue](https://github.com/SilyNoMeta/rtx-encore/issues)，并附上：

- 游戏、版本或商店，以及图形 API；
- 显卡和 NVIDIA 驱动版本；
- RTX Encore 版本和你使用的文件名；
- 菜单中复制的报告（**System** 选项卡，**Copy**）；
- `rtx-encore-logs` 中出现问题的会话日志。日志可能包含电脑上的文件夹名称：发布前请先检查。

每个 issue 报告一个问题，说明操作步骤和观察结果。
