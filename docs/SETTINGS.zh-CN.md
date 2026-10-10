# 设置文件

*[English](SETTINGS.md) · [Français](SETTINGS.fr.md)*

所有设置都可以在[菜单](MENU-AND-OVERLAY.zh-CN.md)（**Insert**）中修改，这是最方便的方法。
本页供偏好直接编辑文件的用户参考。

## 文件

- 名称：`rtx-encore.jsonc`，位于模组文件旁边。首次启动时会创建，使用默认值。
- 格式：带 `//` 注释的 JSON。每个键都有说明其作用的注释。有些编辑器会提示注释错误，但模组能正常读取。
- 设置按分组组织。大部分实时生效；需要重启游戏时，注释会说明。
- 菜单自身的快捷键和布局保存在同一文件的 `menuState` 分组中。
- 旧版本留下的设置文件会保留原有设置并迁移：见[安装](INSTALLATION.zh-CN.md#从旧名称升级)。

设置键不包含路径或命令，因此可以直接将设置文件分享给其他人。

## 主要键

### `frameGeneration`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `mode` | `"game"` | `"game"`（由游戏或 NVIDIA Profile Inspector 决定）、`"fixed"`、`"dynamic"` |
| `multiplier` | `4` | `2` 至 `6`，用于固定模式。`5` 和 `6` 是实验性的。 |
| `dynamicTargetFrameRate` | `0` | 动态模式的目标 FPS。`0` 跟随显示游戏的显示器的刷新率。 |
| `dynamicExperimental56` | `false` | 允许动态模式达到 X5 和 X6。 |

### `dlssSuperResolution`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `dlssRenderScale` | `"game"` | `"game"`、`"dlaa"`、`"quality"`、`"balanced"`、`"performance"`、`"ultra-performance"`、`"custom"` |
| `dlssCustomScale` | `67` | `"custom"` 时的百分比，`50` 至 `100` |

### `neuralRendering`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `nrEnabled` | `false` | 开启 Neural Rendering。 |
| `nrEngine` | `"nvidia"` | `"nvidia"` 或 `"opendlss"`（高度实验性）。需重启。 |
| `nrPasses` | `1` | `1` 至 `4` |
| `nrLaterPassLocalTone` | `0` | 第一遍之后应用的色调，`0` 至 `2` |
| `nrResolution` | `"render"` | `"render"`、`"output"`、`"quality"`、`"balanced"`、`"performance"`、`"ultra-performance"`、`"custom"` |
| `nrResolutionScale` | `67` | `"custom"` 时相对于输出尺寸的百分比，`33` 至 `100` |
| `nrIntensity` | `1` | 强度，`0` 至 `2` |
| `nrStyle` | `0` | `0` 默认、`1` 自然、`2` 电影风格 |
| `nrPass1Style` … `nrPass4Style` | `"inherit"` | `"inherit"`（跟随 `nrStyle`）、`"default"`、`"natural"`、`"cinematic"` |
| `nrLocalTone`、`nrLocalStructure` | `1` | `0` 至 `2` |
| `nrAutoMask` | `false` | 人物遮罩；`nrSkinStructure` 需要它 |
| `nrSkinStructure` | `1` | `0` 至 `2` |
| `nrHdrExposure` | `1` | HDR 游戏：提供给 NR 的亮度 |
| `nrPrecision` | `"exact"` | `"exact"` 或 `"fast"`（RTX 20 和 RTX 30）。需重启。 |
| `nrPreset` | `0` | NVIDIA 模型配置：`0` 自动（推荐），`1` 至 `3` |

#### NR 性能选项

这些设置也在 **Image → Neural Rendering → Performance** 分组中。
测量结果和画面取舍见 [Neural Rendering](NEURAL-RENDERING.zh-CN.md#rtx-30-性能设置)。
引擎专用选项仅作用于下表所列引擎；切换引擎或修改启动时读取的选项，需要重启游戏。

| 键 | 默认 | 收益与取舍 |
| :--- | :---: | :--- |
| `nrPaddingAware` | `false` | 允许 NR 使用略小的相邻尺寸，每个方向最多减少 2 %，不改变 DLSS 渲染尺寸。可能改变画面。实时生效。 |
| `nrMaxInFlight` | `0` | 支持 Reflex 的 DirectX 12 游戏中的 NVIDIA 引擎：`0` 不限制，`1` 或 `2` 个待完成的 NR 帧；帧生成启用时至少使用 `2`。实时生效，游戏内性能影响仍待验证。 |

### `openExperimental`

Open 的性能键位于此独立分组。请先在菜单中选择引擎和适合显卡的后端；这些键本身不会开启 Open。

| 键 | 默认 | 收益与取舍 |
| :--- | :---: | :--- |
| `nrOpenFast` | `false` | RTX 30 上的 Open：Fast 精度，NR 时间更短，画面有轻微差异，不增加显存。需重启。 |
| `nrOpenVramForSpeed` | `false` | RTX 30 上的 Open：NR 时间更短，画面一致；在 1440p、DLSS Balanced 下约多占 160 MB 显存。需重启。 |
| `nrOpenFastProjection` | `true` | RTX 30 上的 Open：处理更快，画面有轻微差异。需重启。 |
| `nrOpenUltraFast` | `false` | Open，一遍 NR：**Ultra-fast mode**，在文档中的对比里 NR 时间约减半。NR 层更新较晚，运动时可能出现瑕疵，多占几 MB 显存。需重启。 |
| `nrOpenUltraFastGhostTolerance` | `0.03` | `0` 至 `0.3`；越低，重影越少，但更多表面暂时等待 NR。实时生效。 |
| `nrOpenUltraFastFillTolerance` | `0.03` | `0` 至 `0.3`；控制刚出现的表面用相似表面的 NR 效果填充的程度。`0` 关闭填充。实时生效。 |

`nrResolution: "ultra-performance"` 将 NR 设置为输出宽度和高度的 33 %。它与 `nrOpenUltraFast` 和
`dlssRenderScale` 相互独立，这些设置不会自动开启彼此。

### `overlay`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `showOverlay` | `false` | 显示叠加层。 |
| `overlayPosition` | `"top-left"` | `"top-left"`、`"top-right"`、`"bottom-left"`、`"bottom-right"` |
| `overlayFontSize` | `14` | 文字大小，`10` 至 `32` 像素 |
| `overlayShowNr`、`overlayShowFramePacing`、`overlayShowGpu`、`overlayShowVram`、`overlayShowVersions` | `false` | 可选显示项目 |

### `hudUi`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `autoUiRecomposition` | `true` | 游戏提供所需内容时，使用界面单独处理 |
| `uiAssist` | `true` | DirectX 12：游戏未提供无界面场景和界面层时，由模组构建 |
| `forceUiRecomposition` | `false` | 即使游戏没有要求，也请求单独处理 |

### `nativeMenuFeatures`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `reflexFrameLimit` | `0` | `0` 跟随游戏；`1` 至 `1000` 限制渲染帧率 |
| `vsyncOff` | `false` | 请求 DirectX 游戏关闭 V-Sync |
| `hairEnabled` | `true` | 在具备该功能的游戏中启用兼容的光线追踪毛发。需重启。 |

### `diagnostics`

| 键 | 默认 | 值 |
| :--- | :---: | :--- |
| `logPerformance` | `false` | 将帧率和帧时间写入 `rtx-encore-logs` 中的 CSV 文件 |
| `logFilesKept` | `3` | 保留的常规日志数及同等数量的异常日志，`1` 至 `100` |

### `keyboardShortcuts`

每个 `hotkey…` 键可以接受一个或多个用逗号分隔的组合键，例如
`"hotkeyFixed4": "Ctrl+Alt+4, Ctrl+Alt+Num4"`。空字符串 `""` 停用该操作。
`"disableKeybinds": true` 停用全部快捷键。

## 其他键

文件中的键比本页列出的更多，包括 Smooth Motion、画质和兼容性开关，以及 Open 引擎的实验性选项。
每个键在文件中有注释，在菜单中有对应的设置；推荐从菜单修改。
