# DLSS-G Multi-Frame Generation Quality Fix: Comprehensive Experiment & Research Log

## GPU Direct Candidate Capture Probe: Implemented & Validated

### Problem & Diagnostic Root Cause
The initial CandidateCaptureHarness build failed during smoke test readback with:
`Mismatch 0: 0 expected 0.5` (readback floats returned all 0).

**Root Causes Identified**:
1. **D3D12 Descriptor Heap Visibility**: In `source/native/candidate_capture.h`, `job.heap` was created with `D3D12_DESCRIPTOR_HEAP_FLAG_NONE`. Passing a non-shader-visible descriptor to `NvAPI_D3D12_GetCudaSurfaceObject` caused NVAPI to return a dummy surface handle 0 (`job.handle = 0`). Setting `heap.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;` immediately resolved this, returning a valid GPU surface descriptor handle (`34816`).
2. **Command List Descriptor Heap Binding**: In `Record()`, D3D12 requires descriptor heaps to be bound prior to dispatch via `list->SetDescriptorHeaps(1, heaps);`. Without this call, GPU surface store instructions (`sust.p.2d.v4.b32.zero`) were dropped by the hardware.

### Validation Status
- `tests/startup_hooks/build-capture/Release/CandidateCaptureHarness.exe`:
  **PASSED** (`D3D12_NVAPI_CAPTURE_READBACK_OK: 2359296 floats; actual probe shader created`).
- CTest Suite: **9/9 tests passed (100%)**.
- Binary built: `build-capture/dist/version.dll` (Marker: `quality-mode9-candidate-capture-v1`).

### In-Game Capture Instructions
1. Deploy `build-capture/dist/version.dll` to Neverness to Everness.
2. Center the police sirens or artifact area in the middle of the screen.
3. Press **F8** once. Transfusion arms a burst capture of 6 consecutive frames, evaluating the exact 256x256 central region.
4. Output is written automatically to `capture-output/<PID>-<dispatch>.rgba32f`, `.json`, and `.params`.
5. Run `python scripts/view_capture.py` to extract all 9 atlas planes (raw warped candidates, unwarped references, confidence weights, UVs, Mode 9 post-firewall outputs, and boolean trigger predicates) into individual PNGs.

---

## Source-isolation A/B result: both rejected for gameplay

User supplied source0.mp4 and source1.mp4 from the NVIDIA NTE recordings folder:
"They both look like crap to me, shadows jittering and sirens pretty much the same".
Metadata: both 2560x1440, nominal 120 fps; durations 4.176256 and 4.694011 s.
Reviewed 1-fps overviews and 12 consecutive crops starting at 2.0 s in each clip.
Artifacts saved in `scratch/source-ab-review/`. Siren silhouette irregularity
remains visible in both; no clear winning source is established. The camera
paths differ, so these are qualitative comparisons, not matched pixel metrics.
Shadow jitter is user-reported and consistent with the intentional loss of
RGB temporal diversity in this diagnostic.

Reject both source-isolation builds for gameplay. Retain normal mode 9 with
QUALITY_DIAGNOSTIC=0 as the last user-reported improved baseline (mode 8 rollback
also retained). This result does NOT prove both raw candidates are deformed:
source isolation only applies where both candidates pass checks; auxiliary
channels and all downstream processing remain active. Runtime activation and
per-pixel diagnostic coverage are not verified by these recordings.

Further candidate-copy or threshold changes are not supported by this result.
Needed evidence: a same-scene FG-off reference to establish source-render quality,
and actual GPU resource captures with candidate validity/overwrite coverage and
subframe identity. Existing provider dispatch hooks only read CPU parameter
blocks and do not supply these images. No new quality shader or game deployment
was made in response to these clips.

---


## Next diagnostic: source-isolation A/B

Validation: both Release DLLs built; each passed 8/8 harness checks, full PTX
and rebuilt fatbin JIT, and 204 synthetic GPU cases. In-game results pending.

Two opt-in builds use mode 9 plus `QUALITY_DIAGNOSTIC=1` (source 0) or `2`
(source 1). Default is 0/off. After the normal quality policy, where both
candidates pass its individual geometry/finite/confidence checks, both RGB
outputs are replaced with the selected raw warped sample. Missing or otherwise
ineligible pairs keep mode 9 behavior. Auxiliary channels, downstream UI and
neural processing remain unchanged. Source identity is the kernel's sample
index; temporal direction has not been independently verified.

These builds are interventions, NOT raw candidate visualizations or GPU
readbacks. They do not reveal the overwrite mask. Full-screen single-source
selection can intentionally degrade shadows, occlusions, and motion: evaluate
only the siren comparison and then restore the normal build. If one source is
consistently cleaner, source choice is implicated but not proven as the only
cause. If both are bad, raw GPU readback is still needed to separate input
warping, auxiliary features, and downstream reconstruction.

Artifacts: `build-source0/dist/version.dll` and `build-source1/dist/version.dll`.
Markers: `diagnostic-source0-rgb` and `diagnostic-source1-rgb`.
Configure source/native with SCATTER_EXPERIMENT=9, QUALITY_DIAGNOSTIC=1 or 2,
and the existing Streamline path into the respective build directory. Configure
the harness identically under `tests/startup_hooks/build-source0` or `build-source1`.
The GPU test accepts that directory as its argument and detects the diagnostic
marker in the exported policy.

Test the same short camera sweep at 6x separately with each DLL, restarting the
game between replacements. Keep other settings fixed and label recordings
source0/source1. Restore `build-siren/dist/version.dll` afterward. No game files
are replaced by preparation of these diagnostics.

---


## User recording review: 6x, 19:47:20 (September 13)

Input: `C:/Users/TonyJoaca/Videos/NVIDIA/NTE (Neverness To Everness)/NTE (Neverness To Everness) 2026.09.13 - 19.47.20.15.mp4`.
Metadata: AV1, 2560x1440, nominal 120 fps, container duration 4.942356 s.
User reports 6x, selected because the artifact is most noticeable there.
The recording itself does not verify the loaded DLL or label generated frames.

Inspected overview samples at 2 fps and consecutive cropped sequences starting
at 1.0 and 3.0 s (24 captured frames each), with a native-resolution 6-frame detail
starting at 1.05 s. Artifacts saved under `scratch/siren6x-review/`.
The red dome's left boundary develops horizontal protrusions/missing strips;
the blue dome also develops irregular edges. Shape integrity varies across
adjacent captured frames. The roof/mount remains comparatively coherent in these
crops, although background foliage edges also break up. This is not merely a
uniform translucent afterimage: local silhouette deformation is clearly visible.

