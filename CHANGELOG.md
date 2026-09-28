# DLSSG-Transfusion v1.4.5.0-rtx20-30-40

Fork of [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) v1.4.5 for **RTX 20, 30 and 40**. Tony's patches on NVIDIA's runtime remain the engine; everything from v1.4.5 is included. This first public release gathers the private builds v1.4.5.1 to v1.4.5.3-rtx2030 and the work done since (Vulkan).

## English

### GPUs
- 🟢 **RTX 30 (Ampere) support.** The architecture gates are lowered to the GPU actually present and the Blackwell kernels are retargeted to `sm_86`. Detected automatically; `"gpuArchitecture"` (`auto`, `ada`, `ampere`, `turing`) overrides it.
- 🧪 **RTX 20 (Turing) support, experimental.** The network runs from PTX and Ampere-only instructions are rewritten for `sm_75`. Validated by emulation on an RTX 30 only, never on a real RTX 20. Requires DLSS-G 310.9.x.

### Image quality
- 🌿 **`explained-warp`, our valid-warp policy, is the default.** Tony's valid warp decides when to trust the motion vectors instead of NVIDIA's blend; his test asks whether the two warped candidates agree. Ours asks whether **the warp explains what changed** between the two frames. Result: fine detail and clean moving shadows at the same time. In a blind comparison in Cyberpunk 2077, lit patches inside moving shadows drop from 2.1 to 0.39 (generated/real ratio) with more detail on fences. `"qualityPolicy": "transfusion"` keeps Tony's policy. See [QUALITY-POLICY.md](docs/QUALITY-POLICY.md).
- 🧭 **UI assist** (`"uiAssist": true`, DX12). When a game does not give DLSS-G its HUD-less scene or UI layer, they are captured and synthesized: more games reach Preset B and translucent HUD elements stay clean. The game's own tags always win. See [HUD-ASSIST.md](docs/HUD-ASSIST.md).
- 🛡️ **UI recomposition can follow the game.** It is still turned on automatically when a game tags HUD-less and UI buffers; `"autoUiRecomposition": false` leaves the choice to the game. DOOM: The Dark Ages tags them without asking for recomposition, and forcing it warped every generated frame: this game now follows its own choice automatically.

### Performance
- ⚡ **Faster frame generation, identical image** (`"optimizedKernels": true`). Specialized, bit-exact kernels for the image passes and both neural networks, with launch fusions. RTX 3070 Ti Laptop at 1080p: **2.33 → 1.57 ms at 2x (−33 %)**, **6.38 → 5.23 ms at 6x (−18 %)**. Requires DLSS-G 310.9.1. See [OPTIMIZED-KERNELS.md](docs/OPTIMIZED-KERNELS.md).

### Vulkan (new)
- 🌋 **Our kernels in Vulkan.** The engine now routes the runtime's kernels through `VK_NVX_binary_import`, so the RTX 30/20 rewrite and the optimized kernels also apply to Vulkan games. Validated on an RTX 4090 in No Man's Sky and DOOM: The Dark Ages (0 module refused). **Not yet tested on RTX 30/20 in Vulkan.**
- 🎚️ **Dynamic mode in Vulkan.** Streamline has no Dynamic MFG in Vulkan: an adaptive controller picks X2 to X6 from the frame rate instead.
- 🖥️ **Overlay in Vulkan**, drawn by the ReShade add-on. The engine no longer draws on the Vulkan driver's own DXGI swapchain, which crashed DOOM: The Dark Ages when loading a save or changing the multiplier.
- See [VULKAN.md](docs/VULKAN.md).

### Controls
- 🎛️ **ReShade settings panel** (optional `DLSSG-Transfusion.addon64`, ReShade 6.8+ with add-on support): every setting in game, saved in the JSON. Replaces the CET panel.
- 🎮 **`"mode": "game"` by default**: the game or NVIDIA Profile Inspector picks the multiplier (`Ctrl + Alt + G`).
- ⌨️ **Remappable shortcuts** (JSON or panel), active only while the game has focus.
- 📈 **Dynamic MFG**: stays at 4x or less unless 5x/6x is allowed; a target of `0` follows the refresh rate of the monitor showing the game.
- 📊 **Optional overlay lines**: UIR, HUD-less / UI alpha sources, DLSS / DLSS-G / Streamline versions, frame pacing, GPU, VRAM, debug.
- 🔍 **DLSS render resolution** (`"dlssRenderScale"`: DLAA to Ultra Performance, or custom), with live `r.ScreenPercentage` control in Unreal Engine 4/5 games.
- 📝 **Readable JSON**, grouped like the panel; older files, Tony's included, are converted with their values kept.

