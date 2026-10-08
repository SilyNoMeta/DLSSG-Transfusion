# Neural Rendering

*[Français](NEURAL-RENDERING.fr.md) · [中文](NEURAL-RENDERING.zh-CN.md)*

DLSS Neural Rendering (NR) enhances the image made by DLSS Super Resolution or DLAA. RTX Encore runs it on RTX 20,
RTX 30 and RTX 40 cards, in DirectX 11, DirectX 12 and Vulkan games.

> [!NOTE]
> Neural Rendering is experimental and off by default. It costs a lot of GPU time: see [Cost](#cost).

Each feature has its own job: **DLSS Super Resolution** reconstructs the game's image, **Neural Rendering** changes
how it looks, **frame generation** adds frames in between. Turning NR on does not turn frame generation on.

## What RTX Encore brings to it

RTX Encore develops its own NR performance options for **Ampere (RTX 30)**, alongside support for RTX 40 and
experimental RTX 20 support. These cover both the NVIDIA engine and our integration of OpenDLSS-NR: faster
processing, control over the NR working resolution, and an **ultra-fast mode** that reduced NR time by **48 %** in
the Wukong comparison below.

| | |
| :--- | :--- |
| **Three generations of cards** | RTX 40, RTX 30 and, experimentally, RTX 20, in DirectX 11, DirectX 12 and Vulkan games. |
| **Faster processing, same image** | Optimizations enabled by default for the NVIDIA engine on RTX 20 and RTX 30, with identical output in the local reference tests. Performance measured on RTX 30; physical RTX 20 validation is still pending. |
| **Fast precision** | NVIDIA engine: about 17 % less NR processing time measured on RTX 30, with small image differences. Open also has its own Fast precision for RTX 30, off by default. |
| **Independent NR resolution** | NR can work below the screen resolution while DLSS keeps its own render resolution and reconstructed detail. Performance and Ultra Performance reduce the amount of NR work without requiring a lower DLSS setting. |
| **More Open performance controls on RTX 30** | **More speed for more VRAM** reduces NR time with the same image, at the cost of VRAM; **Fast projection** favors speed with small image differences. See [Performance controls](#performance-controls-on-rtx-30). |
| **Lower Open memory use** | About 41–43 % less memory allocated by the Open NR engine in local tests of its RTX 30 path, with the same output. See [Memory use](#open-memory-use). |
| **Ultra-fast mode** | RTX Encore's option for the Open engine: 18.7 → 9.8 ms of NR time and 73.8 → 94.9 displayed FPS in the Wukong comparison. See [Ultra-fast mode](#ultra-fast-mode). |
| **Built to live with frame generation** | NR runs once per rendered frame, never on generated frames. |
| **Several passes, a style for each** | One to four passes, each with its own look. |
| **HDR games** | Handled, with a brightness control. |
| **Background preparation** | When NR is preparing or required game inputs are unavailable, the image stays as DLSS made it. |

Options that use more VRAM are off by default.

## Requirements

- NVIDIA's NR file, **`nvngx_dlssnr.dll` version 310.8.0**. It is not included in RTX Encore: supply it
  yourself and put it beside the mod's file, normally in the folder of the game's executable. Any other version is
  refused.
- A game with **DLSS Super Resolution or DLAA turned on**. NR is applied to its result.
- An RTX 40 or RTX 30 card. RTX 20 is experimental.

## Turn it on

1. Put `nvngx_dlssnr.dll` beside the mod's file.
2. Turn DLSS or DLAA on in the game.
3. Open the menu (**Insert**), **Image** tab, and turn **Neural Rendering** on.

NR gets ready in the background: the first frames can stay as DLSS made them. The **Neural Rendering** chip at the
top of the menu shows whether NR is really running, and why not when it is not.

## Main settings

| Setting | Default | What it does |
| :--- | :---: | :--- |
| **Resolution** | Render | The size NR works at: the DLSS render size, the full output size, or Quality (67 %), Balanced (58 %), Performance (50 %), Ultra Performance (33 %) of the output, or a custom scale. **This is the main performance lever**: smaller is faster. |
| **Strength** | 100 % | How strong the effect is, from 0 to 200 %. |
| **Style** | Default | The look of the image: Default, Natural or Cinematic. |
| **Passes** | 1 | NR can be applied again to its own result, up to four times. Each pass costs GPU time and VRAM, and more passes do not guarantee a better image. |
| **Precision** | Exact | NVIDIA engine, RTX 20 and RTX 30: *Fast* reduces NR time, with about 17 % measured on RTX 30 and small image differences. Open has a separate Exact/Fast choice for RTX 30. Restart the game to apply. |

### Performance controls on RTX 30

The **Image → Neural Rendering → Performance** group exposes the work RTX Encore has done to reduce NR cost.
These options have different tradeoffs; their gains are not percentages you can add together.

| Control | Engine | Default | Benefit and tradeoff |
| :--- | :--- | :---: | :--- |
| **Precision: Fast** | NVIDIA or Open on RTX 30 | Exact | Faster processing with a slightly different image. The 17 % measurement above applies to the NVIDIA engine, not to Open. Restart to apply. |
| **More speed for more VRAM** | Open on RTX 30 | Off | Shorter NR time, same image, for about 160 MB more VRAM at 1440p with DLSS Balanced. Leave off when VRAM is nearly full. Restart to apply. |
| **Fast projection** | Open on RTX 30 | On | Faster processing with small image differences. Restart to apply. |
| **Avoid NR padding** | NVIDIA or Open | Off | Allows a nearby smaller NR size, at most 2 % less per axis. Can change the NR image; does not change the DLSS render size. Applies live. |
| **NR frames in flight** | NVIDIA, DirectX 12 with Reflex | 0 | Limits outstanding NR frames to 1 or 2; active frame generation uses at least 2. Applies live; its in-game performance effect remains unverified. |
| **Ultra-fast mode** | Open, one NR pass | Off | About half the NR processing time in the comparison below, with an older NR layer and possible motion artifacts. Restart to apply. |

For a performance comparison, keep one pass and the same scene, output resolution and frame-generation multiplier.
First lower **NR Resolution** to Performance or Ultra Performance while keeping DLSS at your preferred setting.
Then compare Fast precision. To try the ultra-fast mode, choose Open and the backend marked **RTX 30**, enable
**Ultra-fast mode**, and restart. Change one option at a time and check moving characters and newly visible
surfaces as well as FPS.

**Ultra Performance** is the NR resolution preset (33 % of output width and height). **Ultra-fast mode** is
an option of the Open engine. They can be used together, but choosing one does not enable the other.

### Passes and styles

With more than one pass, each pass can have its own style: **Global** (follows the **Style** setting), **Default**,
**Natural** or **Cinematic**. For example, Style Cinematic with pass one set to Natural and pass two to Global gives
Natural, then Cinematic. **Tone after pass one** (default 0) avoids applying the tone correction again at each pass.

Start with one pass and keep it as your reference when comparing. Several passes have not been validated in games
yet.

### Fine tuning

Local tone, local structure, a character mask with its own skin structure setting, the brightness given to NR in
HDR games, and the NVIDIA model profile (**Automatic** is recommended) are in the groups that open under the main
settings. Each has a description in the menu.

## Engines

| Engine | Status | Notes |
| :--- | :--- | :--- |
| **NVIDIA** (default) | Experimental | Runs NVIDIA's NR. |
| **Open** | Highly experimental | An open implementation of NR ([OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR)). Expect crashes, flicker, artifacts and incompatibilities. It still needs `nvngx_dlssnr.dll` beside the mod's file. |

Changing the engine needs a restart, and there is no automatic switch from one engine to the other. With **Open**,
the menu asks you to choose the backend for your card and how it runs; its options are described there. Open does
not run on RTX 20. Its RTX 30 path and performance options are part of RTX Encore's Ampere work; it is not
guaranteed to produce the same image as the NVIDIA engine.

## Cost

NR adds GPU work to every rendered frame. Measured in the built-in benchmark of Black Myth: Wukong on an RTX 3070 Ti
Laptop (2560×1440, DLSS at 58 %, frame generation X3):

| Configuration | Average FPS | 5 % low |
| :--- | ---: | ---: |
| NR off | 146 | 127 |
| NR on, Precision Exact | 76 | 71 |
| NR on, Precision Fast | 82 | 76 |
| NR on, Precision Fast, Resolution Performance | 90 | 83 |

These are **displayed FPS with frame generation X3**, using the NVIDIA NR engine. In this test, going from the
default to Precision Fast and Resolution Performance raises the displayed frame rate by 18 % (76 to 90 FPS).
This does not measure a 17 % increase in game FPS: that figure concerns NR processing time. With frame generation,
NR runs on the frames the game renders, never on the generated ones.

Two experimental options go further, in the **Performance** group: **Avoid NR padding** and, for DirectX 12 games,
**NR frames in flight**. Each is described in the menu. The largest gain is the ultra-fast mode below.

## Ultra-fast mode

**Ultra-fast mode** is an option RTX Encore adds to the Open engine. It spreads NR work across two
rendered frames and keeps the NR layer following the scene between updates. This reduces NR work per frame with
a different temporal image, rather than simply selecting a lower resolution.

Measured in the built-in benchmark of Black Myth: Wukong on an **RTX 3070 Ti Laptop (Ampere)**, with frame generation
X3 (**one run per configuration**):

| | Open engine | Open engine, ultra-fast |
| :--- | ---: | ---: |
| NR time per rendered frame | 18.7 ms | 9.8 ms |
| Displayed frame rate | 73.8 FPS | 94.9 FPS |

That is **48 % less NR time** and **29 % more displayed FPS** in this comparison. The mode spreads the work to
avoid alternating a full-cost NR frame with a frame doing no NR work. Startup and history resets can still cost
more, and this result does not guarantee a gain or smooth frame times in every game. These existing measurements
are not a benchmark of a new release build; an exact build identifier is not recorded on this page.

- **Turn it on**: **Image** tab, engine **Open**, then **Ultra-fast mode** in the **Performance** group.
  Restart the game. It works with one NR pass, in DirectX 11, DirectX 12 and Vulkan games, and was seen in game in
  Black Myth: Wukong, Palworld and Shadows of Doubt.
- **What changes in the image**: the NR layer follows the image with one frame of delay. In fast motion it can lag
  or smear, and a surface that has just appeared receives its NR a moment later.
- **Two controls tune it live**: **Ghost tolerance** (lower: fewer ghosts behind what moves, more surface left
  waiting for NR) and **Fill tolerance** (how readily a surface that has just appeared borrows the NR of similar
  surfaces around it; 0 turns the filling off).
- It uses a few MB of VRAM more.

The mode belongs to the Open engine, which remains highly experimental: see [Engines](#engines).

## Open memory use

Our Ampere work also reduces Open's memory footprint. Existing local automated tests of the RTX 30 path, with
**More speed for more VRAM** off, recorded these GPU allocations belonging to the NR engine:

| NR working size | Before | After |
| :--- | ---: | ---: |
| 1472×828 | 601 MiB | 345 MiB |
| 1920×1080 | 846 MiB | 501 MiB |

That is about **41–43 % less NR memory**, with identical output in the reference comparisons. These figures count
the Open engine's allocations, not total game VRAM, and are local automated results rather than an in-game FPS
measurement. They do not describe memory use of the NVIDIA engine or of frame generation. Enabling **More speed for more VRAM**
adds VRAM again in exchange for less NR time.

## When something does not work

| Symptom | First check |
| :--- | :--- |
| The switch cannot be turned on | `nvngx_dlssnr.dll` is missing beside the mod's file, or is not version 310.8.0. |
| The chip says it is waiting for DLSS | Turn DLSS or DLAA on in the game and load a 3D scene. |
| The first frames have no NR | NR is getting ready in the background; this is expected. |
| The image stays as DLSS made it | Read the reason shown in the menu: when the game does not give NR what it needs, the image is left untouched. |
| Settings revert at once | The settings file could not be saved: check that it is not read-only or locked by an editor. |
| The frame rate drops | NR is costly. Lower **Resolution**, and compare in the same scene. |

When you report a problem, include the game and its graphics API, the engine chosen, and the menu's report
(**System** tab, **Copy**).
