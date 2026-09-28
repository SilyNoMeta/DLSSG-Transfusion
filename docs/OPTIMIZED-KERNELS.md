# Optimized kernels (bit-exact)

*Français : [OPTIMIZED-KERNELS.fr.md](OPTIMIZED-KERNELS.fr.md)*

Setting: `"optimizedKernels": true` (default; restart to apply). These optimizations come from the dlssg_for_sm86 0.3.5 backend: the original optimization work, since lost, is rebuilt from the extracted PTX. Each step was delivered on its own branch and merged into `feat/sm86-75`.

## Step 1 — image kernels (branch `feat/sm86-75-opt-image`)

The DLSS-G 310.9.1 image kernels that Transfusion runs are NVIDIA's Blackwell kernels (PTX sm_120, retargeted by Tony). That PTX is **instruction-for-instruction identical** to resources 8039, 8060 and 8063 of upstream 0.3.5, from which the optimized versions derive:

| Kernel | Optimization | Source |
|---|---|---|
| `Kernel_BlendCandidatesFused` | Ten pairs of contiguous 32-bit stores merged into `st.global.v2.u32`, applied **on top of** the valid-warp policy (same registers) | rule from 9039 |
| `Kernel_OutputPull` | Inpainting masks packed as per-row bits in shared memory, neighborhood tests on bits | embedded PTX 9060 |
| `Kernel_OutputPushFine` | Collective test of pending work, early exit for blocks with no contribution | embedded PTX 9063 |

The substitution happens in the `NvAPI_D3D12_CreateCuModule` hook, at the same place as the Turing rewrite.
- **Identification**: a kernel is recognized by its entry name and by the **fingerprint of NVIDIA's original PTX**, computed offline with `.target` excluded. Without a match, nothing is changed.
- **Blend**: it must contain the ten exact pairs.
- **Compilation chain**: the resulting PTX is retargeted to the target SM (86, 89 or 75), then rewritten for sm_75 on Turing. 9060 contains 8 sm_80 instructions, which the Turing rewrite handles. The driver then compiles it.

Off the Blackwell path (`blackwellTransfusion: false`), the fingerprints do not match and nothing is substituted.

### Validation (RTX 3070 Ti Laptop, NVIDIA runtime 310.9.1)

| Test | Result |
|---|---|
| Output with vs. without optimization: X6 1080p moving, `transfusion` policy | **20/20 identical** |
| Same, `explained-warp` policy | **20/20 identical** |
| UIAlpha 720p | **8/8 identical** |
| Emulated Turing X6 | **20/20 identical**; 3 optimized kernels in sm_75; all modules accepted |
| GPU time 1080p X2 (median, 2 passes) | 2.33 → **2.23 ms** (−0.10 ms, −4 %) |
| GPU time 1080p X6 | 6.32 → **6.10 ms** (−0.23 ms, −3.6 %) |

The gain grows with the multiplier, because OutputPull and OutputPushFine run for every generated frame. On Ada, the PTX is compiled for sm_89: this is correct, but the gain has not been measured there.

## Step 2 — DL1 decoder fusion (branch `feat/sm86-75-opt-fusion`, superseded by step 3)

### Recipe taken from the oracle

To build the fusion, I first **observed** the launch chain.

- **Tools**: two test tools that cannot be reached from the configuration. `DLSSG_TRANSFUSION_CHAIN_DUMP` records, as close to NvAPI as possible, every module, function and `LaunchCuKernelChain` entry. `DLSSG_TRANSFUSION_OBSERVE_ONLY` turns off every Transfusion patch.
- **Setup**: Transfusion as an observer, under the sdli 0.3.5 engine. GPU addresses are identical from one run to the next, so NVIDIA's chain (89 launches) and sdli's (70) can be compared parameter by parameter.

On each decoder level, NVIDIA makes three separate launches:

```
k_upscale(low → tmp) ; k_element_wise(tmp + skip → x) ; k_conv_fp16_nhwc(W, x → out)
```

They are replaced by a single one:

```
k_conv_fp16_nhwc_fused_up(W, low, skip, out, lw, lh, hw, hh)
```

- **Weights**: NVIDIA's for levels 1 to 4.
- **Grid**: ⌈hw·hh/16⌉, checked at 720p, 1080p and 1440p.
- **Blocks**: those of the kernel's `.maxntid` (256, 256, 256, 128).

### Implementation (`decoder_fusion.h`)

- **Kernels**: 4024, 4026, 4028 and 4030 in sm_86, JIT-compiled for sm_89 on Ada; 11024 to 11030 in sm_75 for Turing, without `cp.async` or `m16n8k16`.
- **Creation**: on the runtime's device, when it creates its own `k_upscale`. Recycled function handles always take their latest meaning.
- **Holding**: each launch arrives in its own chain call. The upscale and add are therefore held, with their parameters copied, until the conv that consumes them.
- **Checks**: pointer chaining (the output of one is the input of the next) and consistent dimensions.
- **Fallback**: any other launch, a command list change or a failed check replays the held launches unchanged, in order.

