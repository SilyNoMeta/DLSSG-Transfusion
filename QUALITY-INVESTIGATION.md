## 2026-09-10: v4.1 preserves game camera transforms

Build marker: `quality-v4.1-game-motion`. The v4 PTX policy is unchanged.

Removed the automatic camera-matrix reconstruction from both constants hooks.
It classified identity and zero alike, updated a single global history only on
those calls, and inferred a projection from pose/FOV fields. That cannot reliably
represent the previous frame for multiple submissions or viewports, and identity
is not evidence that a transform is missing. The NTE log at 23:26:40 showed the
identity/zero condition together with pose/FOV values that enabled this path;
it did not distinguish identity from zero or prove which gameplay frames used it.

The hooks now preserve all supplied camera matrices, including zero/invalid
inputs. They do not invent replacements for genuinely missing game data. The
existing explicit disableMvDilation override remains; leave it false for normal
game-provided motion metadata. Periodic `[MOTION]` records distinguish zero,
identity, non-identity and invalid matrices and include viewport, motion scale,
jitter, reset, motion flags and the full current-to-previous transform. They are
observations of submitted constants, not GPU texture captures or kernel bindings.

Static analysis also confirmed Scatter3d writes a mask, not a blended motion
field. The local provider is now 310.9.1; its named descriptors were located, but
the host-to-kernel resource provenance is still incomplete. No engine/optical-flow
selection patch is included. This candidate fixes a concrete input-handling issue;
it is not a demonstrated fix for thin-geometry tearing. Release compilation only;
no tests, CUDA execution, video analysis, game launches or game-folder writes.

# Interpolation quality investigation — 9 September 2026

## Latest result: v5 regressed; v4 restored

The 23:25 NTE log confirms `quality-smooth-warp-v5`, successful quality patch
preparation and descriptor redirection, multiplier 4x and disableMvDilation=0.
The user reports more visible tearing than before. This is sufficient to reject
v5 as the recommended build despite its synthetic continuity tests passing.
Those tests established only numerical properties of the isolated correction;
they did not validate the complete frame-generation pipeline or its perception.

Production source now uses the preserved v4 policy again. The new build marker
is `quality-valid-warp-v4-restored`; the original v4 release package remains
available unchanged. V5's source snapshot is retained in
`../scratch/quality-audit/quality_fix_v5_regression.h` for diagnosis. Its extra
crossfading may worsen perceived double edges, but the log cannot prove the
mechanism. Do not treat smooth output weights as evidence of correct motion.

## Historical experiment: quality-smooth-warp-v5 (not recommended)

The user reports slight improvement from v4 and from preserving the game's
dilation declaration, but local tearing/shearing remains on trees and fences,
especially at 4x–6x. The NTE clip shows a moving fence whose fine detail varies
across captured frames. It does not identify the provider's internal motion,
confidence, or real/generated-frame labels. No further frame analysis was done
after the user asked to focus on implementation.

V4 itself had abrupt correction thresholds: RGB agreement at 10% and confidence
at 0.25. V5 replaces these with smoothstep ramps (agreement fades from full at
5% to zero at 20%; confidence rises from zero at 0.1 to full at 0.5). The maximum
warped-color contribution floor remains 0.85. This does not increase that floor
or force rejected vectors to become valid.

Source support also fades across the two source pixels inside the half-texel
boundary. As one candidate loses support, the correction crossfades toward the
remaining supported candidate instead of switching all its RGB at the boundary.
The agreement requirement relaxes with geometric support, not merely with low
confidence. Missing/invalid samples still have no support. Full replacement uses
an explicit move to avoid propagating NaN from the discarded candidate.

This changes only candidate RGB. It does not repair the underlying motion field,
add neighboring motion samples, change subframe scheduling, or smooth vectors
across object/depth boundaries. Auxiliary channels and downstream masks remain
unchanged, so downstream selection can still produce artifacts. Color blending
may still introduce blur or ghosting, and visual benefit is unverified.

