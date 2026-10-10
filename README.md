<div align="center">

# RTX Encore

**Frame generation up to X6, Smooth Motion and Neural Rendering\
for GeForce RTX 20, RTX 30 and RTX 40.**

One file next to the game, a built-in menu, nothing else to install.

[![Latest release](https://img.shields.io/github/v/release/SilyNoMeta/rtx-encore?include_prereleases&style=for-the-badge&label=Latest&color=76b900)](https://github.com/SilyNoMeta/rtx-encore/releases)
[![Downloads](https://img.shields.io/github/downloads/SilyNoMeta/rtx-encore/total?style=for-the-badge&color=2f81f7)](https://github.com/SilyNoMeta/rtx-encore/releases)

[![GPUs](https://img.shields.io/badge/GPUs-RTX%2020%20%7C%2030%20%7C%2040-76b900?style=flat-square&logo=nvidia&logoColor=white)](docs/COMPATIBILITY.md)
[![Graphics APIs](https://img.shields.io/badge/APIs-DirectX%2011%20%7C%2012%20%7C%20Vulkan-0078d4?style=flat-square)](docs/COMPATIBILITY.md)

**[Download](https://github.com/SilyNoMeta/rtx-encore/releases)** ·
**[Installation](docs/INSTALLATION.md)** ·
**[Français](README.fr.md)** ·
**[中文](README.zh-CN.md)**

</div>

> [!IMPORTANT]
> RTX Encore is an independent, experimental mod. It is not affiliated with or endorsed by NVIDIA.
> It loads inside the game: do not use it in online or anti-cheat protected games, where it can be blocked or get an
> account flagged. Back up every file you replace.

## What it does

| | Feature | In short |
| :---: | :--- | :--- |
| 🎞️ | **[Frame generation X2 to X6](docs/FRAME-GENERATION.md)** | DLSS Multi Frame Generation on RTX 20, 30 and 40, in the games that ship DLSS Frame Generation. Fixed multiplier, Dynamic mode with a target frame rate, or the game's own choice. |
| 🛡️ | **Cleaner generated frames** | Protection against tearing on thin geometry (fences, wires, foliage) and against artifacts in moving shadows; sharp HUD and text. |
| ⚡ | **Lighter frame generation** | A performance option that lowers the GPU cost of each generated frame without changing the image. |
| 🌀 | **[Smooth Motion on RTX 30](docs/SMOOTH-MOTION.md)** | NVIDIA's driver frame interpolation, for DirectX 11, DirectX 12 and Vulkan games, including games without DLSS. Off by default. |
| ✨ | **[Neural Rendering](docs/NEURAL-RENDERING.md)** | NR on RTX 20, 30 and 40, with RTX Encore's own performance options for Ampere: optimized processing, Fast precision, independent NR resolution and an Open ultra-fast mode. One to four passes, a style for each, HDR support. Experimental, off by default. |
| 🎛️ | **[Built-in menu and overlay](docs/MENU-AND-OVERLAY.md)** | Press **Insert** in the game: every setting, the state of each feature, and an overlay with frame rate, multiplier, frame pacing, GPU and VRAM. DirectX 11, DirectX 12 and Vulkan. |
| 🔍 | **DLSS render resolution** | Choose DLAA, Quality, Balanced, Performance, Ultra Performance or a custom scale in games that already use DLSS. |
| 🧩 | **One universal file** | `rtx-encore.dll` takes the name the game loads (`version.dll`, `dinput8.dll`, `winmm.dll`, `dxgi.dll` and fifteen others) or runs as an `.asi` plugin. |

## Neural Rendering built for Ampere

**Making NR more practical on RTX 30 is a major part of RTX Encore's development.** Our work covers faster
processing with the same output in local reference tests, optional Fast precision, NR resolution independent of
DLSS, a reduced Open memory footprint, and performance options for our integration of OpenDLSS-NR.

The Open engine's **Ultra-fast mode** reduced NR time from **18.7 to 9.8 ms (−48 %)** and raised
displayed FPS from **73.8 to 94.9 (+29 %)** in the Black Myth: Wukong benchmark on an RTX 3070 Ti Laptop with frame
generation X3. This is an existing comparison with one run per configuration, not a guaranteed gain. The mode is
experimental, needs one NR pass, and can introduce lag or smearing in the NR layer during motion.

**Ultra Performance resolution** is a separate option: lower the NR working size while keeping your preferred
DLSS render setting. Open's additional RTX 30 controls let you trade small image differences or extra VRAM for
less NR time. **[See the measurements, controls and tradeoffs](docs/NEURAL-RENDERING.md).**

## Supported hardware

| GPU | Frame generation | Smooth Motion | Neural Rendering |
| :--- | :---: | :---: | :---: |
| RTX 40 | ✅ | provided by NVIDIA | ✅ |
| RTX 30 | ✅ | ✅ | ✅ |
| RTX 20 | 🧪 experimental | — | 🧪 experimental |

RTX Encore is made for RTX 20, 30 and 40; RTX 50 cards get frame generation and Smooth Motion from NVIDIA.
Details, graphics APIs, driver and game notes: **[Compatibility](docs/COMPATIBILITY.md)**.

## Quick start

1. Download the **[latest release](https://github.com/SilyNoMeta/rtx-encore/releases)** and extract it.
2. Copy `rtx-encore.dll` next to the game's executable and rename it `version.dll`.
3. Start the game. The menu opens once on first launch; press **Insert** to bring it back.
4. Turn DLSS Frame Generation on in the game's settings. If it was already on, turn it off and on again.

Other file names, the `.asi` plugin, updating and removing: **[Installation](docs/INSTALLATION.md)**.

> [!TIP]
> Start with the defaults: the game, or NVIDIA Profile Inspector, chooses the multiplier. On an 8 GB card, X2 or X3
> (or X4 with High or Medium textures) is the safer starting point.

## Controls

| Shortcut | Action |
| :--- | :--- |
| `Insert` | Open or close the menu |
| `Ctrl + Alt + 2…6` | Fixed multiplier X2 to X6 |
| `Ctrl + Alt + PageUp / PageDown` | Raise or lower the multiplier |
| `Ctrl + Alt + D` | Switch between Fixed and Dynamic mode |
| `Ctrl + Alt + Up / Down` | Raise or lower the Dynamic target by 5 FPS (`Shift`: 1 FPS) |
| `Ctrl + Alt + G` | Give the choice back to the game |
| `Ctrl + Alt + O` / `P` | Show or hide the overlay / move it to the next corner |

Every shortcut can be changed in the menu. They only act while the game window has focus.

## Documentation

| Guide | English | Français | 中文 |
| :--- | :---: | :---: | :---: |
| Installation, updating, removing | [Open](docs/INSTALLATION.md) | [Ouvrir](docs/INSTALLATION.fr.md) | [打开](docs/INSTALLATION.zh-CN.md) |
| Frame generation | [Open](docs/FRAME-GENERATION.md) | [Ouvrir](docs/FRAME-GENERATION.fr.md) | [打开](docs/FRAME-GENERATION.zh-CN.md) |
| Smooth Motion on RTX 30 | [Open](docs/SMOOTH-MOTION.md) | [Ouvrir](docs/SMOOTH-MOTION.fr.md) | [打开](docs/SMOOTH-MOTION.zh-CN.md) |
| Neural Rendering | [Open](docs/NEURAL-RENDERING.md) | [Ouvrir](docs/NEURAL-RENDERING.fr.md) | [打开](docs/NEURAL-RENDERING.zh-CN.md) |
| Menu and overlay | [Open](docs/MENU-AND-OVERLAY.md) | [Ouvrir](docs/MENU-AND-OVERLAY.fr.md) | [打开](docs/MENU-AND-OVERLAY.zh-CN.md) |
| Settings file | [Open](docs/SETTINGS.md) | [Ouvrir](docs/SETTINGS.fr.md) | [打开](docs/SETTINGS.zh-CN.md) |
| Compatibility | [Open](docs/COMPATIBILITY.md) | [Ouvrir](docs/COMPATIBILITY.fr.md) | [打开](docs/COMPATIBILITY.zh-CN.md) |
| Troubleshooting | [Open](docs/TROUBLESHOOTING.md) | [Ouvrir](docs/TROUBLESHOOTING.fr.md) | [打开](docs/TROUBLESHOOTING.zh-CN.md) |
| Changelog | [Open](CHANGELOG.md) | [Ouvrir](CHANGELOG.fr.md) | [打开](CHANGELOG.zh-CN.md) |

## Releases

RTX Encore is distributed as ready-to-use releases on this page; its source code is not published. GitHub shows the
SHA-256 of each downloadable archive: compare it with the one in the release notes before installing.

Version numbers start at 1.0.0. The releases published earlier under the name DLSSG-Transfusion, up to
`v1.4.5.3-rtx20-30-40`, are no longer distributed;
[upgrading from them](docs/INSTALLATION.md#upgrading-from-an-earlier-name) keeps your settings.

## Reporting a problem

Open an [issue](https://github.com/SilyNoMeta/rtx-encore/issues) with the game and its graphics API, your GPU and
driver version, and the report the menu copies for you (**System** tab, **Copy**).
[Troubleshooting](docs/TROUBLESHOOTING.md) lists the first things to check.

## Credits

RTX Encore began as a continuation of [DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) by
TonyJoaca and has since grown into a project of its own. It also owes much to the work of:

- [dashdogy/RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock): frame-generation research, menu rendering and input handling;
- [mavismmg/MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx): frame-generation quality research;
- [sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86): frame-generation research on RTX 20 and RTX 30;
- [maanHimself/OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR): the open Neural Rendering engine;
- everyone who tests a build and reports a bug: several fixes exist only because of those reports.

The components included in the releases and their licenses are listed in
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Disclaimer

RTX Encore is provided as is, without warranty: use it at your own risk. Image quality, stability and performance
vary with the game, its version, the driver and the hardware. High multipliers raise latency and VRAM use.

The components included in the releases keep their own licenses: see
[THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), which must stay with any copy of a release. NVIDIA, GeForce, RTX
and DLSS are trademarks of NVIDIA Corporation. NVIDIA software and games remain subject to their own terms; none is
included in the releases.
