# DLSSG-Transfusion: Universal Multi-Game Proxy & Injection Guide

## Overview

**DLSSG-Transfusion** is a universal mod designed to unlock Multi-Frame Generation (2x, 3x, 4x, 5x, 6x, and Dynamic Mode) on NVIDIA RTX 40-Series (Ada Lovelace), RTX 30-Series (Ampere) and RTX 20-Series (Turing) GPUs across **any game** utilizing NVIDIA Streamline and DLSS Frame Generation (`sl.dlss_g.dll` / `nvngx_dlssg.dll`).

It features:
- **Blackwell Kernel Transfusion**: Backports Blackwell `sm_120` branchless scatter math to Ada `sm_89` at runtime.
- **Zero Configuration / Standalone Mode**: Works completely out-of-the-box in any game without Cyber Engine Tweaks (CET).
- **Proxy DLL Injection**: One primary `DLSSG-Transfusion.dll` can be renamed to
  `version.dll`, `dxgi.dll`, `winmm.dll`, or `dinput8.dll`; exact-export fallback
  binaries and `DLSSG-Transfusion.asi` are also provided.
- **NVIDIA OTA Override**: Forces `slInit` to enable `eAllowOTA | eLoadDownloadedPlugins` so games can load newer/updated models from driver caches or local directories.
- **Dynamic Multiplier Hotkeys**: Switch multipliers live on the fly during gameplay using hotkeys.
- **Diagnostic & Benchmark Telemetry**: Writes runtime status directly to `DLSSG-Transfusion.log` and per-frame CSV data to `DLSSG-Transfusion_perf.csv`.

---

## 1. Choosing a Proxy DLL

Copy `DLSSG-Transfusion.dll` from `dist/`, rename the copy to one of the proxy
names below, and place it in the game executable directory. The already named
DLLs are exact-ordinal fallbacks for games that do not accept the primary binary.

| File | Use Case & Compatibility | Recommended For |
|---|---|---|
| **`version.dll`** | **Primary Recommended Proxy.** Intercepts standard version API calls loaded early by Windows and game engines. Does not conflict with graphics wrappers. | Unreal Engine 4/5 titles, REDengine, Frostbite, Unity, and most modern DX12 games. |
| **`dxgi.dll`** | Alternative for games that initialize DXGI earlier than `version.dll` or ignore local `version.dll`. | Games that fail to load `version.dll`. *Note: Do not use if you already have ReShade or SpecialK named `dxgi.dll`.* |
| **`winmm.dll`** | Clean proxy for games with custom anti-cheat or games that already have existing `dxgi.dll` / `version.dll` mods. | Legacy engines and titles requiring alternative injection vectors. |
| **`DLSSG-Transfusion.asi`** | Native ASI plugin format. | Games using an ASI Loader (e.g. Ultimate ASI Loader, Cyber Engine Tweaks, ScriptHook). |

---

## 2. In-Game Hotkeys

You can control the multiplier at any moment without pausing or exiting the game:

| Hotkey | Action | Notes |
|---|---|---|
| **`Ctrl + Alt + 2`** | Switch to **2x** Multiplier | Native DLSS-G standard (1 generated frame per rendered frame). |
| **`Ctrl + Alt + 3`** | Switch to **3x** Multiplier | Generates 2 frames per rendered frame. |
| **`Ctrl + Alt + 4`** | Switch to **4x** Multiplier | Generates 3 frames per rendered frame. |
| **`Ctrl + Alt + 5`** | Switch to **5x** Multiplier | Generates 4 frames per rendered frame (Experimental). |
| **`Ctrl + Alt + 6`** | Switch to **6x** Multiplier | Generates 5 frames per rendered frame (Experimental). |
| **`Ctrl + Alt + PageUp`** | Increment Multiplier | Steps multiplier up by 1 (max 6x, sets Fixed Mode). |
| **`Ctrl + Alt + PageDown`** | Decrement Multiplier | Steps multiplier down by 1 (min 2x, sets Fixed Mode). |
| **`Ctrl + Alt + D`** | Toggle Dynamic Mode | Toggles between Dynamic and Fixed Mode (target 0 = display refresh rate). |
| **`Ctrl + Alt + Up` / `+`** | Increase Dynamic Target FPS | Steps dynamic target FPS up by 5 FPS (+Shift for 1 FPS fine adjustment). Enables Dynamic Mode. |
| **`Ctrl + Alt + Down` / `-`** | Decrease Dynamic Target FPS | Steps dynamic target FPS down by 5 FPS (+Shift for 1 FPS fine adjustment). Enables Dynamic Mode. |

| **`Ctrl + Alt + G`** | Game Decides | Switches to `"mode": "game"` (the game or NVIDIA Profile Inspector chooses the multiplier). |
| **`Ctrl + Alt + O`** / **`P`** | Overlay | Show/hide the overlay / move it to the next corner. |

Hotkeys automatically save the new configuration to `DLSSG-Transfusion.json` and reapply the setting instantly on the active render thread.