### Fixes
- Cyberpunk 2077 startup crash: modules are pinned while the module scan inspects them.
- DOOM: The Dark Ages crash with the overlay shown (see Vulkan).

### Tested
Cyberpunk 2077 (RTX 3070 Ti Laptop, DX12) · Black Myth: Wukong (RTX 4090, DX12) · No Man's Sky and DOOM: The Dark Ages (RTX 4090, Vulkan). RTX 20: emulation only.

**Install:** copy `DLSSG-Transfusion.dll` (renamed to your proxy: `version.dll`, `dinput8.dll`, `dxgi.dll`, `winmm.dll` or `*.asi`) and `DLSSG-Transfusion.json` next to the game executable; optionally `DLSSG-Transfusion.addon64` next to ReShade. Engine and add-on must come from the same release. Existing JSON files are converted automatically.

The released binaries include every kernel; the public source leaves a few out, see [PUBLIC-SOURCE.md](docs/PUBLIC-SOURCE.md).

## Français

Fork de [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) v1.4.5 pour les **RTX 20, 30 et 40**. Les patchs de Tony sur le runtime NVIDIA restent le moteur ; tout le contenu de la v1.4.5 est inclus. Cette première release publique regroupe les versions privées v1.4.5.1 à v1.4.5.3-rtx2030 et le travail fait depuis (Vulkan).

### GPU
- 🟢 **Prise en charge des RTX 30 (Ampere).** Les verrous d'architecture sont abaissés au GPU réellement présent et les kernels Blackwell reciblés en `sm_86`. Détection automatique ; `"gpuArchitecture"` (`auto`, `ada`, `ampere`, `turing`) permet de forcer la cible.
- 🧪 **Prise en charge des RTX 20 (Turing), expérimentale.** Le réseau tourne depuis le PTX et les instructions propres à Ampere sont réécrites pour `sm_75`. Validée par émulation sur une RTX 30 uniquement, jamais sur une vraie RTX 20. Nécessite DLSS-G 310.9.x.

### Qualité d'image
- 🌿 **`explained-warp`, notre politique valid-warp, devient le défaut.** Le valid warp de Tony décide quand faire confiance aux vecteurs de mouvement plutôt qu'au mélange de NVIDIA ; son test vérifie que les deux candidats déplacés sont d'accord. Le nôtre vérifie que **le déplacement explique ce qui a changé** entre les deux images. Résultat : détail fin et ombres en mouvement propres à la fois. Dans une comparaison à l'aveugle dans Cyberpunk 2077, les plaques éclairées dans les ombres en mouvement passent de 2,1 à 0,39 (rapport générée/réelle), avec plus de détail sur les grillages. `"qualityPolicy": "transfusion"` conserve la politique de Tony. Voir [QUALITY-POLICY.fr.md](docs/QUALITY-POLICY.fr.md).
- 🧭 **Assistance HUD** (`"uiAssist": true`, DX12). Quand un jeu ne fournit pas à DLSS-G sa scène sans HUD ou sa couche d'interface, elles sont capturées et synthétisées : plus de jeux passent en Preset B et les éléments translucides restent nets. Les tags du jeu restent prioritaires. Voir [HUD-ASSIST.fr.md](docs/HUD-ASSIST.fr.md).
- 🛡️ **La recomposition de l'interface peut suivre le jeu.** Elle reste activée automatiquement quand un jeu tague des tampons sans HUD et d'interface ; `"autoUiRecomposition": false` laisse le choix au jeu. DOOM: The Dark Ages les tague sans demander la recomposition, et la forcer déformait chaque image générée : ce jeu suit désormais son propre choix automatiquement.

