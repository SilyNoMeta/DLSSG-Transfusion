# Vulkan: kernel transport through VK_NVX_binary_import

*Français : [VULKAN.fr.md](VULKAN.fr.md)*

Validated on an RTX 4090 (Ada) in No Man's Sky and DOOM: The Dark Ages (table at the end). **Not validated yet on RTX 30 / RTX 20**, the first case where images are rewritten for the architecture.

## Why Vulkan needs its own route

The provider (`nvngx_dlssg.dll`) creates its CUDA modules through a different API in each renderer:

| Renderer | Module creation | Kernel launch | Our hook |
|---|---|---|---|
| D3D12 | `NvAPI_D3D12_CreateCuModule` | `NvAPI_D3D12_LaunchCuKernelChain` | `cu_module_hook.h`, `network_optimizer.h` |
| Vulkan | `vkCreateCuModuleNVX` | `vkCmdCuLaunchKernelNVX` | `vulkan_nvx.h` |

Everything done at provider load does not depend on the renderer and was already active in Vulkan: architecture gates, minimum architecture (`NVSDK_NGX_VULKAN_GetFeatureRequirements` included), Turing network selector, in-place PTX retarget, Blackwell Transfusion and the valid-warp policy. What was missing is everything done when a module is created: sm_75 lowering and the exact image kernels (`optimizedKernels`).

## How it works

The design follows the transport that dlssg_for_sm86 validated in No Man's Sky (`src/companion/vulkan_transport.hpp`, adapted from RTX-Unlocker-RenoDX), without its SHA256 tables, fixed RVAs or precompiled kernels: the images come from our own engine, as in D3D12.

1. **At provider load** (`vulkan_nvx::Install`, called next to the other provider patches):
   - its `GetProcAddress` import is redirected, for the lookups it makes itself (`vkGetDeviceProcAddr` from `vulkan-1.dll`);
   - its `NVSDK_NGX_VULKAN_Init_Ext2` export is detoured to an assembly thunk (`vulkan_nvx_thunks.asm`). The thunk rewrites the `vkGetInstanceProcAddr` / `vkGetDeviceProcAddr` arguments in place, then **jumps** to the original: the provider still sees NGX's return address (it validates its caller; a call would change it).
2. **Wrapped resolvers** give the provider our `vkCreateCuModuleNVX` and `vkCmdCuLaunchKernelNVX`. Any other module gets the driver's functions unchanged.
3. **Module creation** uses `cu_module_hook::Replacement`, the D3D12 code path: same images, same cache.
   - A fatbin is passed with the size its header declares: Transfusion redirects descriptors to larger rebuilt fatbins while the provider keeps passing the original size, and NVX takes an explicit size.
   - If the driver refuses a rewritten image, the provider's own image is tried (behavior of an unmodified runtime). dlssg_for_sm86 saw NVX refuse a modified blend PTX on driver 616.92; the log shows it if it happens here.
4. **At the first Vulkan `Init_Ext2`**, the provider is pinned in memory so that the detour and the redirected import can never outlive it. In D3D12 nothing is pinned and the detour is never reached.
5. **Candidate providers.** NGX loads the game's, the NGX cache's and the driver store's `nvngx_dlssg`, keeps one and unloads the others, sometimes twice at new addresses (seen in No Man's Sky). The transport is installed on each; a slot whose provider is gone is reused at once instead of waiting for the periodic module scan.

## Reading the log

```text
[VK-NVX] transport installed on ...\nvngx_dlssg.dll: Init_Ext2 detoured, 1 GetProcAddress import(s) redirected
[VK-NVX] NVSDK_NGX_VULKAN_Init_Ext2 reached (provider ...): no resolvers passed, the provider looks them up (redirected import), provider pinned=1
[VK-NVX] module #0 fatbin ptx86 size=... -> rewritten status=0 (accepted=1 rejected=0 rewritten=1 resized=0)
[VK-NVX] 1 provider kernel launches recorded
```