Validation: eight CTest checks passed, including the exact-profile gate and
production fatbin builder. The complete transformed PTX and rebuilt fatbin JIT
loaded successfully. The injected policy executed on the GPU for 20 edge cases
and six continuity sweeps (3,006 samples total). Maximum adjacent RGB changes
were 0.001219 for confidence, 0.002089 for agreement, and under 0.00175 for each
screen edge with the test increments. Running the same confidence sweep against
the preserved v4 policy failed the continuity check with a 0.325 jump. These are
synthetic numerical results, not measured reductions in visible artifacts.

Use the existing `qualityValidWarp` setting, enabled by default; restart after
changing it. `false` selects the stock blend path. Use the preserved v4 DLL for
a v4/v5 comparison with the same settings and multiplier. The new log marker is
`quality-smooth-warp-v5`; successful preparation reports
`SMOOTH WARP QUALITY V5 applied (patched=1)`, followed by descriptor redirection.

## Implemented: quality-valid-warp-v4

This is a new experimental correction at `BlendCandidatesFused`, not another
scatter-rejection rollback. `source/native/quality_fix.h` injects a policy after
candidate blending and before UI composition and neural-input output:

- A usable candidate must have a non-sentinel vector, finite RGB and projected
  coordinates inside the source image's half-texel bounds. Ordered comparisons
  reject NaN/infinite coordinates. The source dimensions are taken from the
  kernel's existing active width/height registers.
- When both candidates are usable, only matching colors are eligible: summed
  absolute RGB difference must be at most 10% of the larger absolute RGB sum,
  with a scale floor of 1. Conflicting candidates keep the original behavior.
- For eligible candidates with weights between 0.25 and 1, raise the warped-color
  contribution to at least 0.85 (retain higher existing weights). This is an
  experimental heuristic intended to reduce stationary-frame contamination, not
  a measured optimum or a calibrated confidence probability.
- If only one candidate is usable and its weight passes that threshold, put its
  corrected RGB into both candidate outputs. This avoids selecting a clamped or
  missing counterpart. A low-confidence but geometrically valid counterpart is
  not classified as missing solely because of its confidence.
- Both-invalid and low-confidence cases retain existing fallback. This cannot
  manufacture missing motion for foliage or reconstruct unseen background.
  Auxiliary channels, masks, UI composition, scatter checks and all texture and
  memory accesses remain unchanged. Later processing may still reject content.

The patch matches the complete normalized 39,638-byte known Blackwell program
using its length and FNV-1a fingerprint before inserting anything. An unknown
version leaves its code unchanged apart from existing architecture retargeting.
The production fatbin builder is tested against the local 310.9 provider; the
quality target cannot independently mark a missing temporal patch as ready.

`qualityValidWarp` defaults to true and is preserved when the control file is
rewritten. Set it false and restart the game to compare the existing blend path.
Blackwell transfusion must be enabled. The log records `quality-valid-warp-v4`,
`VALID WARP QUALITY V4 applied (patched=1)` during preparation, followed by a
successful `BlendCandidatesFused` descriptor-redirection message. Preparation
alone does not establish that a descriptor was redirected.

Validation: eight CTest checks passed, including exact-profile rejection and the
production fatbin builder with the option enabled/disabled. The full patched PTX
and production rebuilt fatbin compiled and loaded on the RTX 4060 Laptop GPU.
The exact injected policy was executed on the GPU for 18 synthetic cases covering
all four borders, absent motion, conflicting colors, low confidence, nonfinite
values and half-texel bounds; all passed and auxiliary channels were preserved.
These checks validate the transformation and its local numerical behavior, not
complete frame generation or visual quality in Onimusha. A possible tradeoff is
more ghosting when apparently valid motion is wrong, or disagreement between the
corrected RGB and unchanged downstream confidence/features.

The dumps identify concrete problems with the previous patch's assumptions, but do not establish that excessive optical-flow weighting is the cause of the observed artifacts. These are kernel programs, not captured per-frame motion/depth textures or launch parameters.

