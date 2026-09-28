# NVIDIA driver Smooth Motion on RTX 30 (SM86)

*Français : [SMOOTH-MOTION-SM86.fr.md](SMOOTH-MOTION-SM86.fr.md)*

Status on September 28, 2026: **reproducible static analysis; no activation is
shipped**.

This study targets NVIDIA driver 617.14 `NvPresent64.dll` (SHA-256
`1716d13d169320a2dc71486733202ed75127e8f39cd679a9193848fc1fa45e7f`) on an
RTX 3070 Ti Laptop. It is separate from the `nvngx_dlssg.dll` runtime already
handled by Transfusion.

The NVIDIA binary is not stored in this repository. Run the read-only inspector
against the installed file:

```powershell
py -3 scripts\analyze_nvpresent.py `
  "C:\path\to\NvPresent64.dll" `
  --expect-sha256 1716d13d169320a2dc71486733202ed75127e8f39cd679a9193848fc1fa45e7f
```

The complete non-proprietary manifest is
[`evidence/nvpresent-617.14.json`](evidence/nvpresent-617.14.json).

## Findings

- There are 37 fatbins, each with one SM89 ELF and one SM120 ELF, and no PTX.
- The 20 FP16 SM89 kernels can be metadata-retargeted to SM86 in this exact
  build. Their decoded instructions, encodings, register counts and shared
  memory fit Ampere.
- The 17 `*_fp8` kernels cannot run on SM86. They contain
  `QMMA.16832.F16.E4M3.E4M3`; E4M3 conversions also decode incorrectly.
- Turing cannot use this path: no PTX fallback exists and the FP16 SASS uses
  SM80-era `HMMA.16816`, `LDGSTS` and `REDUX`.
- All SM89 ELF flags are `0x06005904` in this build. A general implementation
  must change only the architecture field to 86, not overwrite all flags with
  a constant.

## Initialization map

`NVP_Init_D3D` at `+0x59e0` evaluates the global configuration with D3D API mask
3, checks the cached effective-enable byte, then initializes DXGI hooks only if
DX11 (`config+0x4f`) or DX12 (`config+0x50`) is allowed. The routine at
`+0x37960` loads `dxgi.dll`, resolves `CreateDXGIFactory2`, and works on factory
vtable slots 10 (`CreateSwapChain`) and 15 (`CreateSwapChainForHwnd`). It is not
the CUDA initializer.

The confirmed configuration fields are:

| Offset | Meaning |
|---:|---|
| `+0x48` | mask tested against the requested API mask |
| `+0x4c` | override for that mask veto, not the main Smooth Motion boolean |
| `+0x4e/+0x4f/+0x50` | Vulkan/DX11/DX12 allowed |
| `+0xe9` | `Smooth Motion enabled`, confirmed by NvPresent's own log string |
| `+0x12a5` | cached effective enable |

The evaluation at `+0xa05b` is equivalent to:

```text
stage = ((mask & requested_api_mask) != 0 && override == 0)
      ? false : smooth_motion_enabled
effective = prerequisite_1 && prerequisite_3 && stage
```

The device gate starts at `+0xc400` and compares `[device+0x14]` with 3 at
`+0xc41c`. The getter at `+0xbd10` returns the same field. The precision
selector at `+0x7ff90` compares its architecture input with 3 at `+0x7ffce`, so
a real Ampere device value of 2 stays on FP16.

The safe Ampere gate patch is only the immediate at `+0xc41f`, from 3 to 2.
Forcing the later `SETGE SIL` to true removes the Turing barrier and is neither
necessary nor acceptable.

The CUDA resolver at `+0x1348d0` wraps `InitOnceExecuteOnce`. Its callback loads
`nvcuda.dll` from the system search path, resolves `cuGetProcAddress_v2` (with a
fallback), and fills CUDA dispatch groups. A local probe showed that the private
slot points into `nvcuda64.dll` in the same DriverStore package; `nvcuda.dll`
exposes a forwarding export. The prototype verifies that the target is executable
and belongs to one of these trusted modules.

## NVSmooth30 review and integration rules

