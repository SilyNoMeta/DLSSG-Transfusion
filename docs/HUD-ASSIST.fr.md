# UI assist : capture HUD-less et synthèse de la couche UI (D3D12)

*English: [HUD-ASSIST.md](HUD-ASSIST.md)*

Depuis v1.4.5.2-rtx20-30-40, UI assist examine tous les tags d'un lot
`slSetTag` avant de copier l'image sans HUD. Si le jeu fournit également sa
propre couche UI, cette copie inutile est évitée. Ce changement a corrigé le
crash signalé au chargement d'une sauvegarde Starfield avec `uiAssist=true`.

Pour diagnostiquer les copies et barrières, activez
`diagnostics.logHudUi` dans le JSON ou **Log HUD/UI** dans le panneau ReShade.
Le réglage s'applique sans redémarrage, trace seulement les trois premières
copies HUD-less du jeu et reste désactivé par défaut.

Branche `feat/sm86-75-hud-alpha`, rapatriée dans `feat/sm86-75`. Portage du travail de dlssg_for_sm86 (`src/companion/hudless_capture.hpp`, licence MIT, `feat/0.3.5` au commit `2bff790`). Guide d'origine : `docs/GUIDE-HUD-ALPHA.fr.md` de ce projet.

## Ce que ça apporte à Transfusion

Transfusion passe déjà en **Preset B** (UI Recomposition) quand le jeu tague une image sans HUD, avec ou sans couche UI. Tony a écrit les patchs UIR et les options, et `CaptureUiResourceTags` décide de l'activation. Deux cas restaient en **Preset A** ou dégradés :

| Situation | Exemple | UI assist |
|---|---|---|
| HUD-less seul, sans alpha | Cyberpunk 2077 | DLSS-G extrait l'UI par `final − HUD-less`. Derrière une UI translucide, le fond est alors dédoublé. On **synthétise la couche UI et son alpha**. |
| Rien n'est tagué pour DLSS-G (les tags existent seulement pour FSR ou XeFG) | Crimson Desert | On **capture le HUD-less** entre la composition de la scène et le dessin de l'UI, puis on synthétise la couche UI. |

Nos tags passent par le vrai `slSetTag`, puis par `CaptureUiResourceTags`. C'est donc la logique UIR de Tony qui déclenche le Preset B, sans chemin parallèle.

Ce mécanisme ne dépend pas de l'architecture : il profite à Ada, Ampere et Turing.

## Fonctionnement (résumé, détail dans l'en-tête de `hud_assist.h`)

- **Installation** : hooks D3D12 posés sur les implémentations du runtime (vtables d'objets jetables), depuis un thread dédié, une seule fois. L'installation a lieu après 300 lots de tags DLS-G contenant la profondeur, soit environ 5 s.
- **Hooks** : `Close`, `DrawInstanced`, `DrawIndexedInstanced`, `CopyTextureRegion`, `CopyResource`, `ResourceBarrier`, `OMSetRenderTargets`, `Barrier` (barrières enhanced), `ExecuteCommandLists`.
- **Capture** : sur une même command list, une texture pleine taille devient lisible, puis le faux buffer de swapchain de Streamline passe en RENDER_TARGET. Suivent 1 ou 2 draws plein écran, puis un changement de cible. On copie alors le buffer, avec le même modèle de barrière que le jeu, et on le tague `HUDLessColor` (`eOnlyValidNow`).
- **Repli** : si une scène a été capturée dans les 5 dernières secondes, une frame qui ne correspond pas à la règle tague l'image finie à la place. HUD-less égal à final signifie « pas d'UI ».
- **Synthèse** : l'image finale est repérée par son nom, ou apprise via la copie « clone » de Streamline au moment du Present. Après exécution de la list du jeu, un compute shader tourne sur **notre** list : `alpha = f(|final − HUD-less|, 3×3)` et `UI = final − (1 − alpha)·HUD-less`. Le résultat est tagué `UIColorAndAlpha`. Les images réelles sont reconstruites exactement.
- **Pause** au-delà de 40 % de couverture (effets plein écran après l'UI), reprise sous 30 %.
- **Priorité au jeu** : si le jeu tague son propre HUD-less, la capture reste inactive (on copie le sien pour la synthèse). Si le jeu tague sa propre couche UI (**`UIColorAndAlpha` ou `UIAlpha`**), la synthèse reste inactive. Maintien 2 s.

### Correction par rapport à l'original

dlssg_for_sm86 considérait `kBufferTypeAlpha` (34) comme une couche UI du jeu au lieu de `kBufferTypeUIAlpha` (69). Un jeu fournissant un UIAlpha voyait donc sa couche doublée par la synthèse. C'est corrigé ici, avec un cas de test dédié, et reporté dans dlssg_for_sm86 (`feat/0.3.5`, commit `a2305e1`).

## Réglage

```json
"uiAssist": true   // D3D12 : capture HUD-less et couche UI quand le jeu ne les tague pas
```

Actif par défaut, comme `UIRecomposition=1` dans dlssg_for_sm86. Il est inactif en Vulkan, avec les jeux qui taguent par `slSetTagForFrame`, et quand le jeu fournit lui-même ces entrées. Valider avec l'indicateur NVIDIA (`DLSSG_IndicatorText=2` dans `HKLM\SOFTWARE\NVIDIA Corporation\Global\NGXCore`) : il doit afficher « Hudless: Yes · UIAlpha: Yes · UIR: ON ».

## Codes du journal (`DLSSG-Transfusion.log`, préfixe « UI assist »)

| Code | Sens |
|---|---|
| A0 / A1 | Le jeu tague son propre HUD-less / sa propre couche UI (capture / synthèse inactives) |
| A2 / A3 | Installation des hooks D3D12 / les hooks reçoivent les barrières |
| B1 / B2 | Buffer Streamline déplacé par barrières enhanced / classiques |
| B3 | Buffer en RENDER_TARGET, capture armée |
| B4 | Draws non conformes à la règle de capture |
| C1 `WxH fmt` | Texture de copie créée |
| C2 | Première scène capturée |
| C3 | Repli sur l'image finie |
| D1 / D2 | Image finale observée / apprise via la copie de Streamline |
| D3 | Première couche UI synthétisée et taguée |
| D4 `n%` / D5 | Pause / reprise |
| E1 / E3 | Streamline refuse le tag HUD-less / UI |
| E2 | Objets GPU indisponibles |
| E4 / E5 | Échec d'installation |

## Validation (RTX 3070 Ti Laptop)

| Test | Résultat |
|---|---|
| `tests/hud_assist` : test GPU D3D12 porté de dlssg_for_sm86, sur un vrai device. Il rejoue la structure de frame de Crimson Desert : capture, 1 ou 2 draws, replis, buffer non présentable, apprentissage via le clone, priorité du jeu (dont le **nouveau cas UIAlpha**), mode interdit, pause et reprise, nom ANSI, barrières enhanced, device sain à la fin | **Tous passés** |
| Runtime NVIDIA, Ampere X2 1080p | Toujours 5/5 identiques à NVIDIA |
| Harnais de contrôle de Tony | Inchangé (échec dynamique préexistant) |

### Non validé

- **En jeu dans Transfusion.** Dans dlssg_for_sm86, la même logique a été validée sur Crimson Desert et Cyberpunk 2077. Ici, seule la couche d'intégration change : soumission des tags, activation et journal.
- Vulkan (non implémenté), et seuils HDR non calibrés en nits.
