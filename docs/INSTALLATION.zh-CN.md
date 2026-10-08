# 安装

*[English](INSTALLATION.md) · [Français](INSTALLATION.fr.md)*

## 所需条件

- 64 位 Windows 10 或 11。
- GeForce RTX 20、RTX 30 或 RTX 40 显卡，并安装较新的 NVIDIA 驱动。
- 帧生成：游戏的设置中必须提供 **DLSS 帧生成（DLSS Frame Generation）**。
- Smooth Motion 和 Neural Rendering 请见各自的页面：
  [Smooth Motion](SMOOTH-MOTION.zh-CN.md)、[Neural Rendering](NEURAL-RENDERING.zh-CN.md)。

> [!WARNING]
> RTX Encore 在游戏进程内加载。请勿安装到联机游戏或受反作弊保护的游戏中：它可能被拦截，或导致账号被标记。

## 发布包内容

| 文件 | 用途 |
| :--- | :--- |
| `rtx-encore.dll` | 模组本体。复制到游戏旁边，并改成游戏会加载的文件名（见下文）。 |
| `alternative-proxies\rtx-encore.asi` | 同一个模组的插件形式，适用于带有 ASI 加载器的游戏。 |
| `alternative-proxies\version.dll`、`winmm.dll`、`dxgi.dll`、`dinput8.dll` | 已命名好的副本，用于少数不接受改名后主文件的游戏。 |
| `cyberpunk-2077-cet-panel\` | Cyber Engine Tweaks 的可选面板（见 [Cyberpunk 2077](#cyberpunk-2077)）。 |
| `README.md`、`docs\`、`THIRD-PARTY-NOTICES.md` | 本文档和第三方组件声明。 |

每个游戏只安装**一份**模组，切勿安装两份。

## 安装步骤

1. 找到真正运行游戏的可执行文件所在的文件夹。对于 Unreal Engine 游戏，它是 `<游戏>\Binaries\Win64\`，
   即 `<游戏>-Win64-Shipping.exe` 所在的位置，而不是根目录下的小启动器。
2. 把 `rtx-encore.dll` 复制到那里并重命名。请按以下顺序尝试文件名：

   | 文件名 | 适用情况 |
   | :--- | :--- |
   | `version.dll` | 首选。适用于大多数游戏，包括 Unreal Engine 4 和 5。 |
   | `dinput8.dll` | 当 `version.dll` 没有被加载，或已被其他模组占用时。 |
   | `winmm.dll` | 同上，另一种途径。 |
   | `dxgi.dll` | 只加载这个文件的游戏。不要替换属于 ReShade、Special K 或 DXVK 的 `dxgi.dll`。 |

   该文件也可以命名为 `d3d9.dll`、`d3d10.dll`、`d3d11.dll`、`d3d12.dll`、`dsound.dll`、`wininet.dll`、
   `winhttp.dll`、`binkw64.dll`、`bink2w64.dll`、`xinput1_1.dll`、`xinput1_2.dll`、`xinput1_3.dll`、
   `xinput1_4.dll`、`xinput9_1_0.dll` 和 `xinputuap.dll`。文件名只决定游戏如何加载模组，并不会增加对其他图形
   API 的支持。使用 Bink 文件名时，请把游戏原有的文件留在旁边，并改名为 `binkw64Hooked.dll` 或
   `bink2w64Hooked.dll`。
3. 如果某个文件名已被其他模组占用，请保留它的文件，为 RTX Encore 另选一个文件名。
4. 启动游戏。首次启动时菜单会自动打开一次；按 **Insert** 可再次打开。
5. 在游戏设置中开启 **DLSS 帧生成**。如果之前已经开启，请先关闭再重新开启，或重启游戏。

### 作为 ASI 插件

如果游戏带有 ASI 加载器（Ultimate ASI Loader、Cyber Engine Tweaks 自带的加载器等），请把
`alternative-proxies\rtx-encore.asi` 复制到该加载器读取的文件夹（通常是 `plugins` 或 `scripts`），
而不是重命名 DLL。

### 模组旁边会出现的文件

- `rtx-encore.jsonc`：你的设置，首次启动时创建。见[设置文件](SETTINGS.zh-CN.md)。
- `rtx-encore-logs\`：会话日志，默认保留最近三份。

## 更新

用新的 `rtx-encore.dll` 替换游戏加载的那个文件，**并保持它原有的文件名**（`version.dll`、`dinput8.dll` 等），
或替换 `rtx-encore.asi`。你的 `rtx-encore.jsonc` 会被保留：不要删除它。

## 从旧名称升级

RTX Encore 最初以 DLSSG-Transfusion 的名称发布（直到 `v1.4.5.3-rtx20-30-40`），开发版本曾一度叫做
RTX Unlocker。

- 用新文件替换游戏文件夹中的旧文件并保持其文件名，或者删除旧文件后重新安装。切勿同时保留两份。
- 你的设置会被保留：`DLSSG-Transfusion.json`、`RTX-Unlocker.jsonc` 或 `RTX-Unlocker.json` 会在首次启动时被
  重命名为 `rtx-encore.jsonc`，其中的值保持不变。
- 插件的文件名变了。如果 `DLSSG-Transfusion.asi` 仍然留在 `rtx-encore.asi` 旁边，模组会发现它：旧文件会最后
  运行一次，然后被重命名为 `DLSSG-Transfusion.asi.replaced`，从下一次启动起只加载 `rtx-encore.asi`。
  `.replaced` 文件可以删除。
- 旧版本的 ReShade 插件（`DLSSG-Transfusion.addon64`）已不再使用：菜单现在是内置的。请把它从游戏文件夹中删除。
- 留在模组旁边的旧日志会被移入 `rtx-encore-logs`。

## 卸载

删除你添加的文件（`version.dll` 或你选择的文件名，或 `rtx-encore.asi`）。也可以删除 `rtx-encore.jsonc` 和
`rtx-encore-logs` 文件夹。模组绝不会修改磁盘上的任何游戏文件或驱动文件。

## 游戏说明

### Unreal Engine 4 和 5

把文件放在 `<游戏>\Binaries\Win64\` 下的 `<游戏>-Win64-Shipping.exe` 旁边，然后在游戏中开启 DLSS 和帧生成。

有些 Unreal Engine 游戏内置了 DLSS，却没有在菜单中提供。通常可以通过游戏的 `Engine.ini` 开启：

- Unreal Engine 5：`%LOCALAPPDATA%\<项目>\Saved\Config\Windows\Engine.ini`
- Unreal Engine 4：`%LOCALAPPDATA%\<项目>\Saved\Config\WindowsNoEditor\Engine.ini`

`<项目>` 是引擎中的项目名称，可能与游戏名称不同。先启动一次游戏让该文件生成，关闭游戏，备份文件，然后添加：

```ini
[SystemSettings]
r.NGX.Enable=1
r.NGX.DLSS.Enable=1
r.TemporalAA.Upsampling=1
r.ScreenPercentage=67

