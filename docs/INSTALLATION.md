# Installation

*[Français](INSTALLATION.fr.md) · [中文](INSTALLATION.zh-CN.md)*

## What you need

- Windows 10 or 11, 64-bit.
- A GeForce RTX 20, RTX 30 or RTX 40 card with a recent NVIDIA driver.
- For frame generation: a game that offers **DLSS Frame Generation** in its settings.
- For Smooth Motion and Neural Rendering, see their own pages:
  [Smooth Motion](SMOOTH-MOTION.md), [Neural Rendering](NEURAL-RENDERING.md).

> [!WARNING]
> RTX Encore loads inside the game. Do not install it in online or anti-cheat protected games: it can be blocked,
> or get an account flagged.

## What the release contains

| File | Use |
| :--- | :--- |
| `rtx-encore.dll` | The mod. Copy it next to the game and give it the name the game loads (below). |
| `alternative-proxies\rtx-encore.asi` | The same mod as a plugin, for games that have an ASI loader. |
| `alternative-proxies\version.dll`, `winmm.dll`, `dxgi.dll`, `dinput8.dll` | Ready-named copies, for the rare game that does not accept the renamed main file. |
| `cyberpunk-2077-cet-panel\` | Optional panel for Cyber Engine Tweaks (see [Cyberpunk 2077](#cyberpunk-2077)). |
| `README.md`, `docs\`, `THIRD-PARTY-NOTICES.md` | This documentation and the third-party notices. |

Install **one** copy of the mod per game, never two.

## Install

1. Find the folder of the executable that really runs the game. For an Unreal Engine game it is
   `<Game>\Binaries\Win64\`, next to `<Game>-Win64-Shipping.exe`, not the small launcher at the root.
2. Copy `rtx-encore.dll` there and rename it. Try the names in this order:

   | Name | When |
   | :--- | :--- |
   | `version.dll` | First choice. Works in most games, including Unreal Engine 4 and 5. |
   | `dinput8.dll` | When `version.dll` is not loaded or is already used by another mod. |
   | `winmm.dll` | Same, another route. |
   | `dxgi.dll` | Games that only load this one. Do not replace a `dxgi.dll` that belongs to ReShade, Special K or DXVK. |

   The file also works as `d3d9.dll`, `d3d10.dll`, `d3d11.dll`, `d3d12.dll`, `dsound.dll`, `wininet.dll`,
   `winhttp.dll`, `binkw64.dll`, `bink2w64.dll`, `xinput1_1.dll`, `xinput1_2.dll`, `xinput1_3.dll`, `xinput1_4.dll`,
   `xinput9_1_0.dll` and `xinputuap.dll`. The name only decides how the game loads the mod; it adds no support for
   another graphics API. With a Bink name, keep the game's own file beside it, renamed `binkw64Hooked.dll` or
   `bink2w64Hooked.dll`.
3. If another mod already uses a name, leave it its file and pick another name for RTX Encore.
4. Start the game. On first launch the menu opens once; press **Insert** to open it again.
5. Turn **DLSS Frame Generation** on in the game's settings. If it was already on, turn it off and on again, or
   restart the game.

### As an ASI plugin

If the game has an ASI loader (Ultimate ASI Loader, the one that comes with Cyber Engine Tweaks, ...), copy
`alternative-proxies\rtx-encore.asi` into the folder that loader reads, usually `plugins` or `scripts`, instead of
renaming the DLL.

### What appears next to the mod

- `rtx-encore.jsonc`: your settings, created on first launch. See [Settings file](SETTINGS.md).
- `rtx-encore-logs\`: the session logs, the three most recent by default.

## Update

Replace the file the game loads with the new `rtx-encore.dll`, **under the name it already has** (`version.dll`,
`dinput8.dll`, ...), or replace `rtx-encore.asi`. Your `rtx-encore.jsonc` is kept: do not delete it.

## Upgrading from an earlier name

RTX Encore was first published as DLSSG-Transfusion (releases up to `v1.4.5.3-rtx20-30-40`), and development builds
were called RTX Unlocker for a while.

- Replace the old file under the name it has in the game folder, or delete it and install the new one. Never leave
  two copies.
- Your settings are kept: `DLSSG-Transfusion.json`, `RTX-Unlocker.jsonc` or `RTX-Unlocker.json` is renamed
  `rtx-encore.jsonc` on first launch, with its values.
- The plugin changed names. If `DLSSG-Transfusion.asi` is still beside `rtx-encore.asi`, the mod notices it: the old
  file runs one last time, is renamed `DLSSG-Transfusion.asi.replaced`, and only `rtx-encore.asi` loads from the next
  launch on. You can delete the `.replaced` file.
- The ReShade add-on of the earlier releases (`DLSSG-Transfusion.addon64`) is no longer used: the menu is built in.
  Remove it from the game folder.
- Older logs left beside the mod are moved into `rtx-encore-logs`.

## Remove

Delete the file you added (`version.dll` or the name you chose, or `rtx-encore.asi`). You can also delete
`rtx-encore.jsonc` and the `rtx-encore-logs` folder. The mod never modifies a game or driver file on disk.

## Game notes

### Unreal Engine 4 and 5

Place the file next to `<Game>-Win64-Shipping.exe`, under `<Game>\Binaries\Win64\`, then turn DLSS and Frame
Generation on in the game.

Some Unreal Engine games include DLSS without offering it in their menus. It can often be turned on from the game's
`Engine.ini`:

- Unreal Engine 5: `%LOCALAPPDATA%\<Project>\Saved\Config\Windows\Engine.ini`
- Unreal Engine 4: `%LOCALAPPDATA%\<Project>\Saved\Config\WindowsNoEditor\Engine.ini`

`<Project>` is the engine's project name, which can differ from the game's title. Start the game once so the file
exists, close it, make a backup, then add:

```ini
[SystemSettings]
r.NGX.Enable=1
r.NGX.DLSS.Enable=1
r.TemporalAA.Upsampling=1
r.ScreenPercentage=67

[/Script/DLSS.DLSSSettings]
bEnableDLSSD3D12=True
```

`r.ScreenPercentage` is the render resolution: 100 for DLAA, 67 Quality, 58 Balanced, 50 Performance, 33 Ultra
Performance. If the game rewrites `Engine.ini` at startup, mark the file read-only after editing it.

### Cyberpunk 2077

Use either `bin\x64\version.dll`, or `bin\x64\plugins\rtx-encore.asi` with the ASI loader that comes with Cyber
Engine Tweaks. Keep the files your other mods need under their own names: `version.dll` for Cyber Engine Tweaks,
`winmm.dll` for RED4ext, `dxgi.dll` for ReShade.

The menu (**Insert**) works on its own. If you use Cyber Engine Tweaks, an optional panel shows the main controls
inside its overlay too: copy the `bin` folder found in `cyberpunk-2077-cet-panel` into the game folder. The panel is
experimental and needs the `.asi` plugin to be installed.

### DXVK, ReShade, Special K, OptiScaler

- Leave `dxgi.dll` to the tool that provides it and give RTX Encore another name.
- With DXVK, the menu attaches to the Vulkan presentation.
- With OptiScaler, see the flip metering option in [Frame generation](FRAME-GENERATION.md#compatibility-options).