- `no resolvers passed`: the case seen in No Man's Sky; the redirected import routes NVX. `resolvers wrapped`: NGX passed them and the thunk wrapped them.
- `-> rewritten`: our image (lowering or exact image kernel) was accepted.
- `-> size from fatbin header`: a descriptor redirected by Transfusion.
- `-> rewritten image refused, provider image used`: NVX refused our image; to report.
- `status` other than 0: the driver refused the module; to report with the whole log.

Without `NVSDK_NGX_VULKAN_Init_Ext2 reached`, the NGX core used another entry point: only the redirected import can still route the NVX functions. To report with the log.

## Features in Vulkan

| Feature | Vulkan |
|---|---|
| X2–X6, hotkeys, `game` mode, ceiling | Streamline hooks, independent of the renderer |
| Architecture gates, Turing network selector | Patched at load, by signature |
| In-place retarget, Blackwell Transfusion, valid-warp policy | At load; now passed with the right size |
| sm_75 lowering, exact image kernels | Through NVX (`vulkan_nvx.h`) |
| DL1/DL2 network optimizer and launch fusions | Not ported (D3D12 launch API only) |
| Dynamic mode | Adaptive controller (below): fixed multipliers X2–X6 chosen from the frame rate; NVIDIA's Dynamic MFG stays D3D12 only |
| DLSS render resolution | D3D12 NGX exports only for now; `r.ScreenPercentage` (Unreal) works |
| Multiplier overlay | Drawn by the ReShade add-on (below); the engine's own overlay is DXGI only |
| UI assist | D3D12 only |

## Overlay in Vulkan

The engine draws its overlay through DXGI, which Vulkan games do not use. The ReShade add-on (`DLSSG-Transfusion.addon64`, ReShade 6.8 with full add-on support) now draws the same overlay with ReShade's ImGui on every frame, menu closed, whenever the engine's overlay has not drawn for a second. In D3D12 the engine's overlay keeps drawing and the add-on stays hidden: never two overlays.

- Same switch (`"showOverlay"`, `Ctrl + Alt + O`), corner (`"overlayPosition"`, `Ctrl + Alt + P`) and extra lines as the engine's overlay; the text comes from the engine (`DLSSGTransfusion_GetOverlay`).
- First line `presented/base fps Nx`: *base* is the game's frame rate, counted from its unique `slSetConstants` frame tokens; *presented* is that rate times the multiplier DLSS-G reports. The DXGI overlay counts presents instead.
- The frame pacing line needs presents counted through DXGI. In No Man's Sky it appears: the NVIDIA driver presents that Vulkan game through a DXGI swapchain, which the engine sees. Where a Vulkan game presents otherwise, the line is absent.
- The engine never draws on that driver swapchain, and keeps no reference to its buffers, device or queue: it only times its presents. Drawing on it made DOOM: The Dark Ages crash as soon as the driver rebuilt it (loading a save, changing the multiplier). Log line: `Native overlay: swapchain created by nvoglv64.dll (Vulkan/OpenGL), frame pacing only; the ReShade add-on draws the overlay`.

## Dynamic mode in Vulkan: the adaptive controller

