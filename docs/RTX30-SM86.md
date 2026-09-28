# Transfusion on RTX 30 (Ampere) and RTX 20 (Turing)

*Français : [RTX30-SM86.fr.md](RTX30-SM86.fr.md)*

Branches `feat/sm86` and `feat/sm86-75`, fork of [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) at commit `93ae5c1` (v1.4.5), September 2026.

**Principle: Tony's patches on NVIDIA's runtime remain the engine.** They are extended to target the architecture actually present. This port ships no sdli1995 code and no NVIDIA-derived binary; only the optional optimized kernels, added later, derive from them (see [PUBLIC-SOURCE.md](PUBLIC-SOURCE.md)). Our work on `dlssg_for_sm86` (`port3109`, `sm86_free`, reverse engineering) only served to find what had to be patched.

## Why NVIDIA's runtime refuses an RTX 30, and what Transfusion already patched

Analysis of `nvngx_dlssg.dll` 310.9.1 (SHA256 `ff6e90eb…`):

| Content | Count | Usable on SM86 as is |
|---|---:|---|
| Neural network ELF cubins, **sm_86** (shipped by NVIDIA) | 39 | **yes** |
| Network ELF cubins, sm_89 | 39 | no |
| Network fatbins, sm_89 PTX only | 39 | no (PTX too recent) |
| Image-kernel fatbins: PTX sm_120 + PTX sm_89 + ELF sm_89 | 31 | no |

All **101 PTX modules compile for sm_86** after a plain `.target` change. There is no FP8, no `wgmma` and no `cp.async.bulk`.

| Gate | Original Transfusion (Ada) | SM86 extension |
|---|---|---|
| Minimum architecture 400 in `GetGPUArchitecture` and the three `*_GetFeatureRequirements` | Absent, Ada passes it | **New**: single immediate found by export name, 0x190 → 0x170. Mechanism taken from `sm86_free`. A hook is impossible: the caller is verified. |
| Blackwell thresholds `cmp …, 0x1b0` (maximum published MFG) | `PatchDlssgArchGates` → 0x190 | Same scan, to **0x170**, or 0x160 for Turing |
| `ValidateMultiFrameCount` guard | `kNgxPatch` | Unchanged |
| Image kernels missing on SM86 | In-place retarget sm_120 → sm_89, Ada images parked at 122 | Same retarget, to **sm_86**, extended to the sm_89-only PTX containers |
| Midpoint fix and V4 quality (`BlendCandidatesFused`) | Fatbin rebuilt as `.target sm_89` | Rebuilt as `.target sm_86`, entry labeled 86 |

The `.target` directive is readable in the LZ4 stream of all 101 PTX modules. Tony's same-length rewrite (`sm_120` → `sm_86 `, `sm_89` → `sm_86`) therefore covers everything without decompression. The driver then JIT-compiles the PTX for sm_86 (through `NvAPI_D3D12_CreateCuModule`).

## Architecture selection

`gpu_arch.cpp` reads the PCI identifiers (D3DKMT) at load time; D3DKMT calls are safe under the loader lock. On Ada, Blackwell or an unknown adapter, the target stays Ada: Tony's behavior is unchanged. `DLSSG-Transfusion.json` gets a new key:

```json
"gpuArchitecture": "auto"   // "auto", "ada", "ampere" or "turing"; restart to apply
```

The `DLSSG_TRANSFUSION_GPU_ARCH` environment variable forces the target for testing and is never saved. Below Ada, the log lists every CUDA module created (`[CU-MODULE]`), with its format and the driver's answer.

## Validation — RTX 3070 Ti Laptop (SM86), driver 617.14, unmodified NVIDIA runtime

| Test | Result |
|---|---|
| `port3109` bench, 1080p, X2, 5 frames, `blackwellTransfusion=false` | **5/5 SHA256 identical to NVIDIA's reference on an RTX 4090**. At X2 the midpoint fix is neutral (t = 0.5). |
| Modules loaded by the runtime | NVIDIA's 39 sm_86 ELF and 25 PTX→sm_86 fatbins. **All accepted.** The 2 remaining rejections are the empty probing calls, as on Ada. |
| X6 1080p, default config: Tony's Blackwell kernels and V4 quality | 25 generated frames, all modules accepted |
| X6 1080p, Ada kernels and midpoint only | 25 generated frames |
| X6 cadence: measured shift of each generated frame | 8·i/6 px within ±0.3 px, no compaction toward the middle |
| Streamline 2.14.1 (game DLLs) with the driver's real NGX core | Without Transfusion: `slIsFeatureSupported` = **6** (`NoSupportedAdapterFound`). With it: **0**, `min_arch` 368. |
| Ada non-regression (`gpuArchitecture=ada` forced) | `.text` and `.rdata` of the patched runtime **byte-identical** to the `93ae5c1` build. Differences limited to `.data` (allocated pointers), as between two runs of the original build. |

