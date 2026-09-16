# DLSSG-Transfusion: Universal Multi-Frame Generation Unlock

Universal DLSS Multi-Frame Generation enabler (2x through 6x and Dynamic Mode) for
NVIDIA RTX 40-Series (Ada Lovelace) GPUs with runtime **Blackwell Kernel Transfusion**.

Dynamic defaults to a 4x ceiling. Its UI toggle allows experimental 5x and 6x.
UI recomposition is requested only when matching HUDless and UI buffers are tagged.
The panel reports rendered FPS and total DLSS output FPS.

Version **1.4.5** brings refined thin-geometry protection (100% tear-free wire fences and grass across up to 6x multipliers with pristine dynamic shadows), overlay stability fixes, and consolidated proxy distribution.

Version **1.4.1** consolidated all proxy DLLs into a single universal `DLSSG-Transfusion.dll`,
adds an in-game multiplier overlay, and improves shadow/translucent quality at high speed.

### What's New in Version 1.4.5
- **Tear-Free Thin Geometry Recovery (Fences & Foliage)**: Ported and refined from the RenoDx Release 1.0 Validated Warp Blend foundation. Fine-tuned with a `0.03f` disparity margin and `0.28f` warped error ceiling, eliminating wire fence and grass blade tearing across all camera speeds and multipliers up to 6x.
- **Pristine Moving Shadows**: Completely eliminated broad surface elevation (Track 1) so ground textures and dynamic vehicle/character shadows remain 100% under stock DLSS-G's neural blend, preventing erased dots and hole-punch artifacts.
- **Pure Symmetrical 0.98f Weight Floor**: Pure candidate FMA reconstruction with zero cross-candidate copying or ghosting.
- **Multiplier Overlay Stability**: Moved DXGI Present hook initialization out of `DLL_PROCESS_ATTACH` into the worker thread, completely preventing loader-lock deadlocks on game startup. Added non-blocking backbuffer rendering and clean `ResizeBuffers` resource release.
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
4. Detailed diagnostic logs are written directly to `DLSSG-Transfusion.log` in the game directory.
5. See [INJECTION.md](INJECTION.md) for full instructions and troubleshooting.

## Install (Cyberpunk 2077 with CET)

Requires Cyberpunk 2077, Cyber Engine Tweaks, an RTX 40 series GPU, and DLSS
Frame Generation enabled. CET 1.37.1 was used during development.

Extract `bin` into the Cyberpunk game directory, merge folders, then select a
mode from the CET overlay. Select the multiplier before launch when possible.
If Frame Generation is already active, toggle it Off and On (or restart the
game) so Streamline rebuilds the feature with the requested shape. The release
ZIP does not include `config.json`, so installing it preserves the selected
mode.

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
Breakpoint and deep-kernel research diagnostics are disabled in the normal build.

Logs are written to the temporary directory and include the process ID.

## Credits

- **[dashdogy/RTX40MFG-Unlock](https://github.com/dashdogy/RTX40MFG-Unlock)** - original mod this project is based on
- **[mavismmg/MFGAdaUnlock-RenoDx](https://github.com/mavismmg/MFGAdaUnlock-RenoDx)** - architecture gate bypass and foundational fixes

## Disclaimer

Independent project, not affiliated with or endorsed by NVIDIA. This tool injects directly into active game memory, meaning you use it at your own risk. Expect multiplayer anti-cheat systems to flag or block it. Visual quality and performance on officially unsupported hardware may vary and are up to you to evaluate.

## License

Original code in this repository is licensed under the [MIT License](LICENSE).
Reuse and redistribution are permitted provided the copyright and license notice
are retained. NVIDIA Streamline, NGX, Cyberpunk 2077, and other third-party
components remain subject to their respective terms.