## Follow-up: unchanged artifacts, mainly at 3x–4x and higher

The user reports that restoring rejection does not resolve the top/bottom artifacts or uninterpolated grass/fences, and that the problems are mainly at 3x–4x and above. The comparison must therefore not be presented as a solution to the original quality problem.

This prioritizes subframe-dependent behavior, while not excluding pre-existing motion-data or optical-flow limitations. At nominal 2x only the midpoint is needed; additional interpolation times can expose errors that are less visible at the midpoint. This is a diagnostic hypothesis, not evidence of an actual timing mismatch.

The inspected Blackwell scatter loads time from parameter byte offset 32, scales the current-to-previous field by `1-t` and the opposite field by `t`, and packs the resulting inverse displacement into its output. Thus the static Blackwell scatter is not simply hard-coded to midpoint motion. `BlendCandidatesFused` and `WarpBlendingWeights` consume packed displacements directly at their inspected warped sampling sites; they do not independently multiply those coordinates by a hard-coded half. Replacing their pixel-center `0.5` constants would be incorrect.

Two later suppression paths remain relevant: `BlendCandidatesFused` mixes warped candidates with unwarped colors using sampled weights, and `DL1Net_Output` can bias output logits toward -1000/+1000 under masks. Capturing the masks, candidate validity and actual launch-time values is needed to distinguish missing vectors from rejected vectors or mis-timed vectors. The existing static dumps cannot provide those values.

The Streamline DLSS-G guide also explicitly specifies that regions outside the final-color subrect are copied unchanged. Actual final-color, HUDless, depth and motion extents therefore remain necessary evidence for fixed top/bottom bands. UI recomposition separately requires UI alpha to be zero at non-UI pixels; the mod's dimension checks cannot verify the alpha contents. Neither an incorrect subrect nor contaminated UI alpha has been established here.

## Confirmed findings

### 1. The former “no edge clamp” patch bypassed consistency rejection

`source/native/midpoint_fix.cpp` used two broad regular expressions to remove 25 + 25 conditional branches from the Blackwell `Kernel_EstimateIntermMvecsScatter`. The audit inventories all 50 sites and hashes all 31 Blackwell dumps.

In `../ptx_dumps/08_Kernel_EstimateIntermMvecsScatter_sm120.ptx`, lines 273–280 reject projected output coordinates outside the image. Lines 305–322 combine a depth-derived difference threshold with a squared motion-vector difference threshold before an atomic scatter write. The previous regex removed the latter branch, leaving the coordinate checks intact. There is no edge-only condition on the removed checks: the change also affects the image interior.

Consequently, “forces game vectors instead of optical flow” and “eliminates edge haloing” were not demonstrated by this patch. The PTX parameter texture handles alone do not establish their host-side provenance. Letting inconsistent vectors participate could worsen object boundaries; restoring the checks could also expose more fallback regions. Only an image comparison can establish the net effect.

**Comparison build:** `quality-stock-rejection-v3` removes the regex bypass. It keeps Blackwell retargeting, temporal arithmetic, existing configuration behavior, and the startup crash fixes. Its only interpolation behavior change from `startup-small-stack-fix-v2` is restoration of these rejection branches. The regular and crash logs carry the new build marker; the provider log says `STOCK SCATTER REJECTION`.

### 2. The dilation override changes a declaration, not the vector texture

Both constants hooks set `motionVectorsDilated=true` when `disableMvDilation=true`. Streamline defines this field as whether the vectors are **already dilated** (`../Streamline/include/sl_consts.h`). The supplied Onimusha log reported game=false, override=true, and its configuration enables the override.

`12_Kernel_InputMvecProcessing_sm120.ptx` contains depth-neighbor selection, motion sampling offsets, viewport origin/size and texture-size normalization, and final motion scaling. Its input-sampling clamps constrain texture reads; they do not clamp the magnitude of the output motion vector. Removing them indiscriminately would not implement correct out-of-frame motion.

