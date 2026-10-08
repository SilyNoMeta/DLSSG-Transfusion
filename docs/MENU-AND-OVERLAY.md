# Menu and overlay

*[Français](MENU-AND-OVERLAY.fr.md) · [中文](MENU-AND-OVERLAY.zh-CN.md)*

RTX Encore draws its own menu and overlay in DirectX 11, DirectX 12 and Vulkan games. Nothing else has to be
installed.

## The menu

Press **Insert** in the game window to open or close it. It opens once by itself on first launch. The shortcut can
be changed in the menu.

A strip of four chips heads every page: **Upscaling**, **Frame Generation**, **Smooth Motion** and **Neural
Rendering**. Each shows the state of its feature with a colored dot and one figure; hover it for the detail, click it
to go to its settings. When a problem lasts, a card under the strip names it and says how to fix it.

| Tab | What it holds |
| :--- | :--- |
| **Frames** | Frame generation with its quality and interface options, Smooth Motion, the rendered-frame limit, V-Sync |
| **Image** | DLSS render resolution, Neural Rendering |
| **Overlay** | A preview of the overlay, its position, its text size and the lines it shows |
| **System** | Shortcuts, compatibility, diagnostics and the session details, with a **Copy** button for bug reports |

Good to know:

- Each change is saved at once in `rtx-encore.jsonc`, with its comments kept.
- Most settings apply live. A `*` after a name marks a setting read when the game starts; it turns orange once
  changed, and a line at the bottom of the menu lists the changes waiting for a restart.
- A modified setting has a brighter name and a reset button.
- Each section shows its main settings; the others are in groups that open in place (**Fine tuning**,
  **Performance**, **Diagnostics**, ...).
- Drag the title bar to move the menu and its bottom-right corner to resize it. Position and size are remembered.
- If a button was held while the menu opened, release it before clicking.

### When the menu opens in a separate window

In a few games the menu cannot be drawn safely inside the image. The settings then open in a separate window and
the overlay becomes a small click-through text over the game. Borderless window mode is recommended in that case.
The settings and their effect are the same.

## The overlay

Turn it on in the **Overlay** tab or with `Ctrl + Alt + O`; `Ctrl + Alt + P` moves it to the next corner.

| Example | Meaning |
| :--- | :--- |
| `74 fps` | The game's frame rate; no frame generation observed |
| `74/37 fps 2x` | Displayed / rendered frame rate, with the observed multiplier |
| `148/74 fps sm` | Estimated output with Smooth Motion / the game's frame rate |
| `296/74 fps 2x+sm` | Frame generation X2, then Smooth Motion |

Optional lines, all off by default:

| Line | Shows |
| :--- | :--- |
| Neural Rendering | Its state and the size it works at |
| Frame pacing | Frame time: average, 99th percentile and jitter |
| GPU | Load, temperature, power and clocks |
| VRAM | Used / total on the card, all programs included |
| Versions | DLSS, DLSS Frame Generation and Streamline versions loaded by the game |
| Interface | Whether the separate interface handling is on, and where the clean scene and interface layer come from |

Position (four corners) and text size (10 to 32 px) are in the same tab.

## Other controls

| Setting | Tab | What it does |
| :--- | :--- | :--- |
| **Render resolution** | Image | Forces the resolution the game renders at before DLSS: DLAA, Quality, Balanced, Performance, Ultra Performance, or a custom scale from 50 to 100 %. The game must already use DLSS. The menu shows the resolution observed and whether the game follows the request; set it back to *Game* if a game misbehaves. In Unreal Engine 4 and 5 games the screen percentage is adjusted live when possible. |
| **Render-frame limit (FPS)** | Frames | Caps the frames the game renders, through NVIDIA Reflex when the game supports it. 0 follows the game. It does not count generated frames and is suspended in Dynamic mode. |
| **Request V-Sync off** | Frames | Asks DirectX games to present without V-Sync. A setting forced in the NVIDIA control panel keeps priority. No effect in Vulkan games. |
| **Compatible ray-traced hair** | System | The Witcher 3 (DirectX 12, game builds 25575366 and 25646871): makes the game's ray-traced hair available on RTX 20, 30 and 40 cards. The game's own hair settings still decide whether hair is rendered. Not validated in game on RTX 20 and RTX 30 yet. Restart to apply. |

## Shortcuts

Defaults are listed in [Frame generation](FRAME-GENERATION.md#shortcuts). In the **System** tab, each action can be
given several key combinations or none. Shortcuts only act while the game window has focus.

## Logs

Session logs are written in the `rtx-encore-logs` folder beside the mod's file; the three most recent are kept
(**Session logs kept**, 1 to 100). **Log performance** writes the frame rate and frame times of the session to a CSV
file in the same folder.
