# DLSSG-Transfusion: Universal Multi-Frame Generation Unlock

Universal DLSS Multi-Frame Generation enabler (2x through 6x and Dynamic Mode) for
NVIDIA RTX 40-Series (Ada Lovelace), RTX 30-Series (Ampere) and RTX 20-Series (Turing) GPUs
with runtime **Blackwell Kernel Transfusion**.

This is a fork of [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion)
v1.4.5. Tony's patches on NVIDIA's runtime remain the engine; this fork extends them to older
GPUs and adds the features below.

### What's New in Version v1.4.5.0-rtx20-30-40
First public release of this fork; it gathers the private builds v1.4.5.1 to v1.4.5.3-rtx2030 and the Vulkan work. Full changelog (English and French): [CHANGELOG.md](CHANGELOG.md).

- **RTX 30 (Ampere) support, RTX 20 (Turing) experimental**: architecture gates lowered to the GPU present, Blackwell kernels retargeted, Ampere-only instructions rewritten for Turing. See [docs/RTX30-SM86.md](docs/RTX30-SM86.md).
- **`explained-warp`, our valid-warp policy, by default**: it keeps the warp where it explains what changed between the two frames, which keeps fine detail and clean moving shadows at the same time. Tony's `transfusion` policy remains available. See [docs/QUALITY-POLICY.md](docs/QUALITY-POLICY.md).
- **Faster frame generation, identical image** (`"optimizedKernels"`): −33 % GPU time at 2x, −18 % at 6x on an RTX 3070 Ti Laptop, bit-exact. See [docs/OPTIMIZED-KERNELS.md](docs/OPTIMIZED-KERNELS.md).
- **UI assist** (DX12) and **UI recomposition that can follow the game** (`"autoUiRecomposition"`, automatic for DOOM: The Dark Ages). See [docs/HUD-ASSIST.md](docs/HUD-ASSIST.md).
- **Vulkan**: our kernels through `VK_NVX_binary_import`, adaptive Dynamic mode, overlay drawn by the ReShade add-on. Validated on an RTX 4090 in No Man's Sky and DOOM: The Dark Ages. See [docs/VULKAN.md](docs/VULKAN.md).
- **ReShade settings panel**, `"mode": "game"` by default, remappable shortcuts, optional overlay lines, DLSS render resolution (with live `r.ScreenPercentage` in Unreal Engine games), readable JSON. See [INJECTION.md](INJECTION.md#reshade-settings-panel-optional).
- **Fixes**: Cyberpunk 2077 startup crash; DOOM: The Dark Ages crash with the overlay shown.

Dynamic defaults to a 4x ceiling; the panel's toggle allows experimental 5x and 6x.

Version **1.4.5** brings refined thin-geometry protection (100% tear-free wire fences and grass across up to 6x multipliers with pristine dynamic shadows), overlay stability fixes, and consolidated proxy distribution.

Version **1.4.1** consolidated all proxy DLLs into a single universal `DLSSG-Transfusion.dll`,
adds an in-game multiplier overlay, and improves shadow/translucent quality at high speed.

### What's New in Version 1.4.5
- **Tear-Free Thin Geometry Protection (Fences & Foliage)**: Ported and refined from the RenoDX addon foundation created by **mavismmg** ([mavismmg/MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx)) with candidate agreement firewall. Fine-tuned with a 14% candidate delta margin, `0.6f` scale floor, and `0.98f` geometric warp floor, completely eliminating wire fence and foliage tearing across all camera speeds and multipliers up to 6x.
- **Pristine Moving Shadows (Zero Erosion Dots)**: Dynamic vehicle, bike, and character shadows maintain complete integrity with zero hole-punching, flickering, or eroded shadow dots, even under extreme speeds (tested at 138+ km/h).
- **Zero-Division PTX Kernel Optimization**: Eliminated all 43 slow SFU division instructions (`div.approx`) across stock `Kernel_BlendCandidatesFused` and runtime quality patches, replacing them with bit-exact single-cycle operations for maximum ALU throughput.
- **Loading Screen & Menu Stability**: Defaulted `disableMenuDetection` to `false`, allowing DLSS-G to safely idle at 1x on static menus and loading screens, preventing DXGI device-hang crashes (`0x887A0005` / `0x887A0006`) in Capcom RE Engine (Onimusha, Pragmata, RE4, DD2) and modern titles.
- **Multiplier Overlay Stability & Corner Placement**: Moved DXGI Present hook initialization out of `DLL_PROCESS_ATTACH` into the worker thread, completely preventing loader-lock deadlocks on game startup. Added `DXGI_PRESENT_TEST` bypass, 1x telemetry suspension, and safe fence/buffer guards. Added configurable screen corner placement (`top-left`, `top-right`, `bottom-left`, `bottom-right` via `overlayPosition`) dynamically anchored to DXGI backbuffer dimensions, with real-time `Ctrl + Alt + P` corner cycling.
- **Disable In-Game Keybinds Option**: Added `disableKeybinds` configuration key (`false` by default). When set to `true`, disables all in-game hotkeys and lets DLSS Frame Generation follow game settings or NVIDIA Profile Inspector multipliers directly.
- **Motion Tracing Toggle**: Added `logMotionTracing` config key (off by default) to eliminate log file overhead during normal gameplay.

### What's New in Version 1.4.0
- **DLSS-G Quality Fix (`qualityValidWarp=true`)**:
  - **Tearing & Disocclusion Artifact Elimination**: Injects runtime PTX patches into NVIDIA's `BlendCandidatesFused` optical flow kernel with 100% pure geometric warp on valid motion candidates, eliminating sub-pixel fence and wire tearing without edge smearing.
  - **Calibrated UI Protection & Zero Ghosting**: Strict temporal delta gate ($>0.25f$) and confidence firewalls freeze static HUD and text elements, completely eliminating UI ghosting, smearing, and trailing during rapid camera pans.
- **Native HUDless UI Recomposition (UIR)**: Unlocks Streamline UI Recomposition when games tag separate UI and HUDless buffers (e.g. Neverness to Everness / NTE, Arknights: Endfield). Background frames receive clean geometric warping without wire or fence tearing, while the UI is recomposited crisply at presentation time.
- **dinput8 Proxy Support**: Added `dinput8.dll` wrapper with complete 64-bit export thunk forwarding.
- **Streamline 2.14+ Interposer Detours**: Robust entry point interception on `sl.interposer.dll` eliminating plugin table dispatch deadlocks and loader-lock hangs.
- **Blackwell Kernel Transfusion**: Automatically converts 31 sm_120 fatbin containers to sm_89 at runtime to eliminate cadence micro-stutter under capped refresh rates (e.g. 138 FPS cap + VSync).
- **Camera Rotation Matrix Reconstruction**: Synthesizes missing view/projection matrices when games omit rotation data (`clipToPrevClip` zero/identity), stabilizing midpoint vector warping.
- **Menu Detection Strip**: Bypasses frame generation suppression during HUD/menu interactions without introducing presentation hitches.

## Preset Guide: Preset A vs Preset B (Why Preset B is Recommended)

DLSSG-Transfusion supports two pipeline modes depending on the game's rendering architecture and buffer tags:

| Feature | Preset A (Single-Surface / Non-UIR) | Preset B (UI Recomposition / UIR ON) ⭐ **RECOMMENDED** |
| :--- | :--- | :--- |
| **Pipeline Architecture** | Single flattened surface (HUD + 3D rendered together) | Separated HUDless 3D scene + Recomposited UI buffer |
| **Fence / Wire Tearing** | Substantially reduced compared to stock DLSS-G | **Zero tearing** (100% pure geometric warp on 3D geometry) |
| **HUD & Text Clarity** | **100% Frozen & Crisp** (Calibrated flow firewall) | **100% Crisp & Native** (Overlaid cleanly at presentation) |
| **UI Ghosting / Smearing**| **Zero ghosting** (Strict optical flow cutoff) | **Zero ghosting** (Mathematically impossible) |
| **GPU Performance Impact**| Baseline | Negligible ($\approx 0.10 - 0.25$ ms, $<1$ FPS difference) |
| **Activation Requirement**| Default fallback when game does not separate UI | Automatically engages when HUDless buffer is tagged |

### Why Preset B is Recommended
In **Preset A**, the 2D UI and 3D world are flattened onto the same render buffer before optical flow analysis. **Preset A uses a limited quality fix because of UI protection**: sub-pixel structures (such as thin wire fences, overhead cables, lattice meshes) and anti-aliased font edges share overlapping optical flow correlation values ($0.03 – 0.08f$) and temporal deltas. Pushing the fix any further to eliminate 100% of 1-pixel fence tearing inevitably drags HUD font edges along with background motion, creating visible ghosting trails. Preset A therefore enforces a strict confidence firewall to guarantee that HUD elements remain completely frozen and sharp with zero ghosting, while still noticeably reducing tearing compared to stock DLSS-G.

In **Preset B (UIR ON)**, there is no compromise:
1. The 3D world motion is evaluated entirely on the clean HUDless buffer using **pure 100% geometric warping**, completely eliminating fence, wire, and foliage tearing.
2. The HUD is extracted and composited directly onto the generated frame at presentation time with perfect native clarity and **zero ghosting**.
3. Always choose or enable **Preset B (UIR ON)** whenever supported by the game or mod configuration for the highest possible visual fidelity.

Version 1.3 introduced Blackwell Kernel Transfusion, backporting Blackwell
sm_120 branchless cadence scatter arithmetic to Ada Lovelace sm_89 at runtime to
eliminate micro-stutter drift under capped refresh rates (e.g. 138 FPS cap + VSync).
It also added real-time frame telemetry (rolling FPS, jitter std-dev, 1% lows, and
DLSSG-Transfusion_perf.csv output), crash hardening against D3D12 E_ABORT, and OptiScaler
Flip Metering bypass.

Version 1.2 added multi-game standalone proxy DLLs (`version.dll`, `dxgi.dll`, `winmm.dll`),
in-game hotkeys (`Ctrl + Alt + 2..6`), and automatic game capability limits.

Version 1.1 restores the preserved D157 runtime and separates DLSS-G feature
identity from version eligibility. A loaded module must expose the DLSS-G-specific
`NVSDK_NGX_D3D12_PopulateDeviceParameters_Impl` export before its version is
considered. The tested provider versions are `310.7.0.*`, `310.7.128.*`,
`310.7.129.*`, and `310.8.0.*`; other `310.7.x` builds are not accepted
implicitly.

Streamline 2.12 and 2.13 module layouts are recognized independently of the
DLSS-G provider version. Updating `nvngx_dlssg.dll` does not require matching
versions of `nvngx_dlss.dll`, `nvngx_dlssd.dll`, `nvngx_dlssnr.dll`,
`nvngx_deepdvc.dll`, or the other Streamline DLLs.

This is an unsupported research mod. Modes above 2x may cause artifacts,
latency, frozen presentation, black screens, or crashes. On 8GB GPUs, 2x-3x (or
4x with High/Medium textures) is recommended to prevent VRAM exhaustion.

## Universal Multi-Game Proxy Injection

The mod can be used in **any game** with NVIDIA DLSS Frame Generation and Streamline without requiring Cyber Engine Tweaks:
1. Copy `DLSSG-Transfusion.dll` from `dist/` and rename it to match your game's proxy:
   - `version.dll` (Recommended for most modern games and Unreal Engine 4/5)
   - `dinput8.dll` (Recommended for games utilizing DirectInput8)
   - `dxgi.dll` (For games initializing graphics early)
   - `winmm.dll` (Alternative proxy)
   - `*.asi` (For games with ASI loaders - rename to any `.asi` filename)
   The DLL auto-detects its proxy role from its filename at runtime.
2. Copy the DLL and `DLSSG-Transfusion.json` into the game executable directory.
3. Use in-game hotkeys to switch multipliers on the fly:
   - `Ctrl + Alt + 2..6`: Fixed 2x through 6x multiplier
   - `Ctrl + Alt + PageUp` / `PageDown`: Increment / Decrement multiplier (Fixed Mode)
   - `Ctrl + Alt + D`: Toggle between Fixed Mode and Dynamic Mode
   - `Ctrl + Alt + Up` / `+`: Increase Dynamic MFG target FPS (+5 FPS; hold Shift for 1 FPS fine adjustment)
   - `Ctrl + Alt + Down` / `-`: Decrease Dynamic MFG target FPS (-5 FPS; hold Shift for 1 FPS fine adjustment)
   - `Ctrl + Alt + O`: Toggle the in-game multiplier/state overlay (off by default)
   - `Ctrl + Alt + P`: Cycle overlay corner position across screen corners (top-left, top-right, bottom-right, bottom-left)
   - Set `"mode": "game"` in `DLSSG-Transfusion.json` to let the game (or NVIDIA Profile Inspector) choose the multiplier and Fixed/Dynamic mode. Overlay hotkeys keep working; any multiplier/mode hotkey switches back to Fixed or Dynamic.
4. Detailed diagnostic logs are written directly to `DLSSG-Transfusion.log` in the game directory.
5. See [INJECTION.md](INJECTION.md) for full instructions and troubleshooting.

## Optional ReShade settings panel

Copy `DLSSG-Transfusion.addon64` next to the ReShade DLL (ReShade 6.8+ with full add-on
support). The ReShade overlay then has a **DLSSG-Transfusion** tab that shows the engine
state and edits every setting of `DLSSG-Transfusion.json` directly (nothing goes into
`ReShade.ini`). Settings that are only read at startup are marked *Restart the game to
apply*. See [INJECTION.md](INJECTION.md#reshade-settings-panel-optional).

## Install (Cyberpunk 2077)

Requires Cyberpunk 2077, an RTX 40, 30 or 20 series GPU, and DLSS Frame Generation
enabled. Extract `bin` into the Cyberpunk game directory and merge folders: the
`.asi` in `bin/x64/plugins` is loaded by the ASI loader of Cyber Engine Tweaks (or use
`version.dll` in `bin/x64`). Choose the mode with the hotkeys, `DLSSG-Transfusion.json`
or the optional ReShade panel; the former CET Lua panel has been removed.
If Frame Generation is already active, toggle it Off and On (or restart the
game) so Streamline rebuilds the feature with the requested shape.

## How it works

The ASI intercepts `slGetFeatureFunction`, watches modules actually loaded by
the game, and identifies Streamline and NGX candidates by exports and exact code
signatures. DLSS-G feature identity is established before the supported-version
gate, preventing same-version DLSS-family siblings from being scanned as the
Frame Generation provider. It patches only mapped process memory, never DLLs on
disk, and does not assume NVIDIA cache paths.

The D157 (v1.0) fix targets Ada's midpoint compaction bug: at higher multipliers, generated samples collapse toward the middle of the frame interval instead of occupying their requested temporal positions, producing near duplicate frames. It backports the corrected slot-9 temporal program used by Blackwell in process memory so each generated sample is evaluated at its own evenly spaced position between rendered frames. If the active adapter, provider version, or layout cannot be verified, the patch fails closed to native 2x.

The bridge becomes ready only after the active DLSS G wrapper and loaded NGX
module are verified and patched. It then adjusts `slDLSSGSetOptions` and reads
actual presentation counts through `slDLSSGGetState`.

The approach targets Streamline DLSS G rather than Cyberpunk's renderer. In
another Streamline game, adapt the early DLL loading integration, UI and config
paths, and game/provider specific signatures.

## Build

Requires Visual Studio 2022, CMake 3.24+, and Streamline SDK 2.12.0.

```powershell
cmake -S .\source\native -B .\build -G "Visual Studio 17 2022" -A x64 `
  -DSTREAMLINE_ROOT="C:\path\to\streamline-sdk-v2.12.0"
cmake --build .\build --config Release --parallel
```

The native build writes `DLSSG-Transfusion.dll` (universal proxy) to `build\dist\`.
Rename the DLL to your target proxy name before placing it in the game directory.
To also build the optional ReShade add-on (`DLSSG-Transfusion.addon64`), add
`-DRESHADE_ROOT="C:\path\to\reshade"`: a ReShade v6.8.0 source checkout with its
`deps/imgui` submodule initialised.
The NGX SDK headers are read from `<STREAMLINE_ROOT>/external/ngx-sdk/include`; if your
Streamline SDK keeps them elsewhere, pass `-DNGX_INCLUDE_DIR=...`.

The public repository leaves out a few kernels, which only affects part of `optimizedKernels`; released binaries include them. See [docs/PUBLIC-SOURCE.md](docs/PUBLIC-SOURCE.md).

Breakpoint and deep-kernel research diagnostics are disabled in the normal build.

Logs are written to the temporary directory and include the process ID.

## Credits

- **[dashdogy/RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock)** - Original mod this project is based on, midpoint compaction research, and Streamline in-memory hooking architecture.
- **[mavismmg/MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx)** - Creator and maintainer of the RenoDX MFG Unlock addon, architecture gate bypass, intermediate scatter retention, and the Validated Warp Blend foundation.
- **[TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion)** - Original DLSSG-Transfusion this fork is based on: Blackwell Kernel Transfusion, valid-warp quality fix, UI recomposition, universal proxy and overlay.
- **[sdli1995/dlssg_for_sm86](https://github.com/sdli1995/dlssg_for_sm86)** - DLSS-G on RTX 20/30 research, whose backend served as the reference for the architecture patches and the optimized, bit-exact network and image kernels.

## Disclaimer

Independent project, not affiliated with or endorsed by NVIDIA. This tool injects directly into active game memory, meaning you use it at your own risk. Expect multiplayer anti-cheat systems to flag or block it. Visual quality and performance on officially unsupported hardware may vary and are up to you to evaluate.

## License

Original code in this repository is licensed under the [MIT License](LICENSE).
Reuse and redistribution are permitted provided the copyright and license notice
are retained. NVIDIA Streamline, NGX, Cyberpunk 2077, and other third-party
components remain subject to their respective terms.