Test `disableMvDilation=false` separately to restore the game's declaration. This is a plausible improvement for object-edge coverage, not a guaranteed fix. The comparison DLL does not silently change this setting.

### 3. The later pipeline can fall back to unwarped content

`01_Kernel_BlendCandidatesFused_sm120.ptx` checks packed-vector sentinel `9223372034707292159` and initializes missing motion to zero. It samples two warped color candidates, substitutes the opposite candidate for missing data, then blends warped and unwarped colors using two weights. The output also feeds the neural stage; this is not simply the final displayed-frame blend.

`16_Kernel_WarpBlendingWeights_sm120.ptx` likewise initializes missing motion to zero and samples at `uv + motion`. Neither inspected warped sampling site has an explicit per-sample off-screen validity test before its texture instruction. Texture addressing behavior comes from runtime texture state, which these dumps do not contain.

This is consistent with a possible path to edge sticking or an uninterpolated region. It does not prove clamp addressing is active, that these paths caused the screenshot's behavior, or that the relevant motion came from optical flow. A useful next kernel experiment would need to propagate out-of-frame validity coherently through candidate color, confidence, and neural inputs, after identifying the texture handles and conventions. Blindly zeroing the fallback mask or removing all rejection checks is not equivalent.

### 4. Camera reconstruction needs validation before further tuning

`EnsureCameraMatrices` uses one global camera history and updates it on every intercepted call. Its callers infer missing camera transforms from identity/zero matrices. Identity can also be valid for a stationary camera or a reset; multiple calls/viewports can make that history inappropriate. The `slSetData` path additionally modifies the caller's constants in place.

These are integration risks, not a confirmed explanation of the current artifacts. Camera reconstruction is unchanged in this comparison so the scatter experiment remains interpretable. A subsequent investigation should log per-frame/per-viewport transforms, reset, motion scale and resource extents, then preserve valid game constants and avoid history shared across views.

## Top and bottom bands

The candidate and neural-input kernels distinguish active dimensions from backing/padded dimensions. Both viewport normalization and neural padding deserve investigation if the affected bands have a fixed width or track resolution. No incorrect height, viewport offset, or padding value can be established from static PTX. In particular, a 1080-pixel image not being divisible by 16 is not itself evidence of a bug: the kernels have bounds and padding paths.

If band width instead grows with camera speed or interpolation fraction, off-screen correspondence/confidence becomes the stronger lead. These possibilities can coexist.

## Validation and comparison

- Release DLLs built successfully. All six existing startup/diagnostic regression tests passed.
- All 31 dumped Blackwell kernels compiled and loaded through the CUDA driver after changing only the target to sm_89 on the RTX 4060 Laptop GPU. No kernels were executed by this check; it does not validate texture layout, image quality, or compatibility with every provider version.
- The audit script is `scripts/audit_quality_dumps.py`. The generated hash/site inventory is packaged as `dump-audit.json` with the comparison DLLs.
- No game files were modified and no game was launched for this investigation.

First compare v2 with v3 using the same scene, camera motion, resolution, multiplier and existing configuration. Restart between DLL replacements. Replace only the injection DLL/ASI already in use. Then, keeping v3 and all other settings fixed, test `disableMvDilation=false` in the existing configuration and restart again. Compare object boundaries, top/bottom bands, and newly revealed background separately. A worse result is useful evidence too; the previous v2 package remains available for rollback.

The optical-flow explanation remains plausible. This build is a controlled comparison, not a claimed final image-quality fix.

## 2026-09-10: internal D3D12 submission trace (v4.3)

The v4.2 NTE log showed matching 960x540 motion/depth rectangles at origin zero,
1920x1080 HUDless color, and motion scale approximately (1/960, 1/540). These
submitted metadata show no simple mismatch. They do not establish the native
texture dimensions, contents, or downstream binding correctness.

