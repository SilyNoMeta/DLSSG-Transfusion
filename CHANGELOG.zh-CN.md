# 更新日志

*[English](CHANGELOG.md) · [Français](CHANGELOG.fr.md)*

## 1.0.0-beta.3 — 预发布版本

- 修正 The Last of Us 中 RTX Encore、OptiScaler 与游戏旧版 Streamline 同时使用时发现的启动崩溃原因。尚待游戏内确认；Dead Space 的情况仍未确认。
- 正确检测 DLSS Ray Reconstruction 活动。NR 缺少兼容深度输入时会说明原因，不再掩盖 DLSS 活动；兼容性仍取决于游戏提供的数据。
- 为每个进程和启动分别保存异常日志，避免错误报告程序覆盖游戏的诊断数据。这有助于调查 Cyberpunk 的启动崩溃，但不代表该崩溃已修复。

- 修复启用内置菜单时，在 Starfield 中开启帧生成导致的崩溃。
  初始测试版本已在游戏中确认正常，包括关闭/重新开启帧生成以及打开菜单。
  最终变体还修正了对象生命周期和诊断功能，并通过了 Windows 自动化测试；尚未对该变体单独进行游戏内复测。
- 限制可选图形跟踪的日志数量，并避免记录已加载模块时产生诊断异常。

### 致谢

- 感谢 [@mennogreg](https://github.com/mennogreg) 提供 Starfield 的问题报告、日志和多次游戏内测试（[#13](https://github.com/SilyNoMeta/rtx-encore/issues/13)），以及 OptiScaler 兼容性问题的报告和日志（[#20](https://github.com/SilyNoMeta/rtx-encore/issues/20)）。
- 感谢 [@hunan-NRT](https://github.com/hunan-NRT) 报告 Ray Reconstruction 检测问题，并感谢 @mennogreg 确认同类现象（[#14](https://github.com/SilyNoMeta/rtx-encore/issues/14)）。
- 感谢 [@naruto490610-alt](https://github.com/naruto490610-alt) 提供 Cyberpunk 报告和日志，帮助发现异常日志缺少进程隔离的问题（[#17](https://github.com/SilyNoMeta/rtx-encore/issues/17)）。

也感谢所有分享问题报告和日志、帮助调查其余问题的用户。

## 1.0.0-beta.2 — 预发布版本

以 **RTX Encore** 名称发布的首个版本。版本号重新从 1.0.0 开始：此版本接替并取代以 DLSSG-Transfusion
名称发布的 `v1.4.5.3-rtx20-30-40`。

### 新增

- **内置菜单和叠加层。** 游戏中按 **Insert** 即可访问全部设置、各功能状态，以及显示帧率、倍率、帧时间稳定性、
  GPU 和显存的叠加层。支持 DirectX 11、DirectX 12 和 Vulkan 游戏。不再需要 ReShade，也不再提供以前的
  ReShade 附加组件。
- **Neural Rendering** 在 RTX 20、RTX 30 和 RTX 40 上作用于 DLSS 输出：一至四遍处理，每遍可选风格，支持
  HDR，可选择 NVIDIA 引擎或实验性的 Open 引擎。实验性功能，默认关闭；需要 NVIDIA 的 `nvngx_dlssnr.dll`
  310.8.0。Open 不支持 RTX 20。
- **我们针对 Ampere（RTX 30）的 NR 性能工作。** NVIDIA 引擎在本地参考测试中保持输出一致的加速处理；NVIDIA
  Fast 精度在 RTX 30 上实测 NR 时间约减少 17 %；独立的 NR 分辨率，包括 Ultra Performance；以及 Open 的
  Fast 精度和可选的速度/显存设置。我们的 **Ultra-fast mode** 超快模式，在 RTX 3070 Ti Laptop、FG X3
  的悟空基准测试中，将 NR 时间从 **18.7 降至 9.8 ms（−48 %）**，显示帧率从 **73.8 提升至 94.9 FPS（+29 %）**，
  每种配置各运行一次。只支持一遍 NR，运动时可能出现瑕疵，收益不保证。[测量结果与取舍](docs/NEURAL-RENDERING.zh-CN.md)。
- **RTX 30 上的 Smooth Motion** 在原有 617.14 之外，新增接受 NVIDIA 驱动 617.42、616.92 和 616.64，
  并可从菜单设置。只有 617.14 已在游戏中验证。
- **一个通用文件。** `rtx-encore.dll` 现在接受十九个文件名，便于与占用常见名称的其他模组共存。
- **新控制项**：通过 NVIDIA Reflex 限制渲染帧率、请求关闭 V-Sync，以及 The Witcher 3 中兼容的光线追踪毛发。
- Cyberpunk 2077 中 **Cyber Engine Tweaks 的可选面板**（实验性）。
- **会话日志**保存在 `rtx-encore-logs` 文件夹中，默认保留最近三份。

### 变更

- 文件改名为 `rtx-encore.*`。设置、快捷键和以前的插件会自动迁移：见[从旧名称升级](docs/INSTALLATION.zh-CN.md#从旧名称升级)。
- 设置文件为 `rtx-encore.jsonc`，按分组组织，每个键附有注释。
- 若干设置和菜单项改用了更清晰的名称。旧版本的设置文件会连同其中的值一并沿用。
- RTX 40 显卡上的帧生成 GPU 时间减少 2 至 3 %。
- 叠加层文字更紧凑，并可设置大小。

### 已知限制

- RTX 20 支持是实验性的，尚未在真实 RTX 20 显卡上验证。
- 多遍 Neural Rendering、Open 引擎、Cyber Engine Tweaks 面板，以及 RTX 20 和 RTX 30 上的光线追踪毛发，
  游戏内测试仍然有限或尚未进行。
- X5 和 X6 是实验性的，需要充足显存。

## 以前的版本

截至 `v1.4.5.3-rtx20-30-40` 的版本以 DLSSG-Transfusion 名称发布，现已不再提供。
