# What the public source leaves out

*Français : [PUBLIC-SOURCE.fr.md](PUBLIC-SOURCE.fr.md)*

The public repository contains all of this fork's code: architecture patches, Turing rewrite, Vulkan transport, controls, overlay, ReShade panel, UI assist, tests and documentation. A few **GPU kernels** are left out, because we do not hold the rights to publish them. The engine builds and runs without them; only part of `"optimizedKernels"` is affected.

## What is missing

| Missing | What it is | Why it is not published |
|---|---|---|
| `source/native/private/kernels/*.ptx` and `private/kernels.rc` | 54 PTX kernels for the DL1 and DL2 networks (40xx for sm_86, 110xx for sm_75) | Taken from the dlssg_for_sm86 0.3.5 backend, which does not publish them in source form, and derived from NVIDIA's DLSS-G network kernels |
| `source/native/private/image_kernels_ptx.h` | PTX of the exact `Kernel_OutputPull` and `Kernel_OutputPushFine` replacements | Derived from NVIDIA's DLSS-G 310.9.1 kernels through the same backend |

The code that uses them is public: `network_optimizer.h` (launch rules and fusions), `image_kernels.h` (identification by fingerprint, Blend vectorization) and the hooks that substitute them.

## What a public build does without them

| Feature | Public build |
|---|---|
| X2–X6, Dynamic and adaptive modes, overlay, panel, hotkeys | Unchanged |
| RTX 30 / RTX 20 support, Blackwell Transfusion | Unchanged (patches of NVIDIA's own runtime, applied in memory) |
| Valid-warp protection, both policies (`explained-warp` and `transfusion`) | Unchanged |
| Vulkan transport (`VK_NVX_binary_import`) | Unchanged |
| UI assist | Unchanged |
| `optimizedKernels`: `BlendCandidatesFused` vector stores | Active (a text rewrite of NVIDIA's kernel, no payload) |
| `optimizedKernels`: `OutputPull`, `OutputPushFine`, DL1/DL2 network and launch fusions | **Inactive**: NVIDIA's kernels run unchanged. Same image, slower frame generation (on an RTX 3070 Ti Laptop at 1080p, about 2.3 ms instead of 1.6 ms at 2x) |

CMake reports it at configure time (`private/ not present`), and the log says it once:

```text
Network optimization unavailable: kernel 4000 is not embedded in this build (docs/PUBLIC-SOURCE.md)
```

The released binaries are built with all of them.

## Also left out

- **Development history.** The private history contains the kernels above, so it is not published: the public repository keeps [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion)'s history (MIT) and adds one commit per release.
- **Prebuilt binaries.** The repository contains none (upstream kept an old `DLSSG-Transfusion.asi`); binaries are published as releases.

## Third-party material that stays

- **Microsoft Detours** (`source/native/detours`, MIT).
- **Small fragments of NVIDIA's PTX** used as search patterns by the in-memory patches (register names, instruction sequences to find and rewrite), and the fingerprints used to recognize NVIDIA's kernels. These identify NVIDIA's code; they do not reproduce it. The patches inherited from Tony's upstream work the same way.
- **The DLSS-G game list** (`nvidia_mfg_manifest.generated.h`): game names and tiers from NVIDIA's public manifest, as in upstream.

Streamline, the NGX SDK and ReShade are not included either; the build takes their paths (`STREAMLINE_ROOT`, `RESHADE_ROOT`, see the README's *Build* section).