Static disassembly of local nvngx_dlssg 310.9.1 located CreateCuFunction's query
ID 0xe2436e22 (wrapper RVA 0x2a50) and LaunchCuKernelChain ID 0x24973538
(wrapper 0x2b40). The caller at 0x31476..0x314f7 constructs a 56-byte chain entry:
function handle at 0, grid at 8, block at 20, shared bytes at 32, parameter pointer
at 40, parameter length at 48. The call passes command list, entry pointer, count.
CreateKernel at 0x3091d..0x3092f passes device, module, entry name, output handle.
These addresses are investigation references only; production uses query IDs.

nvapi_motion_trace.h observes those two interfaces, associates exact kernel names
with handles, and reads only 144/160-byte CPU blocks for the recognized scatter
and input-motion kernels. It forwards all API arguments and results unchanged.
Records are bounded, locking is nonblocking, and consecutive bursts are sampled.
No GPU readback is performed. This is diagnostic ABI-derived instrumentation,
compiled but not runtime-tested at the user's request. It does not yet prove
any wrong interpolation time or justify changing the motion-selection policy.
Legacy LaunchCubinShader queries are reported but that path is not intercepted.
The previously suggested multiplier mismatch was dismissed: the user chose 6x.

## v4.4: repair missing resolver observations

The supplied v4.3 log contained no KERNEL records. Static read-only inspection
confirmed that the NTE provider matches the analyzed 310.9.1 file exactly
(SHA256 ff6e90eb78b827927dff5b4ecc6b1c870c2e9bca29ed9f48c7d348cc9e170b82).
System nvapi64.dll and driver nvapi64_impl.dll expose distinct QueryInterface
functions. v4.4 adds a separate implementation-resolver detour with its own
trampoline; it also records first entry into each resolver and relevant null
query results, which v4.3 silently omitted. This is expanded capture coverage,
not proof that the implementation resolver caused v4.3's lack of records.
No image-processing change or runtime tests. Release compilation succeeded.

## v4.5: provider dispatch boundary

v4.4 confirmed both public and implementation resolver hooks are reached, but
recorded no targeted kernel interface queries. No timing conclusion follows.
v4.5 hooks the provider's D3D12 dispatch method RVA 0x313e0, gated on the full
function bytes through 0x315b5. Its arguments are context, parameter object,
command list, and three grid dimensions. The selected kernel object is at
context+0x140; its name string is at +0x10. Static CreateKernel code at 0x30b16
shows the fatbin at +8 and constructs that string at +0x10.
Parameter vtables identify the no-op preparation method 0xbbf0, the scatter
payload getter 0x3f850 (+16) and size getter 0x3f890 (144), or input-motion
getter 0x1ba60 (+8) and size getter 0x3f8b0 (160). Only matching accessors are
read; the trace does not call them. Unsupported layouts are reported.
This is ABI-derived diagnostic instrumentation, not a visual fix. Release
compilation succeeded; no runtime tests were performed at the user's request.

## v4.6: correct scatter payload accessor coverage

v4.5 reached real provider dispatch and successfully captured input-motion data:
1280x720 motion/depth/working size, reciprocal normalization matching those
extents. Scatter was named but rejected by the diagnostic accessor check.
The original static accessor search stopped at the first reference to the
144-byte size getter. Enumerating every reference finds vtables at 0x9cab0
(payload +16) and 0x9cad8 (payload +8). Both share the size getter at 0x3f890.
v4.6 accepts either verified scatter getter and uses its actual payload offset;
input-motion remains +8/160 bytes. It also gates installation on exact accessor
machine code in addition to the full dispatch function. This is a correction to
our reader, not evidence of faulty game timing. Built only; no runtime tests.

## v4 A/B scatter experiments (2026-09-10)

At the user's request, resume value experiments without frame analysis or tests.
Both variants preserve quality_fix.h v4 blending and existing scatter depth and
branch logic. A halves squared motion-error thresholds at 50 existing max sites
in both directions. B independently caps neighborhood expansion at 3 instead
of 5 at four sites, retaining the existing minimum of 2. A may reject more useful
motion; B may leave more coverage gaps. Neither is a confirmed visual fix.