### Performances
- ⚡ **Génération d'images plus rapide, image identique** (`"optimizedKernels": true`). Kernels spécialisés, identiques bit à bit, pour les passes d'image et les deux réseaux, avec fusion de lancements. RTX 3070 Ti Laptop en 1080p : **2,33 → 1,57 ms en 2x (−33 %)**, **6,38 → 5,23 ms en 6x (−18 %)**. Nécessite DLSS-G 310.9.1. Voir [OPTIMIZED-KERNELS.fr.md](docs/OPTIMIZED-KERNELS.fr.md).

### Vulkan (nouveau)
- 🌋 **Nos kernels en Vulkan.** Le moteur fait passer les kernels du runtime par `VK_NVX_binary_import` : la réécriture RTX 30/20 et les kernels optimisés s'appliquent aussi aux jeux Vulkan. Validé sur RTX 4090 dans No Man's Sky et DOOM: The Dark Ages (aucun module refusé). **Pas encore testé en Vulkan sur RTX 30/20.**
- 🎚️ **Mode dynamique en Vulkan.** Streamline n'a pas de Dynamic MFG en Vulkan : un contrôleur adaptatif choisit X2 à X6 selon la fréquence d'images.
- 🖥️ **Overlay en Vulkan**, dessiné par l'add-on ReShade. Le moteur ne dessine plus sur la swapchain DXGI interne du pilote Vulkan, ce qui faisait planter DOOM: The Dark Ages au chargement d'une partie ou au changement de multiplicateur.
- Voir [VULKAN.fr.md](docs/VULKAN.fr.md).

### Contrôles
- 🎛️ **Panneau de réglages ReShade** (`DLSSG-Transfusion.addon64` facultatif, ReShade 6.8+ avec add-ons) : tous les réglages en jeu, enregistrés dans le JSON. Remplace le panneau CET.
- 🎮 **`"mode": "game"` par défaut** : le jeu ou NVIDIA Profile Inspector choisit le multiplicateur (`Ctrl + Alt + G`).
- ⌨️ **Raccourcis reconfigurables** (JSON ou panneau), actifs seulement quand le jeu a le focus.
- 📈 **Dynamic MFG** : reste à 4x maximum sauf si 5x/6x est autorisé ; une cible de `0` suit la fréquence de l'écran qui affiche le jeu.
- 📊 **Lignes d'overlay facultatives** : UIR, sources HUD-less / alpha UI, versions DLSS / DLSS-G / Streamline, rythme d'affichage, GPU, VRAM, debug.
- 🔍 **Résolution de rendu DLSS** (`"dlssRenderScale"` : DLAA à Ultra Performance, ou personnalisée), avec contrôle en direct de `r.ScreenPercentage` dans les jeux Unreal Engine 4/5.
- 📝 **JSON lisible**, groupé comme le panneau ; les anciens fichiers, ceux de Tony compris, sont convertis en gardant leurs valeurs.

### Corrections
- Crash au démarrage de Cyberpunk 2077 : les modules sont épinglés pendant leur inspection.
- Crash de DOOM: The Dark Ages avec l'overlay affiché (voir Vulkan).

### Testé
Cyberpunk 2077 (RTX 3070 Ti Laptop, DX12) · Black Myth: Wukong (RTX 4090, DX12) · No Man's Sky et DOOM: The Dark Ages (RTX 4090, Vulkan). RTX 20 : émulation uniquement.

**Installation :** copier `DLSSG-Transfusion.dll` (renommée selon le proxy : `version.dll`, `dinput8.dll`, `dxgi.dll`, `winmm.dll` ou `*.asi`) et `DLSSG-Transfusion.json` à côté de l'exécutable du jeu ; facultativement `DLSSG-Transfusion.addon64` à côté de ReShade. Le moteur et l'add-on doivent venir de la même release. Les JSON existants sont convertis automatiquement.

Les binaires publiés contiennent tous les kernels ; le code source public en omet quelques-uns, voir [PUBLIC-SOURCE.fr.md](docs/PUBLIC-SOURCE.fr.md).

## Credits

[TonyJoaca](https://github.com/TonyJoaca/DLSSG-Transfusion) · [sdli1995](https://github.com/sdli1995/dlssg_for_sm86) · [dashdogy](https://github.com/dashdogy/RTX40MFG-Unlock) · [mavismmg](https://github.com/mavismmg/MFGAdaUnlock-RenoDx)