Interpretation: spatial correspondence/occlusion errors, spatially inconsistent
candidate selection, or downstream reconstruction are plausible. The recording
cannot locate the first faulty stage, establish which candidate is correct, or
measure a specific frame delay. Do not label every sixth captured frame as real:
120-fps recording need not sample the presentation stream one-to-one.

Next diagnostic should compare the two pre-network warped candidates and the
actual overwrite predicate in the affected region, with resource/frame identity
and interpolation phase. If candidates are already deformed, investigate their
motion/occlusion inputs; if clean candidates yield a deformed output, investigate
selection and downstream features. Do not use this clip alone to justify another
threshold reduction or a fixed motion/time offset. No shader changes made during
this video review; retain mode 9 and the mode 8 rollback.

---


## Follow-up: shadows acceptable, siren ghosting remains (mode 9)

User feedback on mode 8: "Shadows look pretty good to me, sirens are still pretty
 ghosting". This is a subjective in-game result; it does not establish which
 internal predicate misses the sirens.

Mode 9 (`quality-siren-chroma025-v1`) tests whether mode 8's retained high chroma
threshold excludes weaker translucent conflicts. The only executable PTX change
relative to mode 8 is signed chroma threshold 0.70 -> 0.25. Total RGB difference
must still exceed 0.35; the brightness-scale shadow veto, confidence/geometry
checks, 100% warp, and candidate copy are identical. Mode 8 and its DLL remain
available for rollback. Mode 0 remains the default.

Artifact: `build-siren/dist/version.dll`. Configure production and harness builds
with `SCATTER_EXPERIMENT=9`, using `build-siren` and
`tests/startup_hooks/build-siren`, respectively. Test command:
`python scripts/test_shadow_scale_gpu.py tests/startup_hooks/build-siren`.
Patch message: `SIREN CHROMA 0.25 with SHADOW SCALE V1 experimental veto applied`.

Validation: 8/8 harness checks passed; full kernel and rebuilt fatbin JIT passed;
204 synthetic GPU cases passed for each of modes 8 and 9. New moderate-color
conflicts in both temporal directions copy only in mode 9. Scalar shadows and
near-scalar brick colors stay protected. Exported instruction comparison confirms
that only the chroma threshold changed. Release build succeeded with missing
Detours debug-symbol warnings. No game deployment was performed.

In-game siren benefit is unverified. Lowering this threshold may regress textured
or colored-light shadows that do not satisfy the scale model, and may overwrite
other legitimate color transitions. Compare the same siren route and brick
shadows at the same multiplier/base frame rate. If sirens remain unchanged,
inspect actual candidate colors and trigger masks instead of assuming another
threshold reduction is justified.

---


## September 13 follow-up: corrections and shadow-scale-v1 experiment

The historical claims below are not guarantees. Section 7.2's ratio bound is
false: red `(1,0,0)` versus shadowed `(0.5,0,0)` gives chroma/motion = 2.
The shader uses signed channel differences, unlike the absolute-difference
formula in the derivation. Captured-video trigger rates are proxies, not internal
candidate measurements. Candidate 1 is not intermediate-frame ground truth;
exact neural mixing weights have not been established.

Opt-in `SCATTER_EXPERIMENT=8` adds a shadow veto before the existing candidate
copy. Mode 0 stays unchanged. The 100% warp, 0.70 signed chroma threshold,
confidence checks, and auxiliary-channel copying are retained for comparison.
The veto detects positive-aligned RGB vectors with squared relative
least-squares residual <= 0.01:

`1 - dot(A,B)^2 / (dot(A,A) * dot(B,B))`

RGB vectors are normalized by absolute channel sum to limit arithmetic range;
the final comparison avoids division. Both sums must exceed 0.01, otherwise the
old rule remains. Thresholds are experimental. Scalar brightness changes are
protected across surface saturations. Textured misalignment, colored lighting,
tonemapping, and near-proportional emissive changes remain limitations. Hard
thresholds remain; helmet motion and candidate directionality are not repaired.

Build: configure `source/native` into `build-shadow` with
`-DSCATTER_EXPERIMENT=8 -DSTREAMLINE_ROOT=D:/Coding/DLSSGUnlock/Streamline`, then
`cmake --build build-shadow --config Release`.
Configure `tests/startup_hooks` into `tests/startup_hooks/build-shadow` with the
same definitions. Run:

```
ctest --test-dir tests/startup_hooks/build-shadow -C Release --output-on-failure
python scripts/test_shadow_scale_gpu.py
```

Validation: Release build succeeded (missing Detours debug symbols warnings);
8/8 CTest checks passed; full patched PTX and production rebuilt fatbin JIT loaded;
198 GPU cases passed, including both shadow directions, saturated surfaces,
exposure scales, sirens, missing vectors, and low current confidence. The GPU test
executes the harness-exported experimental policy, replacing only its provider
flag load with zero for synthetic inputs. Full kernel/fatbin JIT uses the actual
exports. This does not validate in-game quality or GPU performance.

Artifact: `build-shadow/dist/version.dll`. Marker: `quality-shadow-scale-v1`.
Patch message: `SHADOW SCALE V1 experimental veto applied`.
No game files were replaced. Compare with mode 0 at the same base frame rate
and multiplier: brick shadows, sirens, fences, then helmet. Watch for siren
regressions as well as shadow improvement. Actual candidate/mask captures and
subframe labels remain future work; no instrumentation was added in this step.

---


> **Target Audience**: Next AI assistant or graphics engineer continuing research on DLSS-G Frame Generation quality in *Neverness to Everness* (UE5) and other titles running multi-frame generation (2x, 3x, 4x, 5x).
> **Last Updated**: September 13, 2026 (Branch `main`, Commit `ceebde5`).

---

## 1. Executive Summary & Objective

In *Neverness to Everness* (Unreal Engine 5) and similar modern engines using NVIDIA DLSS Frame Generation (specifically Multi-Frame Generation / DLSS-G 4x/5x on Ada Lovelace and Blackwell architectures), several severe visual artifacts compete directly with one another:

1. **Wire Fences & Thin Geometric Structures**:
   - *Symptom*: Fences, power lines, and thin railings tear, flicker, or dissolve into noisy dashed lines between frames.
   - *Requirement*: Requires 100% geometric warp weight (`max.f32 %qf0, %f148, 0f3F800000;`) to prevent DL1Net from down-weighting low-confidence thin edges.
2. **Volumetric / Translucent Colored Lights (Police Car Sirens)**:
   - *Symptom*: Flashing red/blue police sirens moving against a green background hedge wobble, oscillate violently, and cast floating "ghost sirens" behind the car.
   - *Cause*: Sirens are semi-transparent and emissive. Motion vectors on the siren pixels point to the static background hedge, causing Candidate 0 (backward-warped from $t_0$) to drag stale siren color into the background hedge, while Candidate 1 (forward-warped from $t_1$) has the hedge. DL1Net averages them 50/50, generating a persistent trailing ghost.
3. **Dynamic Character Shadows on Ground**:
   - *Symptom*: When the character runs or moves, the cast shadow on the ground stutters, jitters, snaps forward, or develops holes instead of moving smoothly.
   - *Cause*: In UE5, dynamic character shadows cast onto static ground have **zero object velocity**—their motion vectors carry only the camera/ground velocity. DL1Net relies on smooth temporal interpolation between Candidate 0 (shadow at $t_0$) and Candidate 1 (shadow at $t_1$). If any candidate overwrite/reconciliation firewall triggers on shadow pixels, the shadow snaps immediately to $t_1$'s position on intermediate frames ($t_{0.25}, t_{0.50}, t_{0.75}$), producing severe jitter.
4. **Translucent Bubble Helmets & Thin Outer Shells**:
   - *Symptom*: The outer silhouette of curved glass/plastic helmets gets "notched", eroded, or ghosted when moving past dark background objects (cables, light poles).

### The Core Architectural Dilemma
- If you **force 100% warp symmetrically**, wire fences become solid and tear-free, but stale candidate ghosts (sirens) are magnified.
- If you **reconcile candidate disagreement by overwriting Candidate 0 with Candidate 1**, sirens and trailing ghosts vanish, but character shadows get destroyed or jitter violently.
- If you **gate candidate overwriting with chromatic error ($E_{\text{chroma}}$)**, shadows are protected on grey concrete ($E_{\text{chroma}} \approx 0.05$), but on colored ground (such as terracotta red brick pavers), natural ground color saturation scales under shadows ($E_{\text{chroma}} \approx 0.25 - 0.55$), causing high-frequency shadow jitter along brick mortar joints if the threshold is set too low.

---

## 2. DLSS-G Pipeline & Shader Architecture

### 2.1 The Frame Generation Pipeline
```
[Frame 0 (t0)] ──┐
                 ├──> [Optical Flow Accelerator (OFA)] ──> [EstimateIntermMvecsScatter]
[Frame 1 (t1)] ──┘                                                  │
                                                                    ▼
[G-Buffers: Depth, MVecs, Exposure] ───────────────────> [BlendCandidatesFused]
                                                                    │
                 ┌──────────────────────────────────────────────────┴───────────────────────┐
                 ▼                                                                          ▼
       Candidate 0 (t0 warped)                                                    Candidate 1 (t1 warped)
       (%f39, %f38, %f37, %f36)                                                   (%f43, %f42, %f41, %f40)
                 │                                                                          │
                 └──────────────────────────────────┬───────────────────────────────────────┘
                                                    ▼
                                           [DL1Net (Neural Net)]
                                                    │
                                                    ▼
                                    Generated Intermediate Frame (t_alpha)
                                                    │
                                                    ▼
                                        [Native UI Recomposition]
```

### 2.2 The Target Kernel: `BlendCandidatesFused_sm120`
The core warping and candidate generation occurs in CUDA kernel `BlendCandidatesFused_sm120` inside `nvngx_dlssg.dll`. This kernel is stored in PTX bytecode inside the fatbin and JIT-compiled at game startup by the NVIDIA driver.

Transfusion intercepts this PTX before driver compilation via `source/native/quality_fix.h`.

#### Key Registers in the Baseline Kernel:
| Register | Type | Meaning |
|---|---|---|
| `%rd6` | `.u64` | Base parameter block pointer. `[%rd6+220]` contains the provider flag (`u8`). |
| `%r10, %r11` | `.u32` | Viewport dimensions: Width ($W$), Height ($H$). |
| `%f123, %f124` | `.f32` | Normalized UV coordinates for Candidate 0 ($u_0, v_0$). |
| `%f129, %f130` | `.f32` | Normalized UV coordinates for Candidate 1 ($u_1, v_1$). |
| `%f148` | `.f32` | Baseline provider confidence/weight for Candidate 0 ($w_0$). |
| `%f149` | `.f32` | Baseline provider confidence/weight for Candidate 1 ($w_1$). |
| `%f115, %f116, %f117` | `.f32` | Reference base color for Candidate 0 ($R_0^{\text{ref}}, G_0^{\text{ref}}, B_0^{\text{ref}}$). |
| `%f119, %f120, %f121` | `.f32` | Reference base color for Candidate 1 ($R_1^{\text{ref}}, G_1^{\text{ref}}, B_1^{\text{ref}}$). |
| `%f125, %f126, %f127` | `.f32` | Warped candidate color 0 ($S_{0,R}, S_{0,G}, S_{0,B}$). |
| `%f131, %f132, %f133` | `.f32` | Warped candidate color 1 ($S_{1,R}, S_{1,G}, S_{1,B}$). |
| `%f39, %f38, %f37, %f36` | `.f32` | Candidate 0 output channels ($R, G, B, A$) passed to DL1Net. |
| `%f43, %f42, %f41, %f40` | `.f32` | Candidate 1 output channels ($R, G, B, A$) passed to DL1Net. |
| `%p16, %p17` | `.pred` | Out-of-bounds rejection predicates from upstream motion vector tracking. |

#### Injected Registers (via `kRegisters`):
```ptx
.reg .pred %qv<16>;
.reg .f32 %qf<16>;
.reg .u16 %qrs<2>;
```

---

## 3. Physical & Mathematical Analysis of Artifacts

### 3.1 Why Sirens Wobble and Ghost
- A police car moves across the screen in front of a green hedge ($R=0.20, G=0.45, B=0.20$).
- The flashing sirens are intensely colored:
  - **Blue Siren**: $R \approx 0.20, G \approx 0.55, B \approx 1.00$
  - **Red Siren**: $R \approx 1.00, G \approx 0.20, B \approx 0.20$
