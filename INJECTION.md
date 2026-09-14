# DLSSG-Transfusion: Universal Multi-Game Proxy & Injection Guide

## Overview

**DLSSG-Transfusion** is a universal mod designed to unlock Multi-Frame Generation (2x, 3x, 4x, 5x, 6x, and Dynamic Mode) on NVIDIA RTX 40-Series (Ada Lovelace) GPUs across **any game** utilizing NVIDIA Streamline and DLSS Frame Generation (`sl.dlss_g.dll` / `nvngx_dlssg.dll`).

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
| **`Ctrl + Alt + D`** | Toggle Dynamic Mode | Toggles between Dynamic and Fixed Mode (default 120 FPS target). |
| **`Ctrl + Alt + Up` / `+`** | Increase Dynamic Target FPS | Steps dynamic target FPS up by 5 FPS (+Shift for 1 FPS fine adjustment). Enables Dynamic Mode. |
| **`Ctrl + Alt + Down` / `-`** | Decrease Dynamic Target FPS | Steps dynamic target FPS down by 5 FPS (+Shift for 1 FPS fine adjustment). Enables Dynamic Mode. |

Hotkeys automatically save the new configuration to `DLSSG-Transfusion.json` and reapply the setting instantly on the active render thread.


---

## 3. Configuration (`DLSSG-Transfusion.json`)

The mod creates and looks for `DLSSG-Transfusion.json` in:
1. Next to the proxy DLL (`version.dll` / `dxgi.dll` / `winmm.dll` / `DLSSG-Transfusion.asi`) in the game directory.
2. If running Cyberpunk 2077 with CET, in `plugins\cyber_engine_tweaks\mods\DLSSG-Transfusion\DLSSG-Transfusion.json`.

Example `DLSSG-Transfusion.json`:
```json
{
  "mode": "fixed",
  "multiplier": 4,
  "showOverlay": false,
  "dynamicTargetFrameRate": 120,
  "dynamicExperimental56": false
}
```

- `"mode"`: `"fixed"` or `"dynamic"`.
- `"multiplier"`: Fixed integer multiplier from `2` to `6`.
- `"dynamicTargetFrameRate"`: Target FPS when in dynamic mode (e.g., `120`, `144`, `165`, `240`).
- `"dynamicExperimental56"`: `true` to allow dynamic mode to scale up to 5x/6x; `false` to clamp dynamic mode to a 4x ceiling.

---

## 4. Diagnostic Logging

The mod automatically creates `DLSSG-Transfusion.log` in the game executable directory.

Log entries include microsecond timestamps:
```text
[2026-09-03 23:23:05.581] DLSSG-Transfusion (Universal Blackwell Transfusion & Multi-Game Edition)
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
- Compatible both as standalone proxy (`bin/x64/version.dll`) or with CET (`bin/x64/plugins/cyber_engine_tweaks/mods/DLSSG-Transfusion/init.lua` + `DLSSG-Transfusion.asi`).
- If using `version.dll`, CET UI will automatically sync with `DLSSG-Transfusion.json` and hotkeys.