### Not validated yet

- **In game**: real presentation, pacing, hotkeys, overlay, UIR.
- **Vulkan**: the kernel images go through `vkCreateCuModuleNVX`; the transport is on branch `feat/sm86-75-vulkan-nvx`, not yet validated on a GPU. See [VULKAN.md](VULKAN.md).
- **Other runtime versions** (310.1 to 310.8) and OTA copies in the NGX cache. The patches are found by signature, without RVAs, but only 310.9.1 is measured.

## Turing (SM75) — branch `feat/sm86-75`

Turing lacks three more things. Each is handled by a patch of the same kind.

**1. The network.** NVIDIA ships no sm_75 cubins. The runtime picks its network variant from the SM version NGX gives it (`Context.GPU.SMVer`, or NvAPI otherwise). Measured by varying that value:

| SM seen by the runtime | Network loaded |
|---|---|
| 8.9 | sm_89 ELF cubins |
| 9.0, 10.0, 12.0 | **PTX** modules |
| 8.6, 7.5, … | sm_86 ELF cubins |

The selector exists in two places: `call [vtbl+0x40]` / `cmp eax, 0x59` / `jle`. `PatchDlssgNetworkSelector` removes that `jle`, with a unique masked signature, on a Turing target only. Every network then goes through PTX, which the in-place retarget brings down to `sm_75`.

**2. sm_80 instructions.** Four forms stop `ptxas` for sm_75. `ptx_lowering.h` rewrites them as sm_75 PTX:

| sm_80 instruction | sm_75 rewrite |
|---|---|
| `mma.sync.m16n8k16.f16` | 2 × `m16n8k8`: the A/B fragments are the two halves of K |
| `cvt.rn.f16x2.f32 d, a, b` | 2 × `cvt.rn.f16.f32` + `mov.b32 d, {b, a}` |
| `max`/`min.f16` and `.f16x2` | through f32, exact |

Inline assembly blocks (`{ instr; }`) are preserved. Across the runtime's 101 PTX modules, 670 `mma`, 78 `cvt` and 294 `min`/`max` are rewritten, and **all 101 compile for sm_75** (`tests/ptx_lowering`).

**3. Where it applies.** Compressed PTX cannot grow in place. `cu_module_hook.h` therefore intercepts `NvAPI_D3D12_CreateCuModule`. When an image contains one of these forms, `midpoint_fix::PrepareModuleImage` decompresses it, rewrites it, rebuilds it as an uncompressed fatbin and passes it to the driver. Images are cached by content and never freed. This also covers the fatbins Tony rebuilds (Blend, Scatter).

The thresholds follow: minimum architecture 0x190 → **0x160**, arch gates 0x1b0 → 0x160.

### Validation without a Turing card

`DLSSG_TRANSFUSION_EMULATE_SM=75` is a **test mode** (`sm_emulation.h`), never read from the configuration. NvAPI then reports SM 7.5 and architecture 0x160, and the bench passes `Context.GPU.SMVer=75`. The RTX 3070 Ti runs the sm_75 PTX, so the whole Turing path really runs, without any sm_86 or sm_89 cubin:

| Test (unmodified NVIDIA runtime 310.9.1) | Result |
|---|---|
| Modules: 39 network PTX (27 rewritten) and 25 sm_75 image kernels | **all accepted** by the driver |
| X2 1080p, Ada kernels | **5/5 frames bit-identical** to NVIDIA's reference, despite the split `mma` |
| X6 1080p, default config (Blackwell + V4 quality) | 25 frames, **25/25 identical** to the Ampere path, cadence 8·i/6 px within ±0.3 px |
| Non-regression | Ampere X2: 5/5 identical to NVIDIA. Forced Ada: `.text` and `.rdata` identical to `93ae5c1`. Control harness unchanged. |

### Limits

- **Never run on a real Turing card.** The sm_75 machine code compiled by the driver has only run on SM86. Performance (without `m16n8k16`) and the VRAM of RTX 20 cards remain unknown.
- **The network selector** is only located for 310.9.x. Elsewhere the patch logs `not found`, and Turing is then not supported.
- GTX 16 (TU116/TU117, no tensor cores) is out of scope.
