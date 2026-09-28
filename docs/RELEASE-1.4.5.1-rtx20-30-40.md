# v1.4.5.1-rtx20-30-40

This build adds an **experimental, opt-in** NVIDIA driver Smooth Motion path on
RTX 30 (SM86). It does not add Smooth Motion support for RTX 20/Turing or
retarget FP8 kernels. The normal DLSS-G features remain separate.

## Verified scope

- NVIDIA driver 617.14 only. The proxy verifies the exact `NvPresent64.dll`
  binary and its expected code layout. The Vulkan profile bypass additionally
  verifies `nvoglv64.dll` from the same DriverStore package. An unknown build
  is refused; do not try to bypass these checks.
- D3D12: Manor Lords on an RTX 3070 Ti Laptop, 60 FPS in-game cap and 120 FPS
  observed in FrameView.
- D3D11: Shadows of Doubt on the same GPU, 60 FPS cap and 120 FPS observed in
  FrameView.
- Vulkan: Enshrouded with the NVIDIA game profile set to Off. The user
  reported working Smooth Motion; logs showed four successful
  `NVP_Init_Vulkan` calls and 19 FP16 CUDA kernels. The exact final FrameView
  FPS was not recorded.

These observations are not a compatibility, stability, performance or visual
quality guarantee for other games. Avoid multiplayer games with anti-cheat.
In particular, **do not force Smooth Motion in No Man's Sky**: adding the
Vulkan bit to its NVIDIA profile makes the feature run but the user observed
a tripled mouse cursor. Its default profile excludes Vulkan Smooth Motion.

Known issue under investigation: one Starfield user reported a D3D12 crash
after enabling DLSS-G while `uiAssist` was active. The same path did not crash
in a second Starfield installation. If affected, set `"uiAssist": false` in
`DLSSG-Transfusion.json` and restart. The exact cause is not established; do
not attribute it to another mod without an isolated reproduction.

## Install and test

1. Back up the game's existing proxy DLL and configuration. Put **one** proxy
   beside the game executable, using `DLSSG-Transfusion.dll` renamed to the
   proxy filename the game loads. Exact-export fallbacks are under
   `alternative-proxies/`; do not copy several proxy variants at once.
2. Copy `DLSSG-Transfusion.json` beside it. In `compatibility`, set
   `"smoothMotionSm86": true` and set `"smoothMotionSm86Api"` to `"d3d12"`,
   `"d3d11"` or `"vulkan"` to match the graphics API actually selected by
   the game, not merely the game engine. If unsure, check the game's **API**
   section on [PCGamingWiki](https://www.pcgamingwiki.com/), then verify its
   in-game graphics settings or startup log. Restart the game.
   Smooth Motion is **false** by default.
3. For the tested setups: Manor Lords used a D3D12 proxy, Shadows of Doubt
   used `dxgi.dll`, and Enshrouded used `dinput8.dll`. See the detailed
   [Smooth Motion notes](SMOOTH-MOTION-SM86.md) for placement and log checks.
4. Compare identical scenes with the JSON option Off/On and FrameView. A log
   saying the backend is armed is not, by itself, proof of generated frames.
   Keep the backup for a quick rollback.

The optional `DLSSG-Transfusion.addon64` settings panel requires ReShade 6.8+
with full add-on support. It must come from this same build. It is not needed
for Smooth Motion itself.

## Résumé français

Smooth Motion sur RTX 30 est expérimental et **désactivé par défaut**. Il faut
activer `smoothMotionSm86` dans le JSON et choisir l'API réelle du jeu avec
`smoothMotionSm86Api`. Le pilote NVIDIA 617.14 analysé est le seul accepté ;
en Vulkan, les deux DLL du pilote doivent correspondre au paquet vérifié.
Manor Lords (D3D12) et Shadows of Doubt (D3D11) ont montré 120 FPS pour un cap
de 60 FPS dans FrameView. Enshrouded (Vulkan) a fonctionné profil NVIDIA Off,
mais son FPS FrameView final n'a pas été relevé. RTX 20/Turing et FP8 restent
exclus. Sauvegardez la DLL et le JSON du jeu avant l'essai ; n'utilisez pas ce
prototype avec un anti-cheat.
Ne forcez pas Smooth Motion dans No Man's Sky : l'ajout du bit Vulkan au
profil NVIDIA active la fonction, mais le curseur se triple en mouvement.
Problème connu en cours d'analyse : un utilisateur de Starfield a signalé un
crash D3D12 avec `uiAssist` actif. Il n'a pas été reproduit sur une seconde
installation. Si nécessaire, réglez `"uiAssist": false` et relancez le jeu ;
la cause exacte reste inconnue.