[/Script/DLSS.DLSSSettings]
bEnableDLSSD3D12=True
```

`r.ScreenPercentage` 是渲染分辨率：100 为 DLAA，67 为质量，58 为平衡，50 为性能，33 为超级性能。
如果游戏在启动时会重写 `Engine.ini`，请在修改后把该文件设为只读。

### Cyberpunk 2077

可以使用 `bin\x64\version.dll`，也可以配合 Cyber Engine Tweaks 自带的 ASI 加载器使用
`bin\x64\plugins\rtx-encore.asi`。请让其他模组保留它们各自需要的文件名：Cyber Engine Tweaks 使用
`version.dll`，RED4ext 使用 `winmm.dll`，ReShade 使用 `dxgi.dll`。

菜单（**Insert**）可以独立工作。如果你使用 Cyber Engine Tweaks，还有一个可选面板会在它的界面中显示主要控制项：
把 `cyberpunk-2077-cet-panel` 中的 `bin` 文件夹复制到游戏文件夹即可。该面板是实验性的，并且需要安装 `.asi`
插件。

### DXVK、ReShade、Special K、OptiScaler

- 把 `dxgi.dll` 留给提供它的工具，为 RTX Encore 另选一个文件名。
- 使用 DXVK 时，菜单会附着在 Vulkan 的画面输出上。
- 使用 OptiScaler 时，请参阅[帧生成](FRAME-GENERATION.zh-CN.md#兼容性选项)中的 flip metering 选项。
