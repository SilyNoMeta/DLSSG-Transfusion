# Troubleshooting

*[Français](TROUBLESHOOTING.fr.md) · [中文](TROUBLESHOOTING.zh-CN.md)*

## First checks

1. **Is the mod loaded?** A `rtx-encore-logs` folder appears beside the mod's file when it is, and **Insert** opens
   the menu. If neither happens, the game did not load the file.
2. **Is there only one copy?** Two copies of the mod in the same game, under two names, cause crashes and odd
   behavior.
3. **What does the menu say?** The four chips at the top give the state of each feature, and a card names any
   lasting problem with its fix.

## The mod does not load

| Check | What to do |
| :--- | :--- |
| Wrong folder | The file must be beside the executable that really runs the game. For Unreal Engine: `<Game>\Binaries\Win64\`. |
| The name is not loaded by this game | Try the next name: `version.dll`, then `dinput8.dll`, `winmm.dll`, `dxgi.dll`. |
| The name is used by another mod | Leave that mod its file and choose another name for RTX Encore. |
| The renamed file is refused | Use the ready-named copy from `alternative-proxies`. |
| The game has an anti-cheat | It can block the file. RTX Encore must not be used there. |

## Frame generation

| Symptom | First check |
| :--- | :--- |
| The game has no Frame Generation option | RTX Encore extends DLSS Frame Generation in games that have it; it cannot add one. See [Smooth Motion](SMOOTH-MOTION.md) for the other games. |
| The option is there but greyed out | Check that DLSS is on, that the game runs in DirectX 12 or Vulkan, and that the mod is loaded. |
| The multiplier stays at X2 | Turn frame generation off and on in the game, or restart it. Check the mode in the menu: in **Game decides**, the game chooses. |
| X5 or X6 is not reached in Dynamic mode | Turn **Allow 5x and 6x** on. |
| Nothing happens with this game's version | The game's `nvngx_dlssg.dll` may be a version that is not supported: see [Compatibility](COMPATIBILITY.md#dlss-frame-generation-versions). |
| Doubled outlines or distorted frames in motion | Turn **Automatic UI recomposition** off. |
| Artifacts on fences, foliage or shadows | Keep **Anti-tearing / anti-ghosting** on and try the other **Protection tuning**. |
| Stutter or crashes at high multipliers | VRAM is probably full: lower the multiplier or the texture quality. |
| Crash in menus or loading screens | Keep **Disable menu detection** off. |

## Smooth Motion and Neural Rendering

See the end of their pages: [Smooth Motion](SMOOTH-MOTION.md#if-it-does-not-work),
[Neural Rendering](NEURAL-RENDERING.md#when-something-does-not-work).

## Menu and settings

| Symptom | First check |
| :--- | :--- |
| **Insert** does nothing | The mod is not loaded (see above), or the shortcut was changed: look at `menuState` in `rtx-encore.jsonc`. |
| The menu opens in a separate window | Expected in a few games: see [Menu and overlay](MENU-AND-OVERLAY.md#when-the-menu-opens-in-a-separate-window). |
| Clicks do not register | Release every mouse button, then click again. Borderless window mode helps. |
| Settings revert at once | The settings file could not be saved: make sure it is not read-only, nor open and locked in an editor. |
| A setting has no effect | Settings marked with `*` are read when the game starts: restart the game. |
| Shortcuts do nothing | They only act while the game window has focus, and `disableKeybinds` must be `false`. |

## Reporting a problem

Open an [issue](https://github.com/SilyNoMeta/rtx-encore/issues) with:

- the game, its version or store, and its graphics API;
- your card and NVIDIA driver version;
- the RTX Encore version and the name you gave the file;
- the report copied from the menu (**System** tab, **Copy**);
- the log of the session where the problem happened, from `rtx-encore-logs`. Logs can contain folder names from your
  PC: read them before posting.

One problem per issue, with what you did and what you saw.