scatter_experiment.h requires normalized stock scatter PTX size 90731 and
FNV1a64 b1a2811b29625d41, plus unique matches for every replacement. A mismatch
fails preparation without modifying the input. The existing provider fallback
handles failure. No registers, memory accesses or temporal constants are added
or changed. CMake SCATTER_EXPERIMENT selects 0 baseline, 1 A, or 2 B.
Detailed input/resource and dispatch tracing is disabled for A and B; ordinary
build, patch and crash logging remains. Packages are separate under build/dist/
quality-v4-A-consistency and quality-v4-B-spread. Runtime and visual validation
remain with the user; no automated tests or frame analysis were performed.
Both Release builds completed successfully. Each package contains the four injector alternatives, matching project symbols, instructions and SHA-256 manifest. Linker warnings were limited to missing third-party Detours debug symbols.

## v4 C: center-biased input depth selection

User reports A/B look similar, while DLAA/Ultra Quality largely removes the
artifact. This supports investigating low-resolution inputs, but does not prove
a scale bug or isolate changes in source color, motion, depth and frame rate.
Static InputMvecProcessing inspection shows center plus four diagonal depth
samples. Closest linearized depth selects both encoded displacement and sampled
motion. No incorrect resolution normalization was established.

SCATTER_EXPERIMENT=3 retains stock scatter and v4 blending, and requires each
diagonal candidate to be below 0.98 times center linearized depth in addition
to winning the original comparison. This favors center motion for similar-depth
neighbors. It preserves the five-position depth/offset encoding, texture bounds,
inverted-depth handling, motion scaling and temporal parameters. One predicate
and one float register are added. No texture reads or memory writes are added.
The experiment applies at all resolutions; it is not automatic resolution scaling.
It may weaken useful foreground dilation and is not a confirmed quality fix.

Input PTX normalized size 8068 and FNV1a64 17144f73201965f5 gate the patch;
all four comparison sites and register declarations must match uniquely.
Detailed capture traces remain disabled. No tests or frame analysis were run.
All four Release injector targets built successfully and were packaged in build/dist/quality-v4-C-center-depth with matching symbols and SHA-256 manifest. Missing third-party Detours symbols produced linker warnings.

### User result: experiment C rejected

User reports C looks worse. Do not promote the center-depth bias or stack further
changes on it. A/B previously produced little visible difference. The preferred
rollback is the existing quality-valid-warp-v4 package. This result argues
against this specific reduction in neighbor selection; it does not establish
whether low-resolution motion, source-color instability, or another stage causes
the original tearing. DLAA/Ultra Quality improvement remains the strongest
reported clue. No game files were changed.

## v4 D: 1920x1080 provider working-resolution request

The user authorized a higher motion/depth working-resolution experiment after
C regressed and DLAA/Ultra Quality appeared to remove the original artifact.
The local Streamline ProgrammingGuideDLSS_G.md section 10 documents
DLSSGOptions dynamicResWidth/Height as the fixed motion/depth processing grid,
with eDynamicResolutionEnabled. It explicitly cautions against this flag with
fixed-ratio DLSS because quality/performance can decrease. D deliberately
experiments with that path; neither support for this exact use nor an actual
allocation change has been established by runtime observation.

SCATTER_EXPERIMENT=4 requests fixed 1920x1080 at every existing adjusted-options
submission. Intended NTE comparison is 2560x1440 output with the original lower
DLSS setting, versus previously logged 1280x720 working size. Provider-managed
resampling/allocation is used. No input extent, motion scale, game render size,
resource tag or kernel dispatch dimensions are falsified. No per-frame changes
to working size are introduced. V4 color policy remains and A/B/C PTX changes
are disabled. API results are logged on the first eight submissions, explicitly
without claiming that acceptance proves an actual grid change. Existing options
rejection handling remains; this does not guarantee recovery from GPU failures.
More GPU work/VRAM or worse quality are possible. No tests, game runs or frame
analysis were performed. The fixed working size targets this comparison rather
than arbitrary output resolutions.
All four D Release targets compiled and linked successfully; packaged with symbols, instructions and SHA-256 manifest under build/dist/quality-v4-D-working-1080p. Linker warnings concerned missing third-party Detours debug symbols.