Streamline offers no native Dynamic MFG in Vulkan. When `"mode": "dynamic"` is selected (or `Ctrl + Alt + D`) and the provider was initialized for Vulkan, the engine keeps submitting **fixed** multipliers and chooses them itself (`adaptive_policy.h`, ported from dlssg_for_sm86 where it was validated in No Man's Sky). D3D12 is unchanged: NVIDIA's Dynamic MFG.

- **Measure**: one sample per game frame (`slSetConstants` frame token), filtered over ~0.6 s. It is the source-frame cadence, which already includes the cost of frame generation; output is estimated as cadence × multiplier, not counted at presentation.
- **Decision**: after 20 samples and 0.75 s, and at least 1.5 s after the previous change: one factor up if the output is below 97 % of the target, one factor down if one factor less still reaches 99.5 % of it. Ceiling X4, or X6 with `"dynamicExperimental56": true`. Target: `"dynamicTargetFrameRate"`, or the refresh rate of the monitor when `0`.
- **Useless raise**: if a raise does not improve the estimated output by 1 % (a capped game), it is undone and raising waits 5 s, then 10, 20 and 40 s after each further useless raise; a new target lifts the wait.
- **Change from dlssg_for_sm86**: the cadence estimate restarts after each accepted change. Kept across a change, it still carried ~8 % of the previous cadence 1.5 s later, which made a useless raise look like a gain: a capped game climbed to X6.
- **Safety**: a change is submitted on the game's thread by the usual reapply path. A multiplier refused by Streamline pauses the controller until settings change or frame generation is turned off and on. A game reset, a skipped frame token or an interval outside 1–200 ms (loading, pause) restarts the measure, not the multiplier.

The NVIDIA overlay then shows the fixed multiplier chosen (for example 4x), not a dynamic range: this is expected. Log lines:

```text
[ADAPTIVE] Vulkan adaptive MFG at X4 (ceiling X6, target 240 FPS)
[ADAPTIVE] X4 -> X5 (source 49.3 FPS, target 240 FPS)
```

## Validation

| Test | Result |
|---|---|
| `tests/vulkan_nvx`: the test executable plays the provider (exports `Init_Ext2`, imports `GetProcAddress`) in front of a fake driver. Thunk (NGX's return address and all nine arguments preserved, resolvers wrapped), rewritten image with `pNext` kept, fallback when refused, fatbin header size (and its refusal when the range is not readable), launch counting, import redirection, pass-through for other modules, reinstall after reload, slot freed on unload | All passed (MinGW build under Wine; the MSVC/MASM build is the reference) |
| No Man's Sky, RTX 4090 (Ada), game's DLSS-G runtime (legacy `dlfg_kernel`, older than 310.9.1) | Transport active through the redirected import; 128 modules created, **0 refused**; the midpoint fix's rebuilt `dlfg_kernel` passed with its header size (98408 → 127200 bytes); over 100,000 provider launches; presentation 6x then 3x as requested by the game. No image rewritten: the exact image kernels need 310.9.1, and Ada needs no lowering |
| Vulkan game on RTX 30 / RTX 20 | **To do** (first case with rewritten images) |
| `tests/adaptive_policy`: controller against a simulated game. Each factor X2–X6 reached from X2 and from X6 (without cost: exactly; with cost: never below the cheapest sufficient factor, at most one above), ceiling, repeated tokens, resets/skips/stalls, capped game (no climb, waits 5/10/20/40 s, new target), refused factor | All passed |
| No Man's Sky, RTX 4090, NGX cache runtime (`dlssg` version 20318464, image kernels identical to 310.9.1) | **Blackwell Transfusion through NVX**: 31 containers retargeted to sm_89, 16 descriptors redirected; the three exact image kernels rewritten (BlendCandidatesFused, OutputPull, OutputPushFine); 64 modules, **0 refused** |
| Same session, adaptive mode | Target 144 FPS (display): X3 at 55–60 source FPS. Target 403 FPS: X3 → X4 → X5 → X6 at 1.5 s intervals (source 58.5 → 49.0 FPS); ceiling X4 as soon as *Allow 5x and 6x* is off; back to X3 when the target returns to 144. Every change accepted by Streamline and reported by DLSS-G (actual 3x/4x/5x/6x) |
| Same session, overlay drawn by the ReShade add-on | Shown top right with its extra lines; first line `249/42 fps 6x`, `219/55 fps 4x`, `181/60 fps 3x`, consistent with ReShade's own counter (≈252, 215, 180 FPS) |
| DOOM: The Dark Ages, RTX 4090, game's runtime 310.9.1 and Streamline 2.14.1, 4x at ~39 source FPS in 4K | The game tags HUD-less and UI (color and alpha) buffers without asking for UI recomposition. With UIR forced by the engine, every generated frame is distorted in motion; with UIR following the game (`autoUiRecomposition: false`, now automatic for this game), generated frames are clean. Transport: 128 modules, 0 refused, exact image kernels rewritten; `Init_Ext2` reached through the redirected import, as in No Man's Sky |
| DOOM: The Dark Ages, same setup, overlay shown | No more crash when loading a save or changing the multiplier: the native overlay only times the driver's DXGI swapchain (`nvoglv64.dll`) and the ReShade add-on draws the overlay (`118/39 fps 3x`, `UIR OFF` with no manual setting, frame pacing line present) |