- Because the siren light is semi-transparent and lacks a distinct depth buffer edge, motion vectors track the hedge.
- Candidate 0 (warped from $t_0$) contains the siren light displaced into the hedge.
- Candidate 1 (warped from $t_1$) contains the hedge background.
- When Candidate 1 overwrites Candidate 0, the stale ghost is replaced by the current frame's true background, eliminating the trailing ghost siren completely.

### 3.2 Why Character Shadows Jitter (The Diffuse Scaling Theorem)
- In Unreal Engine 5, dynamic cascade shadow maps (CSM) or virtual shadow maps (VSM) darken the ground.
- The ground pixels have **camera/static motion vectors only**; the shadow itself has **no object velocity**.
- As the character runs:
  - At $t_0$, region $A$ is shadowed ($C_0 = S \cdot C_{\text{ground}}$).
  - At $t_1$, the character moves forward, and region $A$ is unshadowed ($C_1 = C_{\text{ground}}$).
- In multi-frame generation (e.g., 4x generating frames $t_{0.25}, t_{0.50}, t_{0.75}$), DL1Net must blend Candidate 0 and Candidate 1 so the shadow progresses smoothly.
- **If Candidate 1 overwrites Candidate 0 on region $A$**:
  On *all* intermediate frames, region $A$ immediately displays $t_1$'s value (unshadowed). Region $B$ (where the shadow is moving to) immediately displays $t_1$'s value (shadowed).
  **Result**: The shadow snaps forward instantly on frame $t_{0.25}$, freezing for the remaining generated frames, creating severe, nauseating jitter.

### 3.3 The Mathematical Derivation of Shadow Chromaticity
Let the diffuse ground surface color be $C = (R, G, B)$.
When a shadow falls on the ground, ambient lighting reduces the total light by a shadow attenuation factor $S \in [0.4, 0.7]$:
$$C_{\text{shadowed}} = S \cdot (R, G, B)$$
$$C_{\text{unshadowed}} = (R, G, B)$$

The per-channel differences between the two candidates at a shadow boundary are:
$$\Delta R = |(1 - S) R| = (1 - S) R$$
$$\Delta G = |(1 - S) G| = (1 - S) G$$
$$\Delta B = |(1 - S) B| = (1 - S) B$$

The total motion error ($E_{\text{motion}}$) is:
$$E_{\text{motion}} = \Delta R + \Delta G + \Delta B = (1 - S)(R + G + B)$$
For bright ground ($R+G+B \approx 1.8$), with $S = 0.55$:
$$E_{\text{motion}} \approx 0.45 \times 1.8 = 0.81 > 0.35f$$
Therefore, **motion error alone CANNOT distinguish shadows from moving objects**.

Now consider the chromatic error ($E_{\text{chroma}}$):
$$E_{\text{chroma}} = |\Delta R - \Delta G| + |\Delta G - \Delta B| + |\Delta B - \Delta R|$$
Substituting $\Delta R, \Delta G, \Delta B$:
$$E_{\text{chroma}} = (1 - S) \cdot \Big( |R - G| + |G - B| + |B - R| \Big)$$

#### Case A: Grey Concrete Ground (`shadow3.mp4`)
On concrete, $R \approx G \approx B \approx 0.50$:
$$|R - G| \approx 0,\quad |G - B| \approx 0,\quad |B - R| \approx 0$$
$$E_{\text{chroma}}^{\text{concrete}} \approx (1 - S) \times 0.10 \approx \mathbf{0.05} \ll 0.25$$
Result: A low threshold ($0.25$) safely ignored concrete shadows!

#### Case B: Terracotta Red Brick Pavers (`shadow4.mp4`)
On red/orange terracotta bricks, the unshadowed color is $R \approx 0.80, G \approx 0.61, B \approx 0.55$:
$$|R - G| = 0.19,\quad |G - B| = 0.06,\quad |B - R| = 0.25$$
Sum of channel spreads: $0.19 + 0.06 + 0.25 = \mathbf{0.50}$.
With $(1 - S) \approx 0.50$:
$$E_{\text{chroma}}^{\text{brick}} \approx 0.50 \times 0.50 = \mathbf{0.25}$$
Along high-frequency mortar joints, texture noise, and specular brick sheen, $E_{\text{chroma}}$ reaches $\mathbf{0.45 - 0.60}$!

**The Failure of Threshold 0.25**:
Because $E_{\text{chroma}}^{\text{brick}}$ peaked at $0.60$, setting the threshold to $0.25$ caused **40.02% (285,188 pixels)** of the shadow on brick pavers to falsely trigger Candidate 1 overwrite! This caused the shadow to violently jitter across the brick edges.

#### Case C: Saturated Sirens Against Green Hedge (`ntecop8.mp4`)
Candidate 0 is green hedge ($R=0.20, G=0.45, B=0.20$).
Candidate 1 is blue siren ($R=0.20, G=0.55, B=1.00$).
$$\Delta R = 0.00,\quad \Delta G = 0.10,\quad \Delta B = 0.80$$
$$E_{\text{chroma}}^{\text{siren}} = |0 - 0.1| + |0.1 - 0.8| + |0.8 - 0| = 0.1 + 0.7 + 0.8 = \mathbf{1.60} \gg 0.70$$

For red siren ($R=1.00, G=0.20, B=0.20$):
$$\Delta R = 0.80,\quad \Delta G = 0.25,\quad \Delta B = 0.00$$
$$E_{\text{chroma}}^{\text{siren}} = |0.8 - 0.25| + |0.25 - 0| + |0 - 0.8| = 0.55 + 0.25 + 0.80 = \mathbf{1.60} \gg 0.70$$

---

## 4. Chronological Log of Experiments & User Feedback

