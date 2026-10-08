# Compatibility

*[Français](COMPATIBILITY.fr.md) · [中文](COMPATIBILITY.zh-CN.md)*

These notes record what was tested and observed. They are not a certification: a game update, another driver or
another mod can change the result.

## Graphics cards

| GPU | Frame generation | Smooth Motion | Neural Rendering |
| :--- | :--- | :--- | :--- |
| RTX 40 | Supported. DirectX 12 and Vulkan exercised on an RTX 4090. | Provided by NVIDIA | Supported |
| RTX 30 | Supported. DirectX 12 exercised on an RTX 3070 Ti Laptop. | Supported with the drivers listed below | Supported |
| RTX 20 | Experimental: not validated on a physical RTX 20 card yet. | Not supported | Experimental |

RTX Encore is made for RTX 20, 30 and 40 cards. GTX cards and cards from other vendors are not supported.

## Graphics APIs

| Feature | DirectX 11 | DirectX 12 | Vulkan |
| :--- | :---: | :---: | :---: |
| Frame generation X2 to X6 | — | ✅ | ✅ (experimental on RTX 20 and RTX 30) |
| UI assist | — | ✅ | — |
| Smooth Motion (RTX 30) | ✅ | ✅ | ✅ |
| Neural Rendering | 🧪 | 🧪 | 🧪 |
| Menu and overlay | ✅ | ✅ | ✅ |

DirectX 9 and OpenGL games are not supported.

## DLSS Frame Generation versions

Frame generation works with these versions of the game's `nvngx_dlssg.dll`:

`310.1.0` · `310.2.0` · `310.2.1` · `310.3.0` · `310.4.0` · `310.5.0` · `310.5.2` · `310.5.3` · `310.6.0` ·
`310.7.0` · `310.7.128` · `310.7.129` · `310.8.0` · `310.9.0` · `310.9.1`

- Any other version is left untouched: the game keeps its own frame generation.
- **310.9.0 or 310.9.1** is recommended on every card, for the best quality above X2, and is needed on RTX 20
  cards. **Fast frame generation** needs 310.9.1.
- The other DLSS files of the game (Super Resolution, Ray Reconstruction, ...) can have any version.

## Smooth Motion drivers

Smooth Motion on RTX 30 accepts NVIDIA drivers **617.42**, **617.14**, **616.92** and **616.64**, and no other.
Only **617.14** has been verified in a game. See [Smooth Motion](SMOOTH-MOTION.md).

## Neural Rendering file

Neural Rendering needs NVIDIA's `nvngx_dlssnr.dll` **310.8.0**, supplied by you. See
[Neural Rendering](NEURAL-RENDERING.md).

## Game notes

| Game | API | Notes |
| :--- | :---: | :--- |
| Cyberpunk 2077 | DX12 | Frame generation tested on an RTX 3070 Ti Laptop. Works as `version.dll` or as an `.asi` plugin; see [Installation](INSTALLATION.md#cyberpunk-2077). |
| Black Myth: Wukong | DX12 | Frame generation tested on an RTX 4090. Neural Rendering confirmed by users. |
| No Man's Sky | Vulkan | Frame generation tested on an RTX 4090. **Turn Automatic UI recomposition off**: it causes doubled outlines in camera motion in this game. Neural Rendering with frame generation X2 confirmed at 1080p and 1440p. |
| DOOM: The Dark Ages | Vulkan | Frame generation tested on an RTX 4090. The game keeps its own choice for the interface handling, whatever the setting. |
| Starfield | DX12 | Works with UI assist on; a crash when loading a save was fixed after a user's report. |
| The Witcher 3 | DX12 | Compatible ray-traced hair on game builds 25575366 and 25646871; see [Menu and overlay](MENU-AND-OVERLAY.md#other-controls). |
| Manor Lords | DX12 | Smooth Motion observed working on RTX 30 (driver 617.14). |
| Shadows of Doubt | DX11 | Smooth Motion observed working on RTX 30 (driver 617.14). Neural Rendering confirmed by users. |
| Enshrouded | Vulkan | Smooth Motion observed working on RTX 30 (driver 617.14). Neural Rendering confirmed by users. |
| Bodycam, Palworld, Portal with RTX, Star Wars Zero Company | — | Neural Rendering confirmed by users. |

Tell us what you observe in other games by opening an
[issue](https://github.com/SilyNoMeta/rtx-encore/issues): game, graphics API, card, driver, and what works or not.

## Other tools

| Tool | Notes |
| :--- | :--- |
| ReShade, Special K | Leave them their `dxgi.dll` and give RTX Encore another name. ReShade is not needed for the menu. |
| DXVK | Do not name RTX Encore `dxgi.dll`: DXVK provides that file. The menu is drawn in its Vulkan output. |
| OptiScaler | A dedicated option exists for setups that need it: see [Frame generation](FRAME-GENERATION.md#compatibility-options). |
| Cyber Engine Tweaks, RED4ext | Keep their files under their own names. An optional, experimental panel exists for Cyber Engine Tweaks. |
| NVIDIA Profile Inspector | In **Game decides** mode, the multiplier it sets is followed. |

## Anti-cheat and online games

RTX Encore loads inside the game. Games protected by an anti-cheat can refuse to start, or flag the account. Do not
use it there.