The inspected upstream revision is
[`21fa435`](https://github.com/ItsAdeline/NVSmooth30/commit/21fa43521c2753ffd5c8e9d2092cfc79f9c6c456).
It has improved structural scans, rollback and bridge diagnostics, but it still
retargets all 37 SM89 cubins, overwrites ELF flags with a constant, forces the
gate result true, uses private resolver/slot and wrapper-vtable offsets, selects
the DriverStore DLL by timestamp, creates the D3D12 bridge device on the default
adapter first, and does not propagate HDR color-space state.

Transfusion should therefore:

1. validate native D3D12 first, with D3D11 bridging as a separate milestone;
2. adopt the already-loaded NvPresent module or resolve the package associated
   with the game's NVIDIA adapter;
3. require a coherent structural fingerprint and apply a transactional patch;
4. leave `SETGE` intact and keep the real architecture field at 2;
5. retarget only an explicit allowlist of the 20 FP16 kernels, preserve ELF flag
   bits, and leave FP8 and SM120 untouched;
6. fail closed if an FP8 module is requested or any signature is ambiguous;
7. reject Turing before patching;
8. log the active path, SHA, adapter LUID, patches, module names, CUDA results,
   and rollback.

Broad compatibility validation still requires a real RTX 3070 Ti Laptop game test, 300+ generated
frames, resize/alt-tab/fullscreen coverage, proof that FP8 is never selected,
and identical-scene frametime/1%-low and image-quality comparisons. Successful
kernel execution alone is not a performance or quality result.

An opt-in prototype now lives in `source/native/smooth_motion_sm86.cpp`.
Set `smoothMotionSm86` to `true` in the `compatibility` section of
`DLSSG-Transfusion.json` and restart a native D3D12 game on an Ampere GPU.
`smoothMotionSm86Api` defaults to `d3d12`; explicit `d3d11` and `vulkan`
values select separate experimental backends, each now observed working in one
game; see the D3D11 and Vulkan sections below.
It is disabled by default and accepts only the inspected 617.14 binary by
SHA256 and structural signatures. Its CUDA interception retargets only the
20 FP16 modules in memory; the real-driver parser test confirms 20 rewrites,
17 FP8 refusals and exactly two changed bytes per FP16 cubin. No game execution
or image-quality result had been verified at prototype build time. A subsequent
Manor Lords D3D12 user test on the RTX 3070 Ti Laptop reported FrameView
"MFG 2X" and 120 FPS with the game capped at 60 FPS and native frame generation
disabled. Turning off both the NVIDIA profile and JSON option returned to
60 FPS; a further run with the NVIDIA profile Off and `smoothMotionSm86` true
still activated generation. Thus the JSON switch alone can activate this
driver build in this game; it does not merely supplement the profile. This
is functional evidence, not yet a controlled image-quality, latency, or
long-duration stability measurement. A separate process probe passed
DLL loading, CUDA dispatch resolution and `NVP_Init_D3D` activation on the
target machine. The DriverStore package is pinned
to this laptop, factory timing and adapter LUID are not yet proven, and a late
NvPresent initialization failure might leave DXGI hooks installed. Do not
enable it by default or distribute it as a validated port.
Once installed, the proxy DLL remains loaded for the process lifetime so the
CUDA dispatch slot cannot reference unloaded hook code.

Rewriting the FP8 kernels as FP16 would require rebuilding their matrix math,
data layouts and conversions, not merely replacing `QMMA` with `HMMA`. The
driver already provides and selects the FP16 path for Ampere.

## D3D11 and Vulkan feasibility after D3D12 validation

The inspected 617.14 binary has a native `NVP_CreateSwapchain_D3D11` export
at `+0x5990`, separate from D3D12 at `+0x59a0`. Their implementations at
`+0x9470` and `+0x9600` check different internal types and construct different
objects. `NVP_Init_D3D` accepts either backend; the default D3D12 mode forces
the D3D11 flag (`config+0x4f`) off, while the new `d3d11` mode enables only
that flag and passes the isolated initialization probe. A direct D3D11 experiment is therefore
more justified than immediately adding a D3D11-to-D3D12 shadow swapchain.
The user subsequently observed 120 FPS from a 60 FPS in-game cap in Shadows
of Doubt, with native D3D11 initialized and 19 retargeted FP16 modules
accepted by CUDA. Other games and swapchain modes remain untested.
Enable it only in an isolated experiment with the existing SHA/gate/CUDA
checks, then verify swapchain attachment, Present counts, resize, HDR,
synchronization, adapter selection and shutdown. Use a bridge only if the
native path fails and its known synchronization/HDR problems are addressed.

The same binary exports `NVP_Init_Vulkan` at `+0x5a50`, requesting API mask
`4`. The installed `nv-vk64.json` advertises `VK_LAYER_NV_present` from
`nvoglv64.dll`, and local `vulkaninfo --summary` lists the layer. The default
mode forces the Vulkan flag (`config+0x4e`) off; the new `vulkan` mode enables
it but does not call the private export or claim layer activation. Determine who invokes
the export and its three-argument contract, and whether the layer is active
with the NVIDIA profile Off, before testing SM86 retargeting in a Vulkan
process. Do not call the private export blindly. Enshrouded later generated
frames with the NVIDIA profile On, but not Off; `--keep-vulkan-layers` did not
affect this outcome. Its log shows an `nvoglv64.dll` swapchain and 19 FP16
modules accepted by CUDA.

The profile dependency is explained by the inspected `nvoglv64.dll` build
(SHA256 `68b2d0f82e69e6bb7af67ea554c11caba783e2c75f5ab475db108ff55bf1313c`):
at `+0xda33a4` it reads Smooth Motion Enable setting `0xB0D384C0`, branching
away on zero at `+0xda33b9`; an unidentified intermediate setting
`0xB09B15AF` can also gate this path at `+0xda33e3`. At `+0xda3403` it reads
Enabled APIs setting `0xB0CC0875`, requiring Vulkan bit 4 at `+0xda341a`.
Only after these checks does it resolve
and call `NVP_Init_Vulkan` at `+0xda3471` with its private layer arguments.
NvPresent reads the same settings at `+0x26dbb` and `+0x26f4f`, but our JSON
mode only overrides NvPresent's fields, not the earlier `nvoglv64.dll` gate.
An in-memory, exact-build bypass of the Vulkan-side checks appears
feasible, but is not implemented or validated. First compare their actual
values and the Vulkan initializer's return with the profile Off and On.
Forcing the Vulkan loader to discover `VK_LAYER_NV_present` alone would not
bypass this separate profile check inside the driver layer.

### Read-only Vulkan profile diagnostic

The optional `smooth_motion_drs_probe` target queries those three setting IDs
in the executable's profile and the base profile through the official NVAPI
SDK. It changes neither the profile nor the game's DLLs. Configure CMake with
`-DNVAPI_INCLUDE_DIR=<directory containing nvapi.h>`, build the target, and
pass the absolute path to `enshrouded.exe`. Capture its output once with
Smooth Motion Off and once On, with the game closed. `GetSetting status=-160`
means the setting is absent from that profile, **not** that its effective
value is zero. This probe does not establish whether `NVP_Init_Vulkan` was
called; instrumenting its return remains separate work.

Observation on September 28, 2026: after the user explicitly enabled the
Enshrouded profile, all three IDs still returned `-160` from both the
application and base profiles. A known control setting (`0x1034CB89`, FXAA)
was readable through the same API. Thus the NVAPI probe works, but these
IDs are not exposed by `NvAPI_DRS_GetSetting` in this configuration. This
does not contradict the in-game Vulkan success and does not reveal the
effective value used by `nvoglv64.dll`. The private runtime path must be
observed before deciding on a safe bypass; `-160` must not be read as Off.

An experimental observer is now built into the proxy only when
`smoothMotionSm86Api=vulkan` and the exact 617.14 `NvPresent64.dll` hash and
signatures have passed validation. It detours `NVP_Init_Vulkan` **without
altering its arguments**, calls the original, then logs entry and its boolean
return. It does not bypass the `nvoglv64.dll` gate or force Smooth Motion.
No call in the log could mean either that the gate blocked it or that the
layer was not loaded; corroborate with swapchain and FrameView observations.

Enshrouded validation on September 28, 2026 used **the same DLL** (SHA256
`CEBCD4918CA14C6AD4856C993268A52CECB54CFCC59E28BFF6949F25D8E25CFF`)
and the same JSON configuration:

- NVIDIA profile On: four observed `NVP_Init_Vulkan` calls, each returning
  `1`; an `nvoglv64.dll` swapchain; 19 FP16 kernels accepted by CUDA
  (`status=0`).
- NVIDIA profile Off: trace installed, but no `NVP_Init_Vulkan` call, no
  `nvoglv64.dll` swapchain observed by our overlay, and no Smooth Motion
  kernels loaded during that run.

This establishes a profile-dependent check **before** NvPresent's Vulkan
initialization. It does not isolate which of the three private checks
changes, as `NvAPI_DRS_GetSetting` does not expose them. Any forcing
prototype must be confined to this exact build, verify expected bytes in
`nvoglv64.dll`, and refuse other drivers. A `1` return and CUDA loads do not by themselves measure generated
frame count; use FrameView for that separate validation.

### Vulkan forcing prototype for driver 617.14

With `smoothMotionSm86=true` and `smoothMotionSm86Api=vulkan`, the proxy now
attempts, on the **first return from loading** `nvoglv64.dll`, to neutralize
only the branch that skips Smooth Motion when `0xB0D384C0` is zero. The later
checks (`0xB09B15AF` and Vulkan API authorization `0xB0CC0875`) remain
active. Only six bytes in the game's in-memory module are changed; neither
the driver file nor the NVIDIA profile is modified.

Any uncertainty means refusal: `NvPresent64.dll` must already pass its SHA
and signature checks; `nvoglv64.dll` must come from the same DriverStore
directory, match the inspected 617.14 size and SHA256, and have exactly the
expected instructions at all three checks. The proxy will not patch an
already-used module if it misses the first load. A new driver therefore
needs fresh analysis and an explicit update. `try/catch` cannot make an
incorrect machine-code branch or GPU failure safe.

The standalone probe confirmed in a test process that a foreign module is
refused, the exact module is accepted, and the target six bytes are changed.

**Enshrouded validation, NVIDIA profile Off, September 28, 2026:** the user
reports Smooth Motion working with the new proxy (SHA256
`C9B49CB35FA80042CE9CBA3E0FF57B30AE7E4B091BDEB960552DA46B4824EA01`).
The log shows the `nvoglv64.dll` patch applied, four `NVP_Init_Vulkan` calls
returning `1`, an NVIDIA-layer swapchain, and 19 FP16 kernels loaded with
`status=0`. No exception event appears in the exception log. The exact
FrameView FPS for this run was not reported; user observation and successful
initialization do not replace a quantified before/after measurement.

**No Man's Sky, Vulkan, September 28, 2026 — not recommended:** its NVIDIA
application profile defines Enabled APIs (`0xB0CC0875`) as `3`, which lacks
the Vulkan bit `4`. With that value, the exact-build patch is applied but
there is no `NVP_Init_Vulkan` call or Smooth Motion kernel load. Temporarily
changing the profile value to `4` or `7` makes Smooth Motion run, according
to the user, but moving the mouse then produces a tripled cursor. This is a
functional activation with an unacceptable visual artifact, **not** a
successful compatibility validation. Restoring `3` blocks Smooth Motion
again. The profile value is evidence of a game-specific NVIDIA gate; the
reason NVIDIA selected it and the cursor artifact's exact cause are unknown.
Do not advise overriding this game's profile for normal play. This finding
does not apply to the separate DLSS-G Vulkan path.

After restoring `3`, the user also reported that the in-game DLSS and
DLSS-G options were absent. The current log still shows their modules
loading, and the saved graphics settings show `AntiAliasing=None` and
`DLSSFrameGeneration=Off`; this does **not** establish whether the missing
options are caused by the profile experiment, game settings, or the proxy.
Test with `smoothMotionSm86=false`, then without the proxy if needed, before
attributing that UI symptom to Smooth Motion.

Keep a copy of the previous DLL. The patch must refuse a different driver;
after any driver update, retest in a game without anti-cheat and inspect the
log before using this feature.

Reference: [NVIDIA's official DRS API](https://docs.nvidia.com/nvapi/group__drsapi.html).