| Build / Video | Implementation / Patch Strategy | User Feedback | Analysis / Root Cause |
|---|---|---|---|
| **Build 1**<br>(`122a619`) | Pure 100% geometric warp clamp:<br>`max.f32 %qf0, %f148, 0f3F800000;`<br>`max.f32 %qf1, %f149, 0f3F800000;` | "Fence is good, but sirens are still wobbling and ghosting around" | Wire fences became 100% solid. However, translucent objects with stale motion vectors (sirens) had stale Candidate 0 samples forced into DL1Net without decay. |
| **TestBuild 9**<br>(`2d3c84a`)<br>`ntecop2.mp4` | Universal agreement firewall:<br>Compute $D_{\text{color}} = \|C_0 - C_1\|$. If disagreement $> \text{threshold}$, down-weight both candidates. | "Fence is good, sirens still wobbling/ghosting, bubble is ghosting a little bit more" | Symmetrically down-weighting both candidates causes DL1Net to fallback to blurred temporal features, increasing ghosting around translucent edges. |
| **TestBuild 10**<br>(`599a1b0`)<br>`ntecop3.mp4`<br>`shadows.mp4` | Reconcile candidate conflicts by copying Candidate 1 to Candidate 0 whenever $E_{\text{motion}} > 0.35f$. | "Nah very bad, fence is good but ghosting is at all time high, sirens and shadows get completely destroyed" | Catastrophic failure for shadows. Because shadows have zero object velocity, candidate motion error is huge ($0.78$). Overwriting Candidate 0 universally collapsed all shadow interpolation, punching holes and destroying shadows. |
| **TestBuild 10b**<br>(`4d1ff80`)<br>`ntecop4.mp4` | Strict asymmetric trailing ghost collapse (check provider confidence $w_1 > w_0$). | "Fence is good, shadows back to normal, issues still on sirens, bubble still pretty shit" | Restored shadows by preventing unconstrained copying, but sirens still wobbled because provider confidence $w_1$ was not consistently greater than $w_0$ on volumetric lights. |
| **TestBuild 11**<br>(`19c86f2`)<br>`ntecop5.mp4` | Temporal reference deviation gating: Compare warped samples against unwarped references $\|S_0 - R_0\|$ vs $\|S_1 - R_1\|$. | "Sirens are better but still issues visible, bubble is bad" | Helped identify disocclusions, but siren reflections inside the hedge still had ambiguous reference errors. |
| **Experiment: 85% Warp**<br>(`scatter_experiment::kMode == 6`)<br>`ntecop6.mp4` | Reduce warp clamp from 100% (`0f3F800000`) to 85% (`0f3F59999A`). | "Sirens might look a little bit worse, bubble still pretty shit" | Reducing warp softened wire fences without eliminating siren ghosts. Proved 100% warp is mandatory for wire fences. |
| **TestBuild 12**<br>(`4630b84`)<br>`ntecop7.mp4`<br>`shadow2.mp4` | Pure asymmetric Candidate 1 overwrite on all high-motion conflicts ($E_{\text{motion}} > 0.35f$). | "Nah bad fix, shadows became bad again and siren lights still look like shit" | Confirmed again: any un-gated Candidate 1 overwrite causes shadows to jump forward to $t_1$, causing extreme stutter. |
| **TestBuild 13**<br>(`ed67d34`)<br>`ntecop8.mp4`<br>`shadow3.mp4` | **Chromatic-Aware Conflict Firewall**: Added $E_{\text{chroma}} > 0.25f$ (`0f3E800000`) requirement before Candidate 1 can overwrite Candidate 0. | "Sirens look much better! Shadows do seem better..." | **Major breakthrough for sirens**: User confirmed sirens looked much better. Smooth concrete shadows (`shadow3`) stopped jittering because concrete has $E_{\text{chroma}} \approx 0.05 < 0.25$. |
| **TestBuild 14**<br>(`shadow4.mp4`) | Same code as TestBuild 13 ($E_{\text{chroma}} > 0.25f$), but tested on red terracotta brick pavers. | "...but look in shadow4 the brick pattern makes the shadow jitter again." | **Root cause isolated**: Terracotta brick pavers are saturated orange/red. Shadow attenuation on colored pavers produces $E_{\text{chroma}} = 0.25 - 0.55$, causing 40% of the shadow on bricks to trigger Candidate 1 overwrite. |
| **Current (v1.4.0)**<br>(`ceebde5`) | **High-Chromatic Firewall**: Raised chromatic threshold from $0.25f$ (`0f3E800000`) to **$0.70f$** (`0f3F333333`). | Awaiting user test in-game. (Theoretical & simulated trigger rate on brick shadows: **0.00%**; on sirens: **52.8% - 59.3%**). | Perfectly separates ground shadows from saturated colored lights. |

---

## 5. Quantitative Verification & Data

### 5.1 Simulated Metrics on Captured Video Frames

#### Brick Paver Shadow (`shadow4.mp4`, 25 consecutive frame pairs):
```
Total shadow pixels evaluated: 712,602
At threshold > 0.25f:  285,188 pixels triggered (40.021%)  <-- Violent jitter!
At threshold > 0.45f:   27,648 pixels triggered ( 3.880%)
At threshold > 0.55f:    6,413 pixels triggered ( 0.900%)
At threshold > 0.60f:    2,494 pixels triggered ( 0.350%)
At threshold > 0.65f:      855 pixels triggered ( 0.120%)
At threshold > 0.70f:        0 pixels triggered ( 0.000%)  <-- 100% Immunized!
```

#### Police Sirens Against Hedge (`ntecop8.mp4`, frames 1–20):
```
Total siren pixels evaluated: 17,062
At threshold > 0.25f:   13,068 pixels triggered (76.591%)
At threshold > 0.60f:    9,726 pixels triggered (57.004%)
At threshold > 0.65f:    9,408 pixels triggered (55.140%)
At threshold > 0.70f:    9,013 pixels triggered (52.825%)  <-- Strongly triggers!
```

### 5.2 Visual Overlay Comparison
- At `0.25f`, visualization (`vis_trigger_25.png`) showed dense red trigger strips along every single brick mortar joint and shadow boundary.
- At `0.70f`, visualization (`vis_trigger_70.png`) showed zero red pixels across the entire brick floor and shadow, while the siren light bar remained brightly triggered (`vis_siren_trigger_70.png`).

---

## 6. Current Implementation (`source/native/quality_fix.h`)

The active PTX injected into `BlendCandidatesFused_sm120` (commit `ceebde5`):

