# UI assist: HUD-less capture and UI layer synthesis (D3D12)

Since v1.4.5.2-rtx20-30-40, UI assist checks every tag in a game's
`slSetTag` batch before copying the HUD-less image. If the batch contains a
game-provided UI layer, it skips that unnecessary copy. This fixed the
reported Starfield save-load crash with `uiAssist=true` without changing the
game's own UI tags.

*Français : [HUD-ASSIST.fr.md](HUD-ASSIST.fr.md)*

Branch `feat/sm86-75-hud-alpha`, merged into `feat/sm86-75`. Port of dlssg_for_sm86's work (`src/companion/hudless_capture.hpp`, MIT license, `feat/0.3.5` at commit `2bff790`). Original guide: that project's `docs/GUIDE-HUD-ALPHA.fr.md`.

## What it brings to Transfusion

Transfusion already switches to **Preset B** (UI Recomposition) when the game tags a HUD-less image, with or without a UI layer. Tony wrote the UIR patches and options, and `CaptureUiResourceTags` decides when to engage. Two cases stayed in **Preset A** or degraded:

| Situation | Example | UI assist |
|---|---|---|
| HUD-less only, no alpha | Cyberpunk 2077 | DLSS-G extracts the UI as `final − HUD-less`; behind translucent UI the background is doubled. We **synthesize the UI layer and its alpha**. |
| Nothing tagged for DLSS-G (tags exist only for FSR or XeFG) | Crimson Desert | We **capture the HUD-less scene** between scene composition and UI drawing, then synthesize the UI layer. |

Our tags go through the real `slSetTag`, then through `CaptureUiResourceTags`. Tony's UIR logic therefore engages Preset B, with no parallel path.

The mechanism does not depend on the architecture: it benefits Ada, Ampere and Turing.

## How it works (summary; details in the `hud_assist.h` header)

- **Installation**: D3D12 hooks placed on the runtime's implementations (vtables of throwaway objects), from a dedicated thread, once. Installation happens after 300 DLSS-G tag batches containing depth, about 5 s.
- **Hooks**: `Close`, `DrawInstanced`, `DrawIndexedInstanced`, `CopyTextureRegion`, `CopyResource`, `ResourceBarrier`, `OMSetRenderTargets`, `Barrier` (enhanced barriers), `ExecuteCommandLists`.
- **Capture**: on one command list, a full-size texture becomes readable, then Streamline's fake swapchain buffer goes to RENDER_TARGET. One or two full-screen draws follow, then a render-target change. The buffer is then copied, with the game's own barrier model, and tagged `HUDLessColor` (`eOnlyValidNow`).
- **Fallback**: if a scene was captured in the last 5 seconds, a frame that does not match the rule tags the finished image instead. HUD-less equal to final means "no UI".
- **Synthesis**: the final image is identified by name, or learned through Streamline's "clone" copy at Present. After the game's list executes, a compute shader runs on **our** list: `alpha = f(|final − HUD-less|, 3×3)` and `UI = final − (1 − alpha)·HUD-less`. The result is tagged `UIColorAndAlpha`. Real frames are reconstructed exactly.
- **Pause** above 40 % coverage (full-screen effects after the UI), resume below 30 %.
- **The game has priority**: if the game tags its own HUD-less image, capture stays off (we copy the game's image for the synthesis). If the game tags its own UI layer (**`UIColorAndAlpha` or `UIAlpha`**), synthesis stays off. 2 s hold.

### Fix compared to the original

dlssg_for_sm86 treated `kBufferTypeAlpha` (34) as a game UI layer instead of `kBufferTypeUIAlpha` (69), so a game providing a UIAlpha got its layer doubled by the synthesis. It is fixed here, with a dedicated test case, and was reported back to dlssg_for_sm86 (`feat/0.3.5`, commit `a2305e1`).

## Setting

```json
"uiAssist": true   // D3D12: HUD-less capture and UI layer when the game does not tag them
```

On by default, like `UIRecomposition=1` in dlssg_for_sm86. It stays inactive on Vulkan, with games that tag through `slSetTagForFrame`, and when the game provides these inputs itself. Check with NVIDIA's indicator (`DLSSG_IndicatorText=2` in `HKLM\SOFTWARE\NVIDIA Corporation\Global\NGXCore`): it should show "Hudless: Yes · UIAlpha: Yes · UIR: ON".

For copy/barrier diagnosis, set `diagnostics.logHudUi` to `true` in the JSON
or use **Log HUD/UI** in the ReShade panel. It applies live, traces only the
first three game HUD-less copies, and is off by default.

## Log codes (`DLSSG-Transfusion.log`, "UI assist" prefix)

| Code | Meaning |
|---|---|
| A0 / A1 | The game tags its own HUD-less image / its own UI layer (capture / synthesis off) |
| A2 / A3 | D3D12 hooks installed / hooks receive barriers |
| B1 / B2 | Streamline buffer moved by enhanced / legacy barriers |
| B3 | Buffer in RENDER_TARGET, capture armed |
| B4 | Draws not matching the capture rule |
| C1 `WxH fmt` | Copy texture created |
| C2 | First scene captured |
| C3 | Fallback to the finished image |
| D1 / D2 | Final image observed / learned through Streamline's copy |
| D3 | First synthesized UI layer tagged |
| D4 `n%` / D5 | Pause / resume |
| E1 / E3 | Streamline refuses the HUD-less / UI tag |
| E2 | GPU objects unavailable |
| E4 / E5 | Installation failure |

## Validation (RTX 3070 Ti Laptop)

| Test | Result |
|---|---|
| `tests/hud_assist`: D3D12 GPU test ported from dlssg_for_sm86, on a real device. It replays Crimson Desert's frame structure: capture, 1 or 2 draws, fallbacks, non-presentable buffer, learning through the clone, game priority (including the **new UIAlpha case**), disallowed mode, pause and resume, ANSI name, enhanced barriers, healthy device at the end | **All passed** |
| NVIDIA runtime, Ampere X2 1080p | Still 5/5 identical to NVIDIA |
| Tony's control harness | Unchanged (pre-existing dynamic failure) |

### Not validated

- **In game within Transfusion.** In dlssg_for_sm86, the same logic was validated on Crimson Desert and Cyberpunk 2077. Only the integration layer changes here: tag submission, engagement and logging.
- Vulkan (not implemented), and HDR thresholds not calibrated in nits.
