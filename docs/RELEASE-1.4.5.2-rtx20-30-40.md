# v1.4.5.2-rtx20-30-40

Hotfix for D3D12 `uiAssist` when a game tags its HUD-less image before its own
UI layer in one Streamline tag batch. The plugin now checks the entire batch
before deciding whether it needs to make a HUD-less copy. In the reported
Starfield case, the unnecessary D3D12 resource transition was where the crash
occurred.

The affected user confirmed that their save loads and frame generation works
with `uiAssist=true`. A separate Cyberpunk 2077 test found no regression. GPU
regression tests cover both game UI tag types. These tests do not guarantee
every game or mod combination.

Optional HUD/UI copy tracing is available as `diagnostics.logHudUi` in the
JSON or **Log HUD/UI** in the ReShade panel. It is off by default, can be
changed live, and logs only the first three game HUD-less copies.

Thanks to [**jay33721**](https://github.com/jay33721) for helping us identify
the Starfield UI-assist error.

All v1.4.5.1 features remain, including experimental Smooth Motion on RTX 30,
subject to its exact-driver checks and game-specific limitations. See the
[previous release notes](RELEASE-1.4.5.1-rtx20-30-40.md).

The attached binaries are built from the private source tree, which includes
the GPU kernels omitted from the public source export. The public repository
contains the rest of the code, including this fix and its test.

## Français

Ce correctif évite le crash D3D12 de `uiAssist` lorsqu'un jeu transmet son
image sans HUD avant sa propre couche UI dans le même lot de tags Streamline.
Le plugin examine désormais le lot entier avant de décider de copier l'image.

L'utilisateur concerné confirme que sa sauvegarde Starfield charge et que la
génération d'images fonctionne avec `uiAssist=true`. Un test séparé sur
Cyberpunk 2077 n'a montré aucune régression. Cela ne garantit pas la
compatibilité de toutes les installations ou combinaisons de mods.