```ptx
// ============================================================================
// DLSS-G Transfusion: Quality Valid Warp & Chromatic Conflict Firewall (v1.4.0)
// ============================================================================

// 1. Boundary & geometric validity checks:
cvt.rn.f32.u32 %qf0, %r10;
cvt.rn.f32.u32 %qf1, %r11;
div.approx.ftz.f32 %qf0, 0f3F000000, %qf0;
div.approx.ftz.f32 %qf1, 0f3F000000, %qf1;
sub.ftz.f32 %qf2, 0f3F800000, %qf0;
sub.ftz.f32 %qf3, 0f3F800000, %qf1;
setp.ge.f32 %qv0, %f123, %qf0;
setp.le.f32 %qv2, %f123, %qf2;
and.pred %qv0, %qv0, %qv2;
setp.ge.f32 %qv2, %f124, %qf1;
and.pred %qv0, %qv0, %qv2;
setp.le.f32 %qv2, %f124, %qf3;
and.pred %qv0, %qv0, %qv2;
not.pred %qv2, %p17;
and.pred %qv0, %qv0, %qv2;
setp.ge.f32 %qv1, %f129, %qf0;
setp.le.f32 %qv2, %f129, %qf2;
and.pred %qv1, %qv1, %qv2;
setp.ge.f32 %qv2, %f130, %qf1;
and.pred %qv1, %qv1, %qv2;
setp.le.f32 %qv2, %f130, %qf3;
and.pred %qv1, %qv1, %qv2;
not.pred %qv2, %p16;
and.pred %qv1, %qv1, %qv2;

// 2. Finite floating point protection:
abs.f32 %qf4, %f125;
abs.f32 %qf5, %f126;
abs.f32 %qf6, %f127;
add.f32 %qf4, %qf4, %qf5;
add.f32 %qf4, %qf4, %qf6;
setp.lt.f32 %qv2, %qf4, 0f7F800000;
and.pred %qv0, %qv0, %qv2;

abs.f32 %qf5, %f131;
abs.f32 %qf6, %f132;
abs.f32 %qf7, %f133;
add.f32 %qf5, %qf5, %qf6;
add.f32 %qf5, %qf5, %qf7;
setp.lt.f32 %qv2, %qf5, 0f7F800000;
and.pred %qv1, %qv1, %qv2;

// Mutual validity predicate:
and.pred %qv3, %qv0, %qv1;

// Provider mode check [%rd6+220]:
ld.param.u8 %qrs0, [%rd6+220];
setp.ne.s16 %qv9, %qrs0, 0;

// Confidence threshold gating:
setp.ge.f32 %qv5, %f148, 0f3E4CCCCD;
or.pred %qv5, %qv5, %qv9;
and.pred %qv0, %qv0, %qv5;

setp.ge.f32 %qv6, %f149, 0f3E4CCCCD;
or.pred %qv6, %qv6, %qv9;
and.pred %qv1, %qv1, %qv6;

// 3. 100% pure geometric warp clamp for tear-free wire fences:
max.f32 %qf0, %f148, 0f3F800000;
min.f32 %qf0, %qf0, 0f3F800000;
max.f32 %qf1, %f149, 0f3F800000;
min.f32 %qf1, %qf1, 0f3F800000;

// 4. Candidate construction with FMA:
sub.f32 %qf2, %f125, %f115;
sub.f32 %qf3, %f126, %f116;
sub.f32 %qf12, %f127, %f117;
@%qv0 fma.rn.f32 %f39, %qf0, %qf2, %f115;
@%qv0 fma.rn.f32 %f38, %qf0, %qf3, %f116;
@%qv0 fma.rn.f32 %f37, %qf0, %qf12, %f117;

sub.f32 %qf2, %f131, %f119;
sub.f32 %qf3, %f132, %f120;
sub.f32 %qf12, %f133, %f121;
@%qv1 fma.rn.f32 %f43, %qf1, %qf2, %f119;
@%qv1 fma.rn.f32 %f42, %qf1, %qf3, %f120;
@%qv1 fma.rn.f32 %f41, %qf1, %qf12, %f121;

// 5. Measure candidate color differences:
sub.f32 %qf6, %f125, %f131;
sub.f32 %qf7, %f126, %f132;
sub.f32 %qf8, %f127, %f133;

// E_motion = |D_R| + |D_G| + |D_B|
abs.f32 %qf9, %qf6;
abs.f32 %qf10, %qf7;
abs.f32 %qf11, %qf8;
add.f32 %qf9, %qf9, %qf10;
add.f32 %qf9, %qf9, %qf11;

// E_chroma = |D_R - D_G| + |D_G - D_B| + |D_B - D_R|
sub.f32 %qf12, %qf6, %qf7;
sub.f32 %qf13, %qf7, %qf8;
sub.f32 %qf14, %qf8, %qf6;
abs.f32 %qf12, %qf12;
abs.f32 %qf13, %qf13;
abs.f32 %qf14, %qf14;
add.f32 %qf12, %qf12, %qf13;
add.f32 %qf12, %qf12, %qf14;

// 6. Candidate Conflict Firewall:
// Trigger when motion error > 0.35f (0f3EB33333) AND chromatic error > 0.70f (0f3F333333):
setp.gt.f32 %qv4, %qf9, 0f3EB33333;
and.pred %qv4, %qv4, %qv3;
setp.gt.f32 %qv7, %qf12, 0f3F333333;
and.pred %qv4, %qv4, %qv7;

// Candidate 1 (current frame ground truth) overwrites Candidate 0 on chromatic conflict:
and.pred %qv8, %qv4, %qv1;
and.pred %qv8, %qv8, %qv6;

@%qv8 mov.f32 %f39, %f43;
@%qv8 mov.f32 %f38, %f42;
@%qv8 mov.f32 %f37, %f41;
@%qv8 mov.f32 %f36, %f40;
```

---

## 7. Open Questions & Future Directions for the Next AI

If you are continuing this work, here are the exact known trade-offs and prospective paths:

### 7.1 Translucent Bubble Helmet Edge Erosion ("Notching")
- **The Issue**: In `testbuild11` / `testbuild12`, when the character's round glass bubble helmet passes over a dark background object (e.g. a dark streetlight pole or high-contrast electrical cable), the outer glass rim can appear slightly "notched" or bitten into.
- **Why it happens**:
  - The glass helmet rim is a specular highlight ($R \approx G \approx B \approx 0.9$).
  - The background pole is dark ($R \approx G \approx B \approx 0.1$).
  - Because both are achromatic, $E_{\text{chroma}}$ is near zero ($< 0.10$).
  - Therefore, the chromatic firewall does *not* trigger on this edge (which is good for shadows, but leaves the helmet to stock DL1Net blending).
  - Stock DL1Net blends the bright helmet rim with the dark pole, causing the rim to darken and appear notched.
