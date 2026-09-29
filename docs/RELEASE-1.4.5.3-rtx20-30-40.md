# v1.4.5.3-rtx20-30-40

Redesign of the optional ReShade settings panel. The engine (`DLSSG-Transfusion.dll`
and the alternative proxies) is functionally the same as v1.4.5.2; only
`DLSSG-Transfusion.addon64` changes.

- New look for the **DLSSG-Transfusion** tab: themed panel, toggle switches,
  sliders, segmented buttons, a status card and collapsible sections.
- A reset button appears on a setting that differs from its default, and each
  section has **Reset section**. Defaults match the engine's.
- A badge marks settings that need a game restart, with a card counting the
  pending changes.
- **Compatibility** now offers the experimental Smooth Motion switch for RTX 30
  and the graphics-API choice (`smoothMotionSm86`, `smoothMotionSm86Api`). The
  driver limits from [v1.4.5.1](RELEASE-1.4.5.1-rtx20-30-40.md) still apply.
- **Disable keyboard shortcuts** is no longer in the panel; the
  `disableKeybinds` key in the JSON is unchanged.

Testing: the add-on is built in Release and its configuration unit test
passes. The layout was reviewed in-game by the maintainer in one D3D12 game;
other ReShade font sizes, panel widths and games have not been checked.

The attached binaries are built from the private source tree, which includes
the GPU kernels omitted from the public source export. The public repository
contains the rest of the code, including this panel.

## Français

Refonte du panneau de réglages ReShade (optionnel). Le moteur
(`DLSSG-Transfusion.dll` et les proxies alternatifs) est fonctionnellement
identique à la v1.4.5.2 ; seul `DLSSG-Transfusion.addon64` change.

- Nouvel onglet **DLSSG-Transfusion** : thème sombre, interrupteurs, curseurs,
  boutons segmentés, carte de statut et sections repliables.
- Un bouton de réinitialisation apparaît sur un réglage différent de son
  défaut, et chaque section a **Reset section**. Les valeurs par défaut sont
  celles du moteur.
- Un badge signale les réglages qui demandent de redémarrer le jeu.
- **Compatibility** propose l'interrupteur Smooth Motion expérimental pour
  RTX 30 et le choix de l'API graphique (`smoothMotionSm86`,
  `smoothMotionSm86Api`), avec les mêmes limites de pilote qu'en v1.4.5.1.
- **Disable keyboard shortcuts** n'est plus dans le panneau ; la clé
  `disableKeybinds` du JSON reste inchangée.

Tests : l'add-on est compilé en Release et son test unitaire de configuration
passe. La mise en page a été relue en jeu par le mainteneur dans un seul jeu
D3D12 ; les autres tailles de police, largeurs de panneau et jeux n'ont pas
été vérifiés.