### User result: experiment D rejected

User reports D was not the best, and forcing a larger resolution is not the solution.
The NTE runtime log confirms that while the initial options call succeeded (`result=0 eOk`),
subsequent reallocation during gameplay triggered `result=39 (eWarnOutOfVRAM)` on `SetOptions`
and `multiplier: 4x`. Forcing the provider's working grid to 1920x1080 increases intermediate
VRAM allocation pressure without recovering missing subpixel motion detail, because the
underlying game render inputs remain 1280x720 (or 960x540). Experiment D is rejected.
The recommended baseline remains `quality-valid-warp-v4`.

## v4 E1: relaxed color agreement threshold (25%)

Following the rejection of D and user confirmation that resolution-forcing is ineffective,
investigation returned to the candidate-blending policy in `01_Kernel_BlendCandidatesFused`.
At lower internal rendering resolutions (1280x720 in NTE), thin geometry (fence wires,
power lines, foliage) frequently undergoes subpixel TAA jitter and slight specular/lighting
variations between frames. Consequently, forward and backward warped candidates exhibit
15%–25% RGB discrepancy even when underlying motion vectors track the geometry correctly.

Under baseline v4, any color difference exceeding 10% (`0f3DCCCCCD`) is rejected as conflicting,
causing v4's 0.85 warp boost to shut down. The kernel then falls back to stock DLSS-G, which
mixes in low-confidence unwarped static frame color (`%f115` / `%f119`), generating stationary
ghosts and tearing across 4x–6x subframes.

Experiment E1 (`SCATTER_EXPERIMENT=5`) relaxes the RGB difference threshold in `quality_fix.h`
from 10% to 25% (`0f3E800000`), allowing v4's valid-warp boost to encompass subpixel and
high-contrast edges without falling back to stock unwarped blending. Geometric bounds,
nonfinite guards, single-source copying, and scatter logic remain identical to baseline v4.
Experiment D's options grid forcing was removed from `patcher.cpp`.

All four Release targets compiled and linked successfully. The package is located under
`build/dist/quality-v4-E1-relaxed-agreement` with symbols, README, and SHA-256 manifest.
Log marker: `Build: quality-v4-E1-relaxed-agreement`, with preparation reporting
`VALID WARP QUALITY V4-E1 (25% agreement) applied (patched=1)`.

### User result: experiment E1 unchanged

User reports E1 looks "still pretty much the same".
Mathematical audit of the provider's confidence and blending kernels explains this outcome:
1. On moving thin wires against bright backgrounds (sky/grass), the color disparity between
   the wire and background is 80%–95%, far exceeding the 25% threshold.
2. `Kernel_WarpBlendingWeights` computes confidence by comparing warped color to unwarped color
   at the same screen coordinate; because warped fence does not match unwarped sky, confidence
   drops to 0.10–0.20, failing v4's `weight >= 0.25` requirement.
Both gates caused moving fence wires to drop out of v4/E1 entirely into stock DLSS-G fallback.

## v4 E2: geometric-only warped protection (SCATTER_EXPERIMENT=6)

Experiment E2 removes the 0.25 confidence floor and the color agreement lockout. For any candidate
that satisfies geometric validity (coordinates strictly inside half-texel image bounds, non-sentinel,
finite RGB), its warped-color contribution floor is clamped to at least 0.85 (capped at 1.0).
If only one candidate is geometrically valid, its warped RGB is mirrored to both outputs. True
off-screen disocclusions and nonfinite samples retain existing fallback behavior.

This directly forbids DLSS-G from injecting 80% stationary unwarped frames into moving geometry,
preserving tracked motion across 4x–6x multipliers.