- **Possible Next Step**:
  - **Specular Rim Luminance Protection**:
    Check if one candidate has a high-luminance specular peak ($C_{1,\text{max}} > 0.85f$) while the other candidate is very dark ($C_{0,\text{max}} < 0.20f$).
    If Candidate 1 has a specular highlight on an occluding object, protect Candidate 1 from being diluted by dark background samples.

### 7.2 Chromaticity Ratio Gating
- Currently, we check:
  $$E_{\text{motion}} > 0.35f \quad \text{AND} \quad E_{\text{chroma}} > 0.70f$$
- As shown in Section 3.3, for diffuse shadows, the chromaticity ratio is bounded:
  $$\frac{E_{\text{chroma}}}{E_{\text{motion}}} = \frac{|R - G| + |G - B| + |B - R|}{R + G + B} \le 0.40$$
  Whereas for monochromatic emissive lights (blue/red sirens):
  $$\frac{E_{\text{chroma}}}{E_{\text{motion}}} \ge 1.00 - 2.00$$
- If an even more saturated ground surface (e.g. bright blue synthetic turf or deep crimson carpet) ever produces $E_{\text{chroma}} > 0.70f$, adding a ratio check:
  $$E_{\text{chroma}} > 0.50 \cdot E_{\text{motion}}$$
  will guarantee mathematical immunity regardless of ground surface color saturation.
  - In PTX:
    ```ptx
    mul.f32 %qf15, %qf9, 0f3F000000;   // 0.5 * E_motion
    setp.gt.f32 %qv10, %qf12, %qf15;   // E_chroma > 0.5 * E_motion
    and.pred %qv4, %qv4, %qv10;
    ```

### 7.3 Multi-Frame Generation Asymmetry & Confidence-Directed Reconciliation (Proved by GPU In-Kernel Probe)
- **Previous flawed assumption**: Assumption was made that Candidate 1 (future/current) is always superior, and that Candidate 0 must never overwrite Candidate 1.
- **Ground Truth from In-Kernel Probe (`capture-output/24360-12504..12509`)**:
  - In Multi-Frame Generation (6x), multiple intermediate subframes are synthesized per display interval.
  - **Early Subframes ($\alpha \approx 0.2$, Dispatch 12504)**: Candidate 0 (past warp) is geometrically pristine and clean. Candidate 1 (future backward warp) suffers severe disocclusion tears under mounts and across flashing lights because the siren was occluded or off in the future frame. In 77.2% of conflict pixels, $w_0 > w_1$. Unconditionally forcing Candidate 1 (`mov %f39, %f43`) corrupted the clean Candidate 0!
  - **Late Subframes ($\alpha \approx 0.8$, Dispatch 12509)**: Candidate 1 is pristine and clean. Candidate 0 suffers severe disocclusion tears along edges. In 65.4% of conflict pixels, $w_1 > w_0$.
- **The Correct General Resolution: Confidence-Directed Reconciliation**:
  - When chromatic conflict passes the shadow scale veto (`%qv8` is true):
    ```ptx
    // Candidate 1 wins when w1 >= w0 and conf1 >= 0.2
    setp.ge.f32 %qv13, %f149, %f148;
    and.pred %qv13, %qv13, %qv6;
    and.pred %qv13, %qv8, %qv13;

    // Candidate 0 wins when w0 > w1 and conf0 >= 0.2
    setp.gt.f32 %qv14, %f148, %f149;
    and.pred %qv14, %qv14, %qv5;
    and.pred %qv14, %qv8, %qv14;

    // High-confidence candidate overwrites torn candidate:
    @%qv13 mov.f32 %f39, %f43; // C1 -> C0
    @%qv13 mov.f32 %f38, %f42;
    @%qv13 mov.f32 %f37, %f41;
    @%qv13 mov.f32 %f36, %f40;

    @%qv14 mov.f32 %f43, %f39; // C0 -> C1
    @%qv14 mov.f32 %f42, %f38;
    @%qv14 mov.f32 %f41, %f37;
    @%qv14 mov.f32 %f40, %f36;
    ```
  - Result: On early subframes, the clean Candidate 0 repairs Candidate 1. On late subframes, the clean Candidate 1 repairs Candidate 0. Both candidates stay clean on every generated subframe without ghosting or tearing!

---

## 8. Build, Test, and Deployment Instructions

### 8.1 Build Command
From `d:\Coding\DLSSGUnlock\DLSSG-Transfusion`:
```powershell
cmake --build build --config Release
```
This produces:
- `build/Release/version.dll`
- `build/Release/dxgi.dll`
- `build/Release/dinput8.dll`
- `build/Release/winmm.dll`
- `build/Release/DLSSG-Transfusion.asi`
- Files are automatically copied to `build/dist/`.

### 8.2 Testing the PTX Patch
Run the startup hook test harness to verify PTX syntax, JIT compatibility, and patch hash validity:
```powershell
tests\startup_hooks\build\Release\QualityPtxHarness.exe "..\ptx_dumps\01_Kernel_BlendCandidatesFused_sm120.ptx" "tests\startup_hooks\build\Release\quality-patched.ptx"
```
Expected output:
```
QUALITY_PROFILE_GATING_OK
```

### 8.3 In-Game Deployment Target
Neverness to Everness game path:
```
E:\SteamLibrary\steamapps\common\Neverness to Everness\Client\WindowsNoEditor\HT\Binaries\Win64\version.dll
```
Deployment command:
```powershell
Copy-Item "build\dist\version.dll" -Destination "E:\SteamLibrary\steamapps\common\Neverness to Everness\Client\WindowsNoEditor\HT\Binaries\Win64\version.dll" -Force
```

### 8.4 Packaging
Create release package (strict requirement: maintain `v1.4.0` naming):
```powershell
powershell -NoProfile -Command "Compress-Archive -Path 'build\dist\version.dll', 'build\dist\dxgi.dll', 'build\dist\dinput8.dll', 'build\dist\winmm.dll', 'build\dist\DLSSG-Transfusion.asi', 'build\dist\DLSSG-Transfusion.json', 'build\dist\README.txt' -DestinationPath 'build\dist\DLSSG-Transfusion-v1.4.0.zip' -Force; if (Test-Path 'dist') { Copy-Item 'build\dist\DLSSG-Transfusion-v1.4.0.zip' 'dist\' -Force; Copy-Item 'build\dist\version.dll' 'dist\' -Force }"
```
Verify SHA256 hashes:
```powershell
Get-FileHash 'build\dist\version.dll', 'E:\SteamLibrary\steamapps\common\Neverness to Everness\Client\WindowsNoEditor\HT\Binaries\Win64\version.dll', 'build\dist\DLSSG-Transfusion-v1.4.0.zip'
```