### Validation (RTX 3070 Ti Laptop, NVIDIA runtime 310.9.1)

| Test | Result |
|---|---|
| X6 1080p moving: fusion vs. step 1 only | **20/20 identical**; 4 fused launches per evaluation |
| Emulated Turing, X6 720p: fusion vs. step 1 only | **20/20 identical** |
| GPU time 1080p X2: none / step 1 / steps 1 and 2 | 2.33 / 2.25 / **2.11 ms** |
| GPU time 1080p X6 | 6.38 / 6.13 / **6.04 ms** |

## Step 3 — whole DL1 network (branch `feat/sm86-75-opt-network`)

Comparing sdli level 0 and level 1 in the same process, and therefore with identical allocations, shows that **the whole DL1 network runs on NVIDIA's weights**. The pointers that differed in the NVIDIA / sdli comparison were the same weights, shifted by sdli's allocations. Every convolution and pooling therefore keeps **NVIDIA's parameters unchanged**; only the kernel, grid and blocks change.

`network_optimizer.h` replaces `decoder_fusion.h`. It reproduces sdli's DL1 chain launch by launch:

| Rank | Kernel | Grid | Block |
|---|---|---|---|
| conv 0 | 4000 | ⌈w/16⌉, ⌈h/4⌉, 1 | 128 |
| conv 1 | 4008 | ⌈wh/16⌉ | 128 |
| conv 2 / 3 | 4002 / 4004 | ⌈w/16⌉, ⌈h/4⌉, 4 | 128 |
| conv 4 | 4006 | ⌈w/16⌉, ⌈h/4⌉, 8 | 256 |
| conv 5 / 6 | 4010 / 4012 | ⌈wh/32⌉, 1, 8 | 512 / 128 |
| conv 8 | 4014 | ⌈wh/32⌉, 1, 2 | 512 |
| conv 10 / 12 | 4016 / 4018 | ⌈wh/16⌉ | 256 / 128 |
| conv 14 | 4020 | ⌈wh/32⌉ | 128 |
| conv 16 | 4022 | ⌈wh/16⌉ | 64 |
| decoder L1 to L4 / L5 (upscale + add + conv) | 4024 to 4030 / 4032 | ⌈hw·hh/16⌉ / ⌈hw·hh/64⌉ | 256, 256, 256, 128 / 128 |
| pooling | 4039 | ⌈ow·oh·C/2048⌉ | 256 |

- **Formulas**: taken from the oracle and checked at 720p, 1080p and 1440p; `w×h` are the output dimensions read from the parameters.
- **Embedded kernels**: the PTX are DLL resources (`private/kernels.rc`, `private/kernels/*.ptx`; not in the public repository, see [PUBLIC-SOURCE.md](PUBLIC-SOURCE.md)). The 40xx family serves sm_86 (JIT-compiled for sm_89 on Ada), the 110xx family sm_75.
- **Identification**: each launch is recognized by its rank in the network and its parameter size; fusions also check pointer chaining and dimensions. At the first inconsistency, the rest of that network pass runs as is and held launches are replayed in order.
- **Guard**: active only if a runtime kernel matched the exact 310.9.1 fingerprint. On the default Blackwell path, that is OutputPull.

### Validation (RTX 3070 Ti Laptop)

| Test | Result |
|---|---|
| Recorded DL1 chain | **identical to sdli's** in names, grids and blocks |
| X6 1080p: optimized DL1 vs. step 1 only | **20/20 identical** |
| Emulated Turing X6 720p | **20/20 identical** (18 sm_75 kernels) |
| GPU time 1080p X2: step 1 / full DL1 / sdli level 1 | 2.22 / **1.66** / 1.60 ms |
| GPU time 1080p X6: step 1 / full DL1 / sdli level 1 | 6.13 / **5.57** / 5.20 ms |

## Step 4 — DL2 network (branch `feat/sm86-75-opt-dl2`)

### Findings

- **DL2 runs for every generated frame**: at X6 the chain contains 5 DL2 passes per real frame, against a single DL1 pass. This is why the remaining gap to sdli after step 3 grew with the multiplier (0.06 ms at X2, 0.37 ms at X6).
- **The weights are NVIDIA's.** sdli's weight pointers differ from NVIDIA's by a constant offset (for example +0x5000 across block1's whole residual chain), caused by its own allocations. The bit-exact output obtained with NVIDIA's pointers confirms it.
- **Identifying the variants.** sdli loads precompiled ELFs, not PTX. Each ELF module in the oracle was matched to its PTX resource by compiling the candidates with `ptxas -arch=sm_86`. Sizes agree up to a constant offset (+0.5 to +1.3 KB), and the entry name settles identical sizes.
- **Parameters.** The `conv_dl2_*` kernels are shape-specialized: channels and kernel size are fixed in the code, only the spatial dimensions are read. Their parameters have **exactly NVIDIA's layout** (80, 92, 144 and 152 bytes). Only the two fusions have their own structure.

