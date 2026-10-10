# Settings file

*[Français](SETTINGS.fr.md) · [中文](SETTINGS.zh-CN.md)*

Every setting can be changed in the [menu](MENU-AND-OVERLAY.md) (**Insert**), which is the easiest way. This page is
for those who prefer to edit the file.

## The file

- Name: `rtx-encore.jsonc`, beside the mod's file. It is created on first launch with the default values.
- Format: JSON with `//` comments. Each key carries a comment that says what it does; some editors flag the
  comments, the mod reads them fine.
- Settings are grouped in sections. Most apply live; the comment says when a restart of the game is needed.
- The menu's own shortcut and layout are stored in the same file, in its `menuState` section.
- A settings file left by an earlier version is taken over with its values: see
  [Installation](INSTALLATION.md#upgrading-from-an-earlier-name).

No key holds a path or a command: a settings file can be shared with someone else as it is.

## Main keys

### `frameGeneration`

| Key | Default | Values |
| :--- | :---: | :--- |
| `mode` | `"game"` | `"game"` (the game or NVIDIA Profile Inspector decides), `"fixed"`, `"dynamic"` |
| `multiplier` | `4` | `2` to `6`, used in fixed mode. `5` and `6` are experimental. |
| `dynamicTargetFrameRate` | `0` | Target FPS of dynamic mode. `0` follows the refresh rate of the monitor showing the game. |
| `dynamicExperimental56` | `false` | Lets dynamic mode go up to X5 and X6. |

### `dlssSuperResolution`

| Key | Default | Values |
| :--- | :---: | :--- |
| `dlssRenderScale` | `"game"` | `"game"`, `"dlaa"`, `"quality"`, `"balanced"`, `"performance"`, `"ultra-performance"`, `"custom"` |
| `dlssCustomScale` | `67` | Percent, `50` to `100`, with `"custom"` |

### `neuralRendering`

| Key | Default | Values |
| :--- | :---: | :--- |
| `nrEnabled` | `false` | Turns Neural Rendering on. |
| `nrEngine` | `"nvidia"` | `"nvidia"` or `"opendlss"` (highly experimental). Restart to apply. |
| `nrPasses` | `1` | `1` to `4` |
| `nrLaterPassLocalTone` | `0` | Tone applied after pass one, `0` to `2` |
| `nrResolution` | `"render"` | `"render"`, `"output"`, `"quality"`, `"balanced"`, `"performance"`, `"ultra-performance"`, `"custom"` |
| `nrResolutionScale` | `67` | Percent of the output size, `33` to `100`, with `"custom"` |
| `nrIntensity` | `1` | Strength, `0` to `2` |
| `nrStyle` | `0` | `0` default, `1` natural, `2` cinematic |
| `nrPass1Style` … `nrPass4Style` | `"inherit"` | `"inherit"` (follows `nrStyle`), `"default"`, `"natural"`, `"cinematic"` |
| `nrLocalTone`, `nrLocalStructure` | `1` | `0` to `2` |
| `nrAutoMask` | `false` | Character mask; needed for `nrSkinStructure` |
| `nrSkinStructure` | `1` | `0` to `2` |
| `nrHdrExposure` | `1` | HDR games: brightness given to NR |
| `nrPrecision` | `"exact"` | `"exact"` or `"fast"` (RTX 20 and RTX 30). Restart to apply. |
| `nrPreset` | `0` | NVIDIA model profile: `0` automatic (recommended), `1` to `3` |

#### NR performance options

These controls are also in **Image → Neural Rendering → Performance**. See
[Neural Rendering](NEURAL-RENDERING.md#performance-controls-on-rtx-30) for measured results and image tradeoffs.
Engine-specific options apply only to the engine named below; changing an engine or a restart-only option needs a
game restart.

| Key | Default | Benefit and tradeoff |
| :--- | :---: | :--- |
| `nrPaddingAware` | `false` | Allows a nearby smaller NR size, at most 2 % less per axis, without changing the DLSS render size. Can change the image. Applies live. |
| `nrMaxInFlight` | `0` | NVIDIA, DirectX 12 with Reflex: `0` no limit, `1` or `2` outstanding NR frames; active frame generation uses at least `2`. Applies live; in-game performance effect unverified. |

### `openExperimental`

Open's performance keys are in this separate section. Choose the engine and the backend matching your card from
the menu first; these keys do not turn Open on by themselves.

| Key | Default | Benefit and tradeoff |
| :--- | :---: | :--- |
| `nrOpenFast` | `false` | Open on RTX 30: Fast precision, shorter NR time with small image differences and no extra VRAM. Restart to apply. |
| `nrOpenVramForSpeed` | `false` | Open on RTX 30: shorter NR time, same image, for about 160 MB more VRAM at 1440p DLSS Balanced. Restart to apply. |
| `nrOpenFastProjection` | `true` | Open on RTX 30: faster processing with small image differences. Restart to apply. |
| `nrOpenUltraFast` | `false` | Open, one NR pass: **Ultra-fast mode**, about half the NR time in the documented comparison. Older NR layer, possible motion artifacts, a few MB more VRAM. Restart to apply. |
| `nrOpenUltraFastGhostTolerance` | `0.03` | `0` to `0.3`; lower: fewer ghosts, more surfaces temporarily waiting for NR. Applies live. |
| `nrOpenUltraFastFillTolerance` | `0.03` | `0` to `0.3`; controls filling newly visible surfaces with NR from similar surfaces. `0` turns filling off. Applies live. |

`nrResolution: "ultra-performance"` sets NR to 33 % of output width and height. It is independent of
`nrOpenUltraFast` and of the `dlssRenderScale` setting; these choices do not enable one another.

### `overlay`

| Key | Default | Values |
| :--- | :---: | :--- |
| `showOverlay` | `false` | Shows the overlay. |
| `overlayPosition` | `"top-left"` | `"top-left"`, `"top-right"`, `"bottom-left"`, `"bottom-right"` |
| `overlayFontSize` | `14` | Text size in pixels, `10` to `32` |
| `overlayShowNr`, `overlayShowFramePacing`, `overlayShowGpu`, `overlayShowVram`, `overlayShowVersions` | `false` | Optional lines |

### `hudUi`

| Key | Default | Values |
| :--- | :---: | :--- |
| `autoUiRecomposition` | `true` | Separate interface handling whenever the game supplies what it needs |
| `uiAssist` | `true` | DirectX 12: builds the clean scene and the interface layer when the game does not supply them |
| `forceUiRecomposition` | `false` | Asks for the separate handling even when the game does not |

### `nativeMenuFeatures`

| Key | Default | Values |
| :--- | :---: | :--- |
| `reflexFrameLimit` | `0` | `0` follows the game; `1` to `1000` caps the rendered frames |
| `vsyncOff` | `false` | Asks DirectX games to present without V-Sync |
| `hairEnabled` | `true` | Compatible ray-traced hair, in the games that have it. Restart to apply. |

### `diagnostics`

| Key | Default | Values |
| :--- | :---: | :--- |
| `logPerformance` | `false` | Writes frame rate and frame times to a CSV file in `rtx-encore-logs` |
| `logFilesKept` | `3` | Regular logs kept, plus the same number of exception logs, `1` to `100` |

### `keyboardShortcuts`

Each `hotkey…` key takes one or more combinations separated by commas, such as
`"hotkeyFixed4": "Ctrl+Alt+4, Ctrl+Alt+Num4"`. An empty value `""` turns the action off.
`"disableKeybinds": true` turns every shortcut off.

## Other keys

The file holds more keys than this page lists: Smooth Motion, image-quality and compatibility switches, and the
experimental options of the Open engine. Each has its comment in the file and a labelled control in the menu, which
is the recommended place to change them.