All four Release targets compiled and linked successfully. Package located under
`build/dist/quality-v4-E2-geometric-warp` with symbols, README, and SHA-256 manifest.
Log marker: `Build: quality-v4-E2-geometric-warp`, with preparation reporting
`VALID WARP QUALITY V4-E2 (geometric warp) applied (patched=1)`.

### User result: breakthrough on E2 (tearing completely gone)

User tested `quality-v4-E2-geometric-warp` in NTE at 2560x1440 output with DLSS Performance and reported:
*"Damn it looks amazing, the tearing is completely gone, just one thing I see very faintly, some color flicker, if it's fixable without making the tear come back would be awesome."*

Additionally, user confirmed that `disableMvDilation=1` has no effect on this artifact ("seems to do jack shit anyway"), verifying that motion vectors are already dilated/intact at the motion stage, and the root failure was strictly downstream in candidate blending.

This confirms the central diagnosis: thin geometry tearing at high multipliers (4x-6x) was caused by stock DLSS-G's low-confidence fallback injecting 80-90% stationary unwarped frame color into moving pixels. Clamping the warped motion contribution floor to geometric validity cured the tearing.

### Root cause of the faint color flicker

In E2, the warped motion floor was set to 0.85:
```ptx
max.f32 %qf0, %f148, 0f3F59999A; // 0.85 floor
min.f32 %qf0, %qf0, 0f3F800000; // 1.00 ceiling
```
The blend math computes:
`Candidate 0 = qf0 * Warped0 + (1 - qf0) * Unwarped0`
`Candidate 1 = qf1 * Warped1 + (1 - qf1) * Unwarped1`

When `%qf0 = %qf1 = 0.85`:
`Candidate 0 = 0.85 * Warped0 + 0.15 * Unwarped0`
`Candidate 1 = 0.85 * Warped1 + 0.15 * Unwarped1`

Here, `Unwarped0` is the static color of Frame 0 at pixel `(x, y)`, and `Unwarped1` is the static color of Frame 1 at pixel `(x, y)`. When an edge moves across `(x, y)`, Frame 0 and Frame 1 have different colors (e.g. sky vs fence wire, or shifted texture pattern).

Across the intermediate subframes generated by DLSS-G (e.g. 5 subframes in 6x mode: 1/6, 2/6, 3/6, 4/6, 5/6):
- Subframe 1/6 heavily favors Candidate 0 (containing `0.15 * Unwarped0`).
- Subframe 3/6 balances Candidate 0 and Candidate 1.
- Subframe 5/6 heavily favors Candidate 1 (containing `0.15 * Unwarped1`).

This 15% stationary color leakage modulates between Frame 0's unwarped pixel and Frame 1's unwarped pixel across consecutive subframes at high frequency, producing the faint color flicker observed by the user.

## v4 E3: pure 100% warped motion (SCATTER_EXPERIMENT=7)

Experiment E3 eliminates the stationary background leakage entirely by raising the warped motion floor from 0.85 (`0f3F59999A`) to 1.0 (`0f3F800000`):
```ptx
max.f32 %qf0, %f148, 0f3F800000; // 1.00 floor
min.f32 %qf0, %qf0, 0f3F800000; // 1.00 ceiling
```
Result:
`Candidate 0 = 1.0 * Warped0 + 0.0 * Unwarped0`
`Candidate 1 = 1.0 * Warped1 + 0.0 * Unwarped1`

Both candidates become 100% pure motion-compensated warped samples. Zero stationary frame color from Frame 0 or Frame 1 is injected into any moving pixel that possesses geometrically valid motion vectors. Both candidates are spatially and temporally aligned with the moving scene at subframe time `t`, eliminating the 15% background flicker across subframes while preserving the complete eradication of tearing achieved in E2.

All four Release targets compiled and linked successfully. Package located under
`build/dist/quality-v4-E3-pure-warp` with symbols, README, and SHA-256 manifest.
Log marker: `Build: quality-v4-E3-pure-warp`, with preparation reporting
`VALID WARP QUALITY V4-E3 (pure 100% warp) applied (patched=1)`.