### Reproduced DL2 chain

| NVIDIA layer | Replacement | Grid | Block |
|---|---|---|---|
| `k_initial_merge` + `custom_block0_convPre` | 4042 `conv_dl2_merge_pool` (fusion) | ⌈w/8⌉, ⌈h/4⌉ | 256 |
| block0 `conv0_c8` / `conv1` | 4044 / 4046 `conv_dl2_pool` | ⌈w/8⌉, ⌈h/2⌉ | 128 / 256 |
| block0 `conv2` ×8 | 4050 `conv_dl2_resid` | ⌈w/16⌉, ⌈h/2⌉, 2 | 128 |
| block0 `conv_bot0_hf` | unchanged (NVIDIA) | | |
| block0 `conv_bot1_hf` | 4062 `conv_dl2_bot1_block0` | ⌈w/16⌉, ⌈h/4⌉ | 128 |
| `custom_upsample_hf` | 4059 `upsample_hf` | ⌈wh/256⌉, 3 | 256 |
| `k_central_block` + block1 `conv0` | 4060 `conv_dl2_central_pool` (fusion) | ⌈w/8⌉, ⌈h/4⌉ | 256 |
| block1 `conv1` | 4048 `conv_dl2_pool` | ⌈w/8⌉, ⌈h/2⌉ | 128 |
| block1 `conv2` ×8 | unchanged (NVIDIA), see below | | |
| block1 `conv_bot0_hf` | unchanged (NVIDIA) | | |
| block1 `conv_bot1_hf` | 4058 `conv_dl2_bot1_block1` | ⌈w/16⌉, ⌈h/8⌉ | 256 |

- **Dimensions**: `w×h` are the output dimensions read from NVIDIA's parameters. The formulas were checked against the three recorded sdli chains, with no mismatch.
- **Fusion parameters**:
  - `merge_pool` (68 bytes): convPre weights and bias, the merge's 3 inputs, convPre's output, then input h, w, output h, w and the merge factor.
  - `central_pool` (92 bytes): conv0 weights and bias, the central block's 6 inputs, conv0's output, then input h, w, output h, w and the factor.
  - In both cases the producer's output is not written. The next layer overwrites it before any read, as with sdli.
- **Safeguards**:
  - **Identification**: each layer is recognized by its NVIDIA kernel name, then checked against its exact shape (parameter size, channels, 3×3 kernel).
  - **Fusions**: they also check chaining (producer output = conv input) and dimensions.
  - **Fallback**: on any mismatch, the held producer is replayed as is and the layer runs with NVIDIA's kernel.
- **Not taken**:
  - **`conv_dl2_reschain`** (block1's 8 `conv2` in a single launch). It is a persistent kernel with a grid barrier. It relies on an atomic counter that sdli allocates itself, and its grid is capped (136 at 1080p, 192 instead of 230 at 1280) so that every block is resident. An oversized grid would hang the GPU. sdli's own measured gain for this fusion (bit 16) is within noise.
  - Both `bot0` layers: sdli leaves them to NVIDIA as well.

### Validation (RTX 3070 Ti Laptop)

| Test | Result |
|---|---|
| X6 1080p moving, DL1+DL2 vs. step 1 only | **20/20 identical** |
| Emulated Turing X6 720p (27 sm_75 kernels) | **20/20 identical** |
| 1440p X2, vs. the same build without optimization | **4/4 identical** |
| GPU time 1080p X2: DL1 only / DL1+DL2 / sdli level 1 | 1.65 / **1.57–1.58** / 1.53–1.60 ms |
| GPU time 1080p X6: DL1 only / DL1+DL2 / sdli level 1 | 5.59 / **5.22–5.24** / 5.20–5.21 ms |

Passes are interleaved in the same bench (DL1, DL2, sdli, sdli, DL2, DL1), followed by three extra DL2 / sdli X2 passes.

## Summary (1080p, RTX 3070 Ti Laptop)

| | X2 | X6 |
|---|---|---|
| No optimization | 2.33 ms | 6.38 ms |
| Step 1, image kernels | 2.25 ms | 6.13 ms |
| Steps 1 to 3, full DL1 | 1.66 ms | 5.59 ms |
| **Steps 1 to 4, DL1 + DL2** | **1.57 ms (−33 %)** | **5.23 ms (−18 %)** |
| sdli level 1 (reference) | 1.53–1.60 ms | 5.20 ms |

**sdli level 1 is matched**, within measurement noise, with output bit-identical to NVIDIA's runtime. Everything stays under the single `optimizedKernels` setting, also active on Ada. Still to validate on an RTX 4090: output and timing, with the same bench.

## Remaining leads

- **`conv_dl2_reschain`**: only worth revisiting with a counter buffer allocated by Transfusion and a grid computed from real occupancy. The expected gain is a few hundredths of a millisecond at best.
- **Image kernels**: sdli precompiles its 26 image kernels as sm_86 ELF. Transfusion has the driver compile the PTX, which should produce the same code; no difference was measured.
