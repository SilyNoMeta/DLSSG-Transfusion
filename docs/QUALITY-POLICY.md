# Valid-warp policy: `transfusion` or `explained-warp`

*Français : [QUALITY-POLICY.fr.md](QUALITY-POLICY.fr.md)*

Branch `feat/sm86-75-qualitywarp-v3`, merged into `feat/sm86-75`.

The *valid warp* was invented by Tony Joaca for Transfusion. It is a PTX patch inserted into `Kernel_BlendCandidatesFused`: when the warped candidates are reliable, the output follows the warp rather than NVIDIA's blend. `dlssg_for_sm86` wrote its own reimplementation, up to a third iteration shipped in v0.3.5-5 as "V3". That numbering is unrelated to Tony's V4.

## The idea behind `explained-warp`

Between two real frames, DLSS-G has two candidates for each generated pixel: the previous and the next frame, each warped along the motion vectors. NVIDIA blends them; the valid warp decides when to trust the warp instead.

Tony's test asks whether **the two warped candidates agree**. It is safe, but it also rejects correct motion whenever the two candidates legitimately differ: a shadow moving over lit ground, fine detail that aliases differently from one frame to the next.

`explained-warp` asks a different question: **does the warp explain what changed?** It compares the two candidates twice, before warping (`Es`) and after (`Em`). If the images differ a lot unwarped but little once warped, the motion vectors account for the change, and the full warp is taken even where the candidates do not match exactly. If warping does not reduce the difference, NVIDIA's blend is kept. A candidate NVIDIA already gives real weight to is also followed when the image changes a lot.

This is why it keeps fine detail and clean moving shadows at the same time (measurements below), where each earlier policy traded one for the other.

```json
"qualityValidWarp": true,         // master switch (Tony), unchanged
"qualityPolicy": "explained-warp" // "explained-warp" (default) or "transfusion"; restart to apply
```

| | `transfusion` (Tony, `QUALITY_VALID_WARP_V4`) | `explained-warp` (dlssg_for_sm86 V3, default) |
|---|---|---|
| Criterion | The two warped candidates **agree**: RGB difference ≤ 14 % of the maximum luminance | The warp **explains the change** between the two source frames: `Em + 0.08 < Es` and `Em < 0.15` |
| Forced case | — | Valid candidate, NVIDIA weight ≥ 0.20 and a changing image (`Es > 0.25`) |
| Effect | Warp weight raised to at least 0.98 | Full warp |
| Field feedback | Doubled shadows seen in v0.3.5-3; guard A, which fixed them (v0.3.5-4), reduced detail. Without guard A: lit patches inside moving shadows (see below) | Cyberpunk, X2 (v0.3.5-5): 310.9.1-11 level of detail and a clean shadow, with a few specks at the tips of fast limbs |

`Es` and `Em` are the L1 RGB differences between the two candidates, unwarped and warped.

## Implementation

- `quality_explained_warp.h` holds the PTX block produced by dlssg_for_sm86's `tools/companion/quality_program.py` (`program_v3('blackwell')`), byte for byte.
- It is inserted at the same anchor as Tony's policy, before `ld.param.u8 %rs8, [%rd6+220]`, and replaces it: the two are never combined.
- The PTX fingerprint guard and the exact SFU replacements of `quality_fix::Patch` apply to both policies.
- The Blackwell Blend PTX patched by Transfusion is **instruction-for-instruction identical** to upstream 0.3.5's 8039, on which V3 was designed: same register layout.

## Validation (RTX 3070 Ti Laptop, unmodified NVIDIA runtime 310.9.1)

| Test | Result |
|---|---|
| `ptxas` compilation, both policies, for sm_86, sm_89 and sm_75 (after the Turing rewrite) — `tests/quality_policy` | 6/6 |
| `explained-warp` against the dlssg_for_sm86 v0.3.5-5 release (V3 as cubin), 720p moving: X6, UIAlpha, HUD-less only | **20/20, 4/4, 4/4 bit-identical frames** (HUD-less only compared with the full release, which also applies the UIR patches) |
| `transfusion` against the same release | Different (0.4 to 0.7 % of pixels): the two policies are indeed distinct |
| `explained-warp` on emulated Turing (`DLSSG_TRANSFUSION_EMULATE_SM=75`) | 20/20 identical to the Ampere path |

## In-game comparison (September 26, 2026)

Cyberpunk 2077, RTX 3070 Ti Laptop, X2, SDR, recorded with the NVIDIA App (one image per present). Same save and same route as the dlssg_for_sm86 takes: fence, run along the fence, road, then a take dedicated to the character's shadow. The builds were analysed **blind**: the policy behind each take was only revealed after the results.

Generated-frame detail: median sharpness (Laplacian) of a generated frame divided by that of its two real neighbours, on moving frames, per phase (higher = more detail kept). Two takes per build.

| Build | Fence | Road |
|---|---|---|
| NVIDIA only (dlssg_for_sm86, `Optimized=0`) | 0.810 | 0.808 – 0.813 |
| dlssg_for_sm86 v0.3.5-4 (Tony's V4 + guard A) | 0.814 – 0.817 | 0.822 – 0.829 |
| **`transfusion`** (Tony's V4, no guard A) | 0.815 – 0.819 | 0.830 – 0.834 |
| **`explained-warp`** | 0.826 – 0.827 | 0.835 – 0.836 |
| dlssg_for_sm86 v0.3.5-5 (V3) | 0.827 – 0.832 | 0.833 – 0.838 |

Lit patches inside moving shadows: pixels clearly brighter in the generated frame than anywhere in a 9×9 px neighbourhood (half resolution) of both real neighbours, where those are dark. These pixels are grouped into blocks, and the number of blocks per generated frame is divided by the same measure on real frames (lower = cleaner shadow).

| Build | Generated / real ratio |
|---|---|
| NVIDIA only | 0.26 |
| **`explained-warp`** | 0.39 |
| dlssg_for_sm86 v0.3.5-5 (V3) | 0.64 |
| dlssg_for_sm86 v0.3.5-4 (V4 + guard A) | 0.88 |
| **`transfusion`** | **2.1** |

Findings:
- **`transfusion` without guard A fills moving shadows with hard-edged shreds of lit ground** (leg, head), worse than the 310.9.1-11 patches. Removing guard A does not bring the detail back either: the fence stays at the v0.3.5-4 level.
- **`explained-warp` matches v0.3.5-5** on the fence and the road, as expected since its images are bit-identical offline. On the single shadow take it gives 0.39 against 0.64, and a detail of 0.887 against 0.928, for the same algorithm. These gaps show the take-to-take variability: on this single take, a gap below about 1.6× is not significant. The gap for `transfusion` (3× to 5×) is.
- **HUD**: the same share of yellow is kept on moving quest markers for every build with UIR (~96 %, against 83 % for 310.9.1-11 without UIR). Animated markers (pulsing ring, check mark) tear the same way with both policies.
- **City (cars, pedestrians)**: nothing notable with either policy; the two takes did not film the same scene.

**Consequence: `explained-warp` becomes the default policy.** `transfusion` remains available.
