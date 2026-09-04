# RTX 40 MFG Unlock for Cyberpunk 2077

Experimental Cyber Engine Tweaks mod providing fixed 2x through 6x and Dynamic
DLSS Frame Generation controls on RTX 40 series GPUs. The 5x and 6x modes are
especially experimental.

Dynamic defaults to a 4x ceiling. Its UI toggle allows experimental 5x and 6x.
UI recomposition is requested only when matching HUDless and UI buffers are tagged.
The panel reports rendered FPS and total DLSS output FPS.

Version 1.3 introduces Blackwell Kernel Transfusion, backporting Blackwell
sm_120 branchless cadence scatter arithmetic to Ada Lovelace sm_89 at runtime to
eliminate micro-stutter drift under capped refresh rates (e.g. 138 FPS cap + VSync).
It also adds real-time frame telemetry (rolling FPS, jitter std-dev, 1% lows, and
RTX40MFG_perf.csv output), crash hardening against D3D12 E_ABORT, and OptiScaler
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
1. Choose one proxy DLL from `dist/`:
   - `version.dll` (Recommended for most modern games and Unreal Engine 4/5)
   - `dxgi.dll` (For games initializing graphics early)
   - `winmm.dll` (Alternative proxy)
   - `RTX40MFG.asi` (For games with ASI loaders)
2. Copy the DLL into the game executable directory.
3. Use in-game hotkeys to switch multipliers on the fly:
   - `Ctrl + Alt + 2..6`: Fixed 2x through 6x multiplier
   - `Ctrl + Alt + PageUp` / `PageDown`: Increment / Decrement multiplier (Fixed Mode)
   - `Ctrl + Alt + D`: Toggle between Fixed Mode and Dynamic Mode
4. Detailed diagnostic logs are written directly to `RTX40MFG.log` in the game directory.
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

The D157 (v1.0) fix targets Ada’s midpoint compaction bug: at higher multipliers, generated samples collapse toward the middle of the frame interval instead of occupying their requested temporal positions, producing near duplicate frames. It backports the corrected slot-9 temporal program used by Blackwell in process memory so each generated sample is evaluated at its own evenly spaced position between rendered frames. If the active adapter, provider version, or layout cannot be verified, the patch fails closed to native 2x.

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

The native build writes `build\Release\RTX40MFG.asi`. The CET UI and its
FPS/status client are tracked at
`bin\x64\plugins\cyber_engine_tweaks\mods\RTX40MFG\init.lua`. Breakpoint and
deep-kernel research diagnostics are disabled in the normal build.

Logs are written to the temporary directory and include the process ID.

## License

Original code in this repository is licensed under the [MIT License](LICENSE).
Reuse and redistribution are permitted provided the copyright and license notice
are retained. NVIDIA Streamline, NGX, Cyberpunk 2077, and other third-party
components remain subject to their respective terms.