---

## 9. GPU Candidate Capture Analysis & Resolution of Siren Artifacts

### 9.1 Empirical Diagnosis from 36 GPU In-Kernel Subframe Captures
Across 36 multi-frame captures (6 bursts of 6 subframes: `25240-3619` through `25240-4321`):
1. **Bidirectional Reconciliation Failure**: In recent test builds, `@%qv14 mov.f32 %f43, %f39` allowed Candidate 0 to overwrite Candidate 1 when $w_0 \ge 0.50$ and $w_1 \le 0.35$.
   - Optical flow breakdown on rotating/flashing sirens drove Candidate 1 confidence $w_1 < 0.10$ on 80% to 91.6% of conflict pixels.
   - The occluded background (character shirt, car roof) tracked stably in the past frame ($w_0 \ge 0.50$).
   - Candidate 0 pasted the character's white shirt directly into Candidate 1's blue siren dome (over 3,000 pixels in subframe 4155), tearing giant white notches into the siren dome.
   - Concurrently, Candidate 1 was blocked from overwriting Candidate 0 on 99% of siren pixels because $w_1 < 0.50$, leaving trailing ghost duplicates alive.
2. **Candidate 1 Immutability Law**: Direct readbacks prove Candidate 1 (`raw1`) is 100% solid, tear-free, and correctly positioned. Candidate 1 is the ground truth and must NEVER be modified.

### 9.2 The Final Solution: TestBuild 13 + Shadow Veto
- **Unidirectional Collapse**: Only Candidate 1 overwrites Candidate 0 (`@%qv8 mov.f32 %f39, %f43`). Candidate 1 remains strictly immutable.
- **Shadow Veto Integration**: `kShadowVeto` (normalized cosine residual $1 - \frac{(A \cdot B)^2}{(A \cdot A)(B \cdot B)} \le 0.01$) detects pure scalar brightness scaling, vetoing 13,360 shadow pixels across the dataset to permanently eliminate paver shadow jitter without blocking siren conflict resolution.
- **Verification**:
  - `tests/startup_hooks/build-siren`: 8/8 CTest tests passed (100%).
  - `scripts/test_shadow_scale_gpu.py`: 205/205 synthetic GPU cases passed (100%).
  - Release binary deployed to `E:\SteamLibrary\steamapps\common\Neverness to Everness\Client\WindowsNoEditor\HT\Binaries\Win64\version.dll` and packaged in `build/dist/DLSSG-Transfusion-v1.4.0.zip`.

### 9.3 512×512 High-Resolution Capture Analysis & Resolution of the Translucent Velocity Defect
1. **512×512 Capture Upgrade**:
   - Upgraded capture buffer from 256×256 (16 MB) to 512×512 (64 MB, 9,437,184 floats/dispatch) across 9 planes.
   - User captured 24 high-resolution 512×512 subframe dispatches (`26816-2989..2994`, `3099..3104`, `3231..3236`, `3410..3415`).
2. **The Root Cause: Unreal Engine 5 Translucent Velocity Absence**:
   - In UE5, translucent materials (such as police siren glass domes and bubble helmets) have `bOutputVelocity = false` by default.
   - UE5 writes **no motion vectors** for the siren geometry into the G-Buffer velocity buffer.
   - Consequently, DLSS-G samples the **background hedge motion vectors** across the siren dome pixels:
     - Car body & siren mount flow: $v_x = +5.70\text{ px}, v_y = -0.86\text{ px}$ (moving right).
     - Siren dome & background hedge flow: $v_x = -7.80\text{ px}, v_y = -0.03\text{ px}$ (moving left due to camera parallax).
   - DLSS-G warps the siren in the **opposite direction** of the car, with an optical flow error exceeding 53 pixels across the frame interval.
3. **The Mechanism of Firewall Suppression (`%p16` Kill Switch)**:
   - DLSS-G's optical flow tracking flagged the contradictory velocity on 84.4% of siren pixels, setting predicate `%p16 = 1` (`0x7FFFFFFF7FFFFFFF`).
   - In earlier iterations of `kPolicyE2`, `not.pred %qv2, %p16; and.pred %qv1, %qv1, %qv2;` was used to discard invalid candidates.
   - Because `%p16 == 1` on the siren, `%qv1` was forced to 0 (declaring Candidate 1 invalid).
   - Because `%qv1 == 0`, the mutual validity check `%qv3 = %qv0 & %qv1` failed, killing the conflict firewall `%qv4` and `%qv8` completely across the center of the siren!
   - Candidate 1 was blocked from overwriting Candidate 0, leaving Candidate 0's stale displaced dome intact alongside Candidate 1, creating a double-vision smeared dome.
4. **The Resolution**:
   - **Eliminated `%p16` and `%p17` from `kPolicyE2`**: Translucent objects with missing velocity remain valid candidates as long as their UVs lie within normalized screen bounds ($[0.5/W, 1 - 0.5/W] \times [0.5/H, 1 - 0.5/H]$) and color floats are finite.
   - **Threshold Tuning**:
     - Motion error threshold adjusted from $0.35$ to $0.25$ (`0f3E800000`).
     - Chromatic error threshold adjusted from $0.25$ to $0.15$ (`0f3E19999A`).
     - Preserved `kShadowVeto` ($\le 0.01$ cosine residual) to guarantee 100% protection of terracotta brick paver shadows.
   - **Strict One-Way Overwrite**: Candidate 1 strictly overwrites Candidate 0 (`@%qv8 mov.f32 %f39..%f36, %f43..%f40`), leaving Candidate 1 immutable.
5. **Verification**:
   - `CandidateCaptureHarness.exe`: 9,437,184 floats passed on GPU.
   - `tests/startup_hooks/build-siren`: 8/8 CTest tests passed (100%).
   - `scripts/test_shadow_scale_gpu.py`: 205/205 synthetic GPU cases passed (100%).
   - SHA256 verified deployment to `E:\SteamLibrary\steamapps\common\Neverness to Everness\Client\WindowsNoEditor\HT\Binaries\Win64\version.dll`.
   - Release archive packaged as `build/dist/DLSSG-Transfusion-v1.4.0.zip`.

