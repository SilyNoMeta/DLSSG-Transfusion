<div align="center">

# DLSS FG and Smooth Motion for RTX 20 / 30 / 40

### DLSSG-Transfusion · Universal frame-generation toolkit

**DLSS Multi-Frame Generation from X2 to X6, Dynamic Mode, image-quality fixes,\
and experimental NVIDIA driver Smooth Motion — delivered as one universal proxy.**

[![Latest release](https://img.shields.io/github/v/release/SilyNoMeta/DLSSG-Transfusion?style=for-the-badge&label=Latest&color=76b900)](https://github.com/SilyNoMeta/DLSSG-Transfusion/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/SilyNoMeta/DLSSG-Transfusion/total?style=for-the-badge&color=2f81f7)](https://github.com/SilyNoMeta/DLSSG-Transfusion/releases)
[![License](https://img.shields.io/github/license/SilyNoMeta/DLSSG-Transfusion?style=for-the-badge&color=8b5cf6)](LICENSE)

[![GPU support](https://img.shields.io/badge/GPUs-RTX%2020%20%7C%2030%20%7C%2040-76b900?style=flat-square&logo=nvidia&logoColor=white)](#gpu--api-status)
[![Graphics APIs](https://img.shields.io/badge/APIs-DX11%20%7C%20DX12%20%7C%20Vulkan-0078d4?style=flat-square&logo=windows11&logoColor=white)](#gpu--api-status)
[![Research mod](https://img.shields.io/badge/Status-Experimental-f59e0b?style=flat-square)](#gpu--api-status)

[![Download v1.4.5.3](https://img.shields.io/badge/Download-v1.4.5.3--rtx20--30--40-76b900?style=for-the-badge&logo=github)](https://github.com/SilyNoMeta/DLSSG-Transfusion/releases/download/v1.4.5.3-rtx20-30-40/DLSSG-Transfusion-v1.4.5.3-rtx20-30-40.zip)
[![Installation guide](https://img.shields.io/badge/Installation-Guide-2f81f7?style=for-the-badge&logo=readthedocs&logoColor=white)](INJECTION.md)
[![Changelog](https://img.shields.io/badge/Release-Changelog-f59e0b?style=for-the-badge)](CHANGELOG.md)

Built on [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) v1.4.5.<br>
Tony's NVIDIA runtime patches remain the engine; this fork extends them to older GPUs and adds its own quality, performance, Vulkan and control layers.

</div>

> [!IMPORTANT]
> This is an unsupported research mod. DLSS Frame Generation features require a game that ships NVIDIA DLSS-G through Streamline. The separate Smooth Motion path is opt-in, RTX 30 only, and locked to the inspected NVIDIA 617.14 driver files. RTX 20 support is experimental, and modes above X2 can increase latency, VRAM use, visual artifacts or instability. Back up every file you replace.

## At a glance

| | Support |
| :--- | :--- |
| **GPUs** | RTX 40 · RTX 30 · RTX 20 *(experimental)* |
| **DLSS Frame Generation** | X2–X6 · game-controlled · fixed · dynamic/adaptive |
| **Driver Smooth Motion** | Experimental FP16 path on RTX 30 · disabled by default · NVIDIA 617.14 only |
| **Graphics APIs** | DLSS FG: DirectX 12 / Vulkan · Smooth Motion: DirectX 11 / 12 / Vulkan |
| **Proxy names** | `version.dll` · `dinput8.dll` · `dxgi.dll` · `winmm.dll` · any `.asi` name |
| **Configuration** | Readable JSON · remappable shortcuts · optional ReShade 6.8+ panel |
| **Current release** | [`v1.4.5.3-rtx20-30-40`](https://github.com/SilyNoMeta/DLSSG-Transfusion/releases/tag/v1.4.5.3-rtx20-30-40) |

## Quick install

1. **Download and extract** the [latest release](https://github.com/SilyNoMeta/DLSSG-Transfusion/releases/latest).
2. Copy `DLSSG-Transfusion.dll` next to the game's rendering executable and rename it to the proxy the game loads — usually `version.dll`.
3. Copy `DLSSG-Transfusion.json` beside it. Existing configuration files, including Tony's, are upgraded automatically while preserving their values.
4. Enable DLSS Frame Generation in the game. If it was already enabled, toggle it off and on or restart the game.

For proxy selection, ReShade, Vulkan runtimes, game-specific notes and troubleshooting, read the **[full installation guide](INJECTION.md)**.

> [!TIP]
> Start with the supplied defaults: `"mode": "game"` lets the game or NVIDIA Profile Inspector choose the multiplier. Dynamic Mode stays at X4 or below unless experimental X5/X6 is explicitly allowed. On 8 GB GPUs, X2–X3 — or X4 with High/Medium textures — is the safer starting point.

## What's new in `v1.4.5.3-rtx20-30-40`

This release redesigns the optional ReShade settings panel. The engine is unchanged from `v1.4.5.2`; only `DLSSG-Transfusion.addon64` differs. It retains every earlier feature, including the Starfield UI-assist hotfix and experimental Smooth Motion on RTX 30. See the [full changelog](CHANGELOG.md) and [dedicated v1.4.5.3 notes](docs/RELEASE-1.4.5.3-rtx20-30-40.md).

| | Highlight | What changed |
| :---: | :--- | :--- |
| 🎛️ | **Redesigned ReShade panel** | Themed layout with toggle switches, sliders, segmented buttons, a status card, collapsible sections, reset buttons (per setting and per section) and restart badges. Defaults match the engine's. |
| 🌀 | **Smooth Motion switch** | The RTX 30 Smooth Motion switch and its graphics-API choice are now in the panel under Compatibility; the driver limits are unchanged. |
| 🩹 | **Starfield UI-assist hotfix** (`v1.4.5.2`) | The complete Streamline tag batch is inspected before making a HUD-less copy, avoiding the redundant D3D12 transition that crashed the reported save. A separate Cyberpunk 2077 regression test also passed. |
| 🌀 | **Smooth Motion on RTX 30** | Opt-in FP16 driver path for D3D11, D3D12 and Vulkan, strictly gated to the inspected NVIDIA 617.14 binaries. Unknown builds, FP8 and RTX 20 are refused. |
| 🟢 | **RTX 30 support** | Architecture gates follow the detected GPU and Blackwell kernels are retargeted to `sm_86`. |
| 🧪 | **RTX 20 support** | Ampere-only instructions are rewritten for `sm_75`. This path has only been validated through RTX 30 emulation and requires DLSS-G 310.9.x. |
| 🌿 | **Cleaner generated frames** | The default `explained-warp` policy preserves more fine detail while keeping moving shadows clean. Tony's `transfusion` policy remains selectable. |
| ⚡ | **Optimized kernels** | Bit-exact image and neural-network kernels reduce measured frame-generation GPU time on the tested RTX 3070 Ti Laptop. |
| 🧭 | **UI assist & recomposition** | DX12 games missing HUD-less/UI tags can receive assisted buffers; native game tags always take priority. |
| 🌋 | **Vulkan path** | Runtime kernels are routed through `VK_NVX_binary_import`, with adaptive Dynamic Mode and a ReShade-drawn overlay. |
| 🎛️ | **Better controls** | Optional ReShade panel, readable JSON, remappable shortcuts, configurable overlay and DLSS render scale. |
| 🛡️ | **Stability fixes** | Fixes include Cyberpunk 2077 startup scanning and DOOM: The Dark Ages Vulkan overlay crashes. |

The affected Starfield user confirmed that the save now loads and DLSS-G works with `uiAssist=true`; this does not establish compatibility with every setup or mod combination. Optional HUD/UI tracing is available live through `diagnostics.logHudUi`, logs only the first three game HUD-less copies and remains off by default. The release contains no game-specific diagnostic build flag. Thanks to [**jay33721**](https://github.com/jay33721) for helping identify the UI-assist error.

### Measured optimization result

With `"optimizedKernels": true`, the specialized kernels produced the same output bit-for-bit in the validation and reduced frame-generation GPU time on an **RTX 3070 Ti Laptop at 1080p**:

| Mode | Reference | Optimized | Difference |
| :---: | ---: | ---: | ---: |
| X2 | 2.33 ms | 1.57 ms | **−33%** |
| X6 | 6.38 ms | 5.23 ms | **−18%** |

These are results from one tested system, not a universal performance guarantee. The optimized path requires DLSS-G 310.9.1. See **[Optimized Kernels](docs/OPTIMIZED-KERNELS.md)** for the protocol and scope.

## GPU & API status

| Target | Status | Validation |
| :--- | :---: | :--- |
| RTX 40 / Ada | ✅ Supported | DX12 and Vulkan paths exercised on RTX 4090 |
| RTX 30 / Ampere | ✅ Supported | DX12 exercised on RTX 3070 Ti Laptop |
| RTX 20 / Turing | 🧪 Experimental | `sm_75` path validated by emulation on RTX 30; no physical RTX 20 validation yet |
| Vulkan on RTX 20/30 | 🧪 Experimental | Vulkan engine validated on RTX 4090; older-GPU Vulkan path still needs physical testing |

Test coverage for this release includes Cyberpunk 2077 (RTX 3070 Ti Laptop, DX12), Black Myth: Wukong (RTX 4090, DX12), No Man's Sky and DOOM: The Dark Ages (RTX 4090, Vulkan). Other games, runtimes and hardware can behave differently.

## Experimental Smooth Motion on RTX 30

Release `v1.4.5.1` introduced an opt-in SM86 FP16 path for NVIDIA driver Smooth Motion; it remains included unchanged in `v1.4.5.3`. Enable it in the `compatibility` section and select the API the game is actually using:

```jsonc
"smoothMotionSm86": true,
"smoothMotionSm86Api": "d3d12" // d3d12, d3d11 or vulkan
```

| API | Observed game test | Result reported during validation |
| :--- | :--- | :--- |
| DirectX 12 | Manor Lords | FrameView showed 120 FPS from a 60 FPS game cap |
| DirectX 11 | Shadows of Doubt | FrameView showed 120 FPS from a 60 FPS game cap |
| Vulkan | Enshrouded | Generated frames worked with the NVIDIA profile set to Off; final FrameView value was not recorded |

These are limited user observations, not a compatibility, latency or visual-quality guarantee. The implementation accepts only the inspected NVIDIA **617.14** files and fails closed on unknown binaries or ambiguous signatures. The Vulkan route additionally requires matching `NvPresent64.dll` and `nvoglv64.dll`. FP8 kernels and RTX 20/Turing are not supported by this path.

Read **[Smooth Motion on RTX 30](docs/SMOOTH-MOTION-SM86.md)** for hashes, gates, evidence and limitations, or the **[French version](docs/SMOOTH-MOTION-SM86.fr.md)**.

## Image quality & UI recomposition

`explained-warp` decides whether motion-vector warping explains the change between two frames before trusting it over NVIDIA's blend. In the project's blind Cyberpunk 2077 comparison, the generated/real ratio for lit patches inside moving shadows dropped from **2.1 to 0.39**, while more fence detail was retained. Set `"qualityPolicy": "transfusion"` to keep Tony's original candidate-agreement policy. See **[Quality Policy](docs/QUALITY-POLICY.md)**.

UI assist (`"uiAssist": true`) can capture a HUD-less scene and synthesize a UI layer on DX12 when the game does not provide them. The game's own tags always win. Automatic UI recomposition can also be disabled with `"autoUiRecomposition": false` so the game keeps full control; this avoids forcing recomposition in games such as DOOM: The Dark Ages where doing so can warp generated frames. See **[HUD Assist](docs/HUD-ASSIST.md)**.

### Preset A vs Preset B

| Feature | Preset A — single surface | Preset B — UI recomposition ⭐ |
| :--- | :--- | :--- |
| Pipeline | HUD and 3D scene are flattened together | Separate HUD-less 3D scene and recomposited UI |
| Thin geometry | Substantially reduced tearing versus stock DLSS-G | Pure geometric warp can eliminate tearing on the clean 3D surface |
| HUD and text | Confidence firewall keeps static UI sharp | Native UI is composited at presentation time |
| Activation | Fallback when the game exposes no separate UI buffers | Automatic when valid HUD-less and UI buffers are tagged |
| Measured GPU cost | Baseline | Approximately 0.10–0.25 ms and under 1 FPS in the original validation |

Preset A must balance one shared optical-flow surface: thin wires, fences, anti-aliased text and HUD edges can occupy similar correlation ranges (`0.03–0.08f`). Its confidence firewall therefore protects UI clarity while reducing geometry artifacts. Preset B removes that conflict by warping the clean 3D surface and compositing the UI separately. Use Preset B when the game or assisted path supplies valid buffers.

## Controls

The supplied configuration defaults to `"mode": "game"`. Any multiplier or mode hotkey leaves game-controlled mode and applies the requested fixed or dynamic mode.

| Shortcut | Action |
| :--- | :--- |
| `Ctrl + Alt + 2…6` | Select fixed X2–X6 |
| `Ctrl + Alt + PageUp / PageDown` | Increase or decrease the fixed multiplier |
| `Ctrl + Alt + D` | Toggle Fixed / Dynamic Mode |
| `Ctrl + Alt + Up / +` | Raise the Dynamic target by 5 FPS (`Shift` for 1 FPS) |
| `Ctrl + Alt + Down / -` | Lower the Dynamic target by 5 FPS (`Shift` for 1 FPS) |
| `Ctrl + Alt + G` | Return control to the game / NVIDIA Profile Inspector |
| `Ctrl + Alt + O` | Toggle the multiplier/state overlay |
| `Ctrl + Alt + P` | Cycle the overlay corner |

Set `"disableKeybinds": true` to disable every shortcut. Shortcuts are remappable in the JSON or optional panel and only act while the game has focus.

Dynamic Mode uses native Streamline control on compatible DX12 integrations. Because Streamline does not expose the same mode on Vulkan, the Vulkan path uses an adaptive controller that selects X2–X6 from frame rate and target.

## Universal proxy

One `DLSSG-Transfusion.dll` handles every supported loader role by detecting its own filename. Install **one copy only**.

| Rename to | Use case |
| :--- | :--- |
| `version.dll` | Recommended first choice, including most Unreal Engine 4/5 games |
| `dinput8.dll` | Games that load DirectInput 8 |
| `dxgi.dll` | Games that load DXGI early — do not overwrite an existing ReShade `dxgi.dll` |
| `winmm.dll` | Alternative WinMM proxy route |
| `anything.asi` | Games with an ASI loader |

Diagnostic output is written to `DLSSG-Transfusion.log` in the game directory. The full proxy export notes and game-specific guidance are in **[INJECTION.md](INJECTION.md)**.

## Optional ReShade panel

With ReShade 6.8+ and full add-on support, copy `DLSSG-Transfusion.addon64` beside the ReShade DLL. The **DLSSG-Transfusion** tab can edit and save every JSON setting, show engine state, and draw the Vulkan overlay. Settings read only at startup are marked accordingly.

The panel can also expose optional lines for UIR, HUD-less/UI-alpha sources, DLSS/DLSS-G/Streamline versions, frame pacing, GPU, VRAM and debug data. `"dlssRenderScale"` supports DLAA through Ultra Performance or a custom scale; Unreal Engine 4/5 games can additionally receive live `r.ScreenPercentage` control when they already use DLSS.

Engine and add-on must always come from the same release.

## Cyberpunk 2077

Either use the universal proxy as `version.dll` in `bin/x64`, or extract the release's `bin` layout into the game directory and let Cyber Engine Tweaks load the `.asi` from `bin/x64/plugins`. The former CET Lua panel has been removed; use the JSON, hotkeys or optional ReShade panel.

## How it works

The proxy intercepts `slGetFeatureFunction`, watches modules actually loaded by the game, and identifies Streamline and NGX candidates through exports and exact code signatures. DLSS-G feature identity is established before version eligibility so same-version DLSS-family modules are not mistaken for the frame-generation provider. Only mapped process memory is patched; DLLs on disk and assumed NVIDIA cache paths are not used.

The preserved D157 fix addresses Ada midpoint compaction at higher multipliers, where generated samples can collapse toward the middle of the frame interval. It backports the corrected Blackwell slot-9 temporal program so each generated sample is evaluated at its requested position. If the active adapter, provider or layout cannot be verified, the patch fails closed to native X2.

Blackwell `sm_120` cadence kernels are retargeted at runtime for the active architecture. RTX 20 additionally receives rewrites for Ampere-only instructions. On Vulkan, the engine routes compatible binary modules through `VK_NVX_binary_import`. Once the wrapper and NGX provider are verified and patched, the bridge adjusts `slDLSSGSetOptions` and reads real presentation counts through `slDLSSGGetState`.

Streamline 2.12 and 2.13 layouts are recognized independently from the DLSS-G provider version. Updating `nvngx_dlssg.dll` does not require matching versions of `nvngx_dlss.dll`, `nvngx_dlssd.dll`, `nvngx_dlssnr.dll`, `nvngx_deepdvc.dll` or other Streamline modules. Tested provider families include `310.7.0.*`, `310.7.128.*`, `310.7.129.*` and `310.8.0.*`; unrelated `310.7.x` builds are not accepted implicitly.

## Earlier upstream milestones retained

<details>
<summary><strong>DLSSG-Transfusion 1.4.5 — quality and stability work</strong></summary>

- Thin-geometry protection refined from the Validated Warp Blend foundation by [mavismmg](https://github.com/mavismmg/MFGAdaUnlock-RenoDx), with a 14% candidate margin, `0.6f` scale floor and `0.98f` geometric-warp floor. The original validation reported tear-free wire fences and grass through X6.
- Moving-shadow protection against hole-punching, flicker and erosion artifacts, including the original high-speed 138+ km/h test case.
- All 43 `div.approx` sites in the stock and quality-policy `Kernel_BlendCandidatesFused` paths replaced by reciprocal/multiply forms.
- `disableMenuDetection=false` by default so DLSS-G can idle at X1 on static menus and loading screens, avoiding observed DXGI device hangs in RE Engine and other titles.
- DXGI Present hook initialization moved out of `DLL_PROCESS_ATTACH`, plus `DXGI_PRESENT_TEST` bypass, X1 telemetry suspension and fence/buffer guards.
- Overlay placement in every screen corner with live `Ctrl + Alt + P` cycling.
- `disableKeybinds` and opt-in `logMotionTracing` configuration switches.

</details>

<details>
<summary><strong>DLSSG-Transfusion 1.4.0 — valid warp, UIR and universal injection</strong></summary>

- Runtime PTX patches (`"qualityValidWarp": true`) for the optical-flow blend kernel, with geometric warping on valid motion candidates and a strict temporal/UI confidence gate (`>0.25f` temporal delta in the original policy).
- Native HUD-less UI recomposition when games tag separate 3D and UI buffers, including integrations such as Neverness to Everness and Arknights: Endfield.
- Complete 64-bit `dinput8.dll` forwarding and Streamline 2.14+ interposer detours.
- Runtime Blackwell-to-Ada kernel transfusion, missing camera-matrix reconstruction and menu-detection handling.

</details>

Version 1.4.1 consolidated the proxy routes into one universal DLL, added the multiplier overlay and improved moving-shadow/translucent quality at speed. Version 1.3 added Blackwell branchless cadence scatter arithmetic on Ada, frame telemetry (rolling FPS, jitter deviation, 1% lows and CSV output), D3D12 `E_ABORT` hardening and OptiScaler Flip Metering bypass. Version 1.2 added the standalone proxy routes, hotkeys and automatic capability limits. Version 1.1 restored the preserved D157 runtime and separated DLSS-G feature identity from version eligibility.

## Documentation

| Guide | English | Français |
| :--- | :---: | :---: |
| Installation, configuration & troubleshooting | [Open](INJECTION.md) | — |
| Current release — `v1.4.5.3` | [Open](docs/RELEASE-1.4.5.3-rtx20-30-40.md) | [Ouvrir](docs/RELEASE-1.4.5.3-rtx20-30-40.md#français) |
| Complete changelog | [Open](CHANGELOG.md) | [Ouvrir](CHANGELOG.md#français) |
| Smooth Motion on RTX 30 | [Open](docs/SMOOTH-MOTION-SM86.md) | [Ouvrir](docs/SMOOTH-MOTION-SM86.fr.md) |
| RTX 30 / `sm_86` architecture | [Open](docs/RTX30-SM86.md) | [Ouvrir](docs/RTX30-SM86.fr.md) |
| Quality policy | [Open](docs/QUALITY-POLICY.md) | [Ouvrir](docs/QUALITY-POLICY.fr.md) |
| Optimized kernels | [Open](docs/OPTIMIZED-KERNELS.md) | [Ouvrir](docs/OPTIMIZED-KERNELS.fr.md) |
| HUD assist | [Open](docs/HUD-ASSIST.md) | [Ouvrir](docs/HUD-ASSIST.fr.md) |
| Vulkan | [Open](docs/VULKAN.md) | [Ouvrir](docs/VULKAN.fr.md) |
| Public source scope | [Open](docs/PUBLIC-SOURCE.md) | [Ouvrir](docs/PUBLIC-SOURCE.fr.md) |

## Build from source

Requires Visual Studio 2022, CMake 3.24+ and Streamline SDK 2.12.0.

```powershell
cmake -S .\source\native -B .\build -G "Visual Studio 17 2022" -A x64 `
  -DSTREAMLINE_ROOT="C:\path\to\streamline-sdk-v2.12.0"
cmake --build .\build --config Release --parallel
```

The build writes the universal `DLSSG-Transfusion.dll` to `build\dist`. To also build `DLSSG-Transfusion.addon64`, pass `-DRESHADE_ROOT=...` pointing to a ReShade 6.8.0 source checkout with `deps/imgui` initialized. NGX headers are read from `<STREAMLINE_ROOT>/external/ngx-sdk/include`; use `-DNGX_INCLUDE_DIR=...` for another layout.

The public repository intentionally omits a few kernels, affecting part of `optimizedKernels`; release binaries include the complete set. See **[Public Source Scope](docs/PUBLIC-SOURCE.md)**. Breakpoint and deep-kernel research diagnostics are disabled in normal builds. Build logs go to the temporary directory and include the process ID.

## Credits

- **[TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion)** — original project and the core Blackwell Kernel Transfusion, valid-warp, UI recomposition, proxy and overlay work.
- **[dashdogy/RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock)** — midpoint-compaction research and Streamline in-memory hooking architecture.
- **[mavismmg/MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx)** — architecture bypass, intermediate-scatter retention and the Validated Warp Blend foundation.
- **[sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86)** — RTX 20/30 DLSS-G research and the reference for architecture patches and optimized kernels.

## Disclaimer & license

This independent project is not affiliated with or endorsed by NVIDIA. It injects into active game memory and may be blocked or flagged by multiplayer anti-cheat systems. Use it at your own risk. Visual quality, stability and performance on unsupported hardware vary by game and system.

Original code in this repository is licensed under the [MIT License](LICENSE). Redistribution must retain the applicable copyright and license notice. NVIDIA Streamline, NGX, games and all other third-party components remain subject to their own terms.