These are the defaults. Every shortcut can be changed with the `hotkey...` keys of `DLSSG-Transfusion.json` (for example `"hotkeyFixed4": "Ctrl+Alt+4, Ctrl+Alt+Num4"`: several combinations separated by commas, `""` disables the action) or recorded in the ReShade panel. Shortcuts only react while the game window has focus.


---

## 3. Configuration (`DLSSG-Transfusion.json`)

The mod creates and looks for `DLSSG-Transfusion.json` in:
1. Next to the proxy DLL (`version.dll` / `dxgi.dll` / `winmm.dll` / `DLSSG-Transfusion.asi`) in the game directory.
2. Older Cyberpunk 2077 installs that still have the former CET mod folder use `plugins\cyber_engine_tweaks\mods\DLSSG-Transfusion\DLSSG-Transfusion.json`.

Every key can also be changed in game with the optional ReShade add-on (see *ReShade settings panel* below).

Settings are grouped in the same sections and order as the ReShade panel, with a `//` comment after each one (editors may flag the comments; the mod ignores them). Excerpt (full default file: [`config/DLSSG-Transfusion.json`](config/DLSSG-Transfusion.json)):
```jsonc
{
  "configVersion": 3,                        // Settings apply live unless the comment says to restart the game.
  "frameGeneration": {
    "mode": "game",                          // fixed, dynamic or game (the game or NVIDIA Profile Inspector decides; default).
    "multiplier": 4,                         // Fixed mode multiplier: 2 to 6. 5 and 6 are experimental.
    "dynamicTargetFrameRate": 0,             // Dynamic mode target FPS. 0 follows the refresh rate of the monitor showing the game.
    "dynamicExperimental56": false           // Let Dynamic mode go up to 5x and 6x.
  }
}
```

Older files, including the flat `// commented` layout of TonyJoaca's DLSSG-Transfusion, are still read: their values are kept and the file is rewritten in this layout on first launch. Keys may also stay flat at the top level; sections are only for readability.

- `"mode"`: `"fixed"`, `"dynamic"` or `"game"` (default: the game or NVIDIA Profile Inspector chooses the multiplier).
- `"multiplier"`: Fixed integer multiplier from `2` to `6`.
- `"dynamicTargetFrameRate"`: Target FPS when in dynamic mode (e.g., `120`, `144`, `165`, `240`); `0` follows the refresh rate of the monitor showing the game. In Vulkan games, where NVIDIA offers no Dynamic MFG, dynamic mode switches between fixed multipliers itself: see [docs/VULKAN.md](docs/VULKAN.md#dynamic-mode-in-vulkan-the-adaptive-controller).
- `"dynamicExperimental56"`: `true` to allow dynamic mode to scale up to 5x/6x; `false` to clamp dynamic mode to a 4x ceiling.
- `"overlayShowUiRecomposition"`, `"overlayShowHudless"`, `"overlayShowUiAlpha"`, `"overlayShowVersions"`: extra overlay lines showing whether UI recomposition is on, where the HUD-less scene and the UI alpha come from (game, UI assist capture/injection, or none), and the DLSS / DLSS-G / Streamline versions loaded by the game.
- `"autoUiRecomposition"`: `true` (default) turns DLSS-G UI recomposition on when the game tags HUD-less and UI buffers without asking for it; `false` follows the game's own choice. Try `false` if generated frames are badly distorted in a game that tags those buffers. DOOM: The Dark Ages is recognized and always follows its own choice (forcing UIR there distorts every generated frame in motion). `"forceUiRecomposition"` still forces it.
- `"overlayShowFramePacing"`, `"overlayShowGpu"`, `"overlayShowVram"`, `"overlayShowDebug"`: extra overlay lines with the displayed frame time (average, 99th percentile, jitter), GPU load/temperature/power/clocks and VRAM used/total (read from the NVIDIA driver through NVML), and a debug line (mode, generated-frame ceiling, Dynamic pacer hook, last Streamline result, patch route). All extra lines are off by default.
- `"logHudUi"` under `"diagnostics"`: trace the first three game-provided HUD-less copies and their D3D12 barrier steps. Off by default; applies live and is also available in the ReShade panel as **Log HUD/UI**.

### DLSS Super Resolution render resolution

`"dlssRenderScale"` (`"game"`, `"dlaa"`, `"quality"`, `"balanced"`, `"performance"`, `"ultra-performance"` or `"custom"` with `"dlssCustomScale"` in percent, 50 to 100) forces the resolution a D3D12 game renders at before DLSS upscales it: the mod answers the game's "optimal settings" query to NGX with that size. It only works in games that already use DLSS Super Resolution and ask NGX for the recommended size; the ReShade panel shows the observed render resolution and confirms when the game follows the request. It is a render-resolution preset, not a K/M model preset. If a game misbehaves, set it back to `"game"`.

#### Unreal Engine games that ship DLSS but hide it

Some Unreal Engine 4/5 games include NVIDIA's DLSS plugin (an `nvngx_dlss.dll` somewhere under the game's `Engine\Plugins` or `<Game>\Plugins` folders, and usually Streamline `sl.*.dll` files) but offer no DLSS option in their menus (REANIMAL, for example). DLSS can often be turned on from the game's `Engine.ini`:

- UE5: `%LOCALAPPDATA%\<Project>\Saved\Config\Windows\Engine.ini`
- UE4: `%LOCALAPPDATA%\<Project>\Saved\Config\WindowsNoEditor\Engine.ini`

`<Project>` is the engine project name, which can differ from the game's title (REANIMAL uses `Everholm`). Start the game once so the file exists, close it, make a backup, then add:

```ini
[SystemSettings]
r.NGX.Enable=1
r.NGX.DLSS.Enable=1
r.TemporalAA.Upsampling=1
r.ScreenPercentage=67

[/Script/DLSS.DLSSSettings]
bEnableDLSSD3D12=True
```

`r.ScreenPercentage` is the render resolution Unreal feeds DLSS: 100 for DLAA, 67 Quality, 58 Balanced, 50 Performance, 33 Ultra Performance. If the game rewrites `Engine.ini` at startup, mark the file read-only after editing it. Frame generation then works as in any Streamline game.

Unreal decides its render resolution from `r.ScreenPercentage` rather than asking DLSS, so the mod also looks for that console variable in Unreal Engine 4/5 executables (by the code pattern every Unreal build uses to declare it, not per game) and, when found, sets it live from `dlssRenderScale` and restores the game's value when set back to `"game"`. The ReShade panel says whether it was found. Protected or unusual executables may hide it; then use `r.ScreenPercentage` in `Engine.ini` as above.

### ReShade settings panel (optional)

`DLSSG-Transfusion.addon64` adds a **DLSSG-Transfusion** tab to the ReShade overlay (ReShade 6.8 or newer, *with full add-on support*). Copy it next to the ReShade DLL in the game folder.

- It shows the engine state (bridge, requested/actual multiplier, rendered/displayed FPS) and every key of `DLSSG-Transfusion.json`.
- Each change is written straight into `DLSSG-Transfusion.json`, keeping its comments. Nothing is stored in `ReShade.ini`.
- Mode, multiplier, target FPS, overlay, UI, menu detection and logging settings apply live.
- `forceOTA`, `patchFlipMetering`, `blackwellTransfusion`, `qualityValidWarp`, `qualityPolicy`, `optimizedKernels` and `gpuArchitecture` are read when the game or DLSS-G starts: the panel marks them *Restart the game to apply* after a change.
- It draws the multiplier overlay where the engine cannot (Vulkan games: the engine's overlay is DXGI only). See [docs/VULKAN.md](docs/VULKAN.md#overlay-in-vulkan).
- The engine does not need ReShade; without the add-on, edit the JSON by hand or use the hotkeys.

---

## 4. Diagnostic Logging

The mod automatically creates `DLSSG-Transfusion.log` in the game executable directory.

Log entries include microsecond timestamps:
```text
[2026-09-03 23:23:05.581] DLSSG-Transfusion v1.4.5.0-rtx20-30-40 for RTX 20, 30 and 40
[2026-09-03 23:23:05.581] Loaded as: version.dll
[2026-09-03 23:23:05.581] Proxied system DLL: C:\WINDOWS\system32\version.dll
[2026-09-03 23:23:05.581] Module Path: D:\Games\GameDir\version.dll
[2026-09-03 23:23:05.581] Game Directory: D:\Games\GameDir (PID: 19628)
[2026-09-03 23:23:05.581] Direct Detours on sl.interposer.dll installed successfully (slGetFeatureFunction, slInit)
[2026-09-03 23:23:05.581] Early LoadLibraryW/ExW hooks installed on kernel32.dll
[2026-09-03 23:23:07.595] Streamline maximum: patched RVA 0x1767: sl.dlss_g.dll
[2026-09-03 23:23:07.595] NGX device support: patched RVA 0x1022: nvngx_dlssg.dll
[2026-09-03 23:23:07.595] Intercepted slDLSSGSetOptions for live multiplier control
[2026-09-03 23:23:07.595] Intercepted slDLSSGGetState for render-thread reapply and actual telemetry
[2026-09-03 23:23:07.595] Applied fixed multiplier: 4x, result=0
```

---

## 5. Game-Specific Compatibility Notes

### Unreal Engine 4 / 5 Titles (e.g. S.T.A.L.K.E.R. 2, Mortal Shell, etc.)
- Locate the main game binary (usually in `<GameRoot>/<GameName>/Binaries/Win64/<GameName>-Win64-Shipping.exe`).
- Place `version.dll` directly next to the Shipping executable.
- Enable DLSS and Frame Generation in the in-game display settings.
- Use `Ctrl+Alt+3..6` to switch multipliers.

### REDengine (Cyberpunk 2077)
- Compatible both as standalone proxy (`bin/x64/version.dll`) or as `bin/x64/plugins/DLSSG-Transfusion.asi` (loaded by the ASI loader shipped with Cyber Engine Tweaks).
- The in-game settings panel is the optional ReShade add-on; the former CET Lua panel has been removed.
