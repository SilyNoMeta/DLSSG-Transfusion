# Fichier de réglages

*[English](SETTINGS.md) · [中文](SETTINGS.zh-CN.md)*

Tous les réglages se changent dans le [menu](MENU-AND-OVERLAY.fr.md) (**Inser**), ce qui est le plus simple. Cette
page s'adresse à ceux qui préfèrent modifier le fichier.

## Le fichier

- Nom : `rtx-encore.jsonc`, à côté du fichier du mod. Il est créé au premier lancement avec les valeurs par défaut.
- Format : JSON avec des commentaires `//`. Chaque clé porte un commentaire qui dit ce qu'elle fait ; certains
  éditeurs signalent les commentaires, le mod les lit sans problème.
- Les réglages sont regroupés en sections. La plupart s'appliquent en direct ; le commentaire indique quand il faut
  relancer le jeu.
- Le raccourci et la disposition du menu sont enregistrés dans le même fichier, dans sa section `menuState`.
- Un fichier de réglages laissé par une version antérieure est repris avec ses valeurs : voir
  [Installation](INSTALLATION.fr.md#mise-à-niveau-depuis-un-ancien-nom).

Aucune clé ne contient de chemin ni de commande : un fichier de réglages peut être partagé tel quel.

## Clés principales

### `frameGeneration`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `mode` | `"game"` | `"game"` (le jeu ou NVIDIA Profile Inspector décide), `"fixed"`, `"dynamic"` |
| `multiplier` | `4` | `2` à `6`, utilisé en mode fixe. `5` et `6` sont expérimentaux. |
| `dynamicTargetFrameRate` | `0` | FPS visés en mode dynamique. `0` suit la fréquence de rafraîchissement de l'écran qui affiche le jeu. |
| `dynamicExperimental56` | `false` | Laisse le mode dynamique monter à X5 et X6. |

### `dlssSuperResolution`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `dlssRenderScale` | `"game"` | `"game"`, `"dlaa"`, `"quality"`, `"balanced"`, `"performance"`, `"ultra-performance"`, `"custom"` |
| `dlssCustomScale` | `67` | Pourcentage, `50` à `100`, avec `"custom"` |

### `neuralRendering`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `nrEnabled` | `false` | Active Neural Rendering. |
| `nrEngine` | `"nvidia"` | `"nvidia"` ou `"opendlss"` (très expérimental). Relancez pour appliquer. |
| `nrPasses` | `1` | `1` à `4` |
| `nrLaterPassLocalTone` | `0` | Ton appliqué après la première passe, `0` à `2` |
| `nrResolution` | `"render"` | `"render"`, `"output"`, `"quality"`, `"balanced"`, `"performance"`, `"ultra-performance"`, `"custom"` |
| `nrResolutionScale` | `67` | Pourcentage de la taille de sortie, `33` à `100`, avec `"custom"` |
| `nrIntensity` | `1` | Intensité, `0` à `2` |
| `nrStyle` | `0` | `0` par défaut, `1` naturel, `2` cinématique |
| `nrPass1Style` … `nrPass4Style` | `"inherit"` | `"inherit"` (suit `nrStyle`), `"default"`, `"natural"`, `"cinematic"` |
| `nrLocalTone`, `nrLocalStructure` | `1` | `0` à `2` |
| `nrAutoMask` | `false` | Masque des personnages ; nécessaire pour `nrSkinStructure` |
| `nrSkinStructure` | `1` | `0` à `2` |
| `nrHdrExposure` | `1` | Jeux HDR : luminosité donnée à NR |
| `nrPrecision` | `"exact"` | `"exact"` ou `"fast"` (RTX 20 et RTX 30). Relancez pour appliquer. |
| `nrPreset` | `0` | Profil de modèle NVIDIA : `0` automatique (recommandé), `1` à `3` |

#### Options de performance NR

Ces réglages se trouvent aussi dans **Image → Neural Rendering → Performance**. Voir
[Neural Rendering](NEURAL-RENDERING.fr.md#réglages-de-performance-sur-rtx-30) pour les mesures et les compromis
d'image. Les options propres à un moteur ne s'appliquent qu'au moteur indiqué ci-dessous ; changer de moteur ou
d'option lue au démarrage demande de relancer le jeu.

| Clé | Défaut | Gain et compromis |
| :--- | :---: | :--- |
| `nrPaddingAware` | `false` | Autorise une taille NR voisine plus petite, au plus 2 % de moins par axe, sans changer la taille de rendu DLSS. Peut changer l'image. S'applique en direct. |
| `nrMaxInFlight` | `0` | NVIDIA, DirectX 12 avec Reflex : `0` sans limite, `1` ou `2` images NR en attente ; la génération d'images active utilise au moins `2`. S'applique en direct ; effet sur les performances en jeu à vérifier. |

### `openExperimental`

Les clés de performance Open sont dans cette section distincte. Choisissez d'abord le moteur et le backend adapté
à votre carte depuis le menu ; ces clés n'activent pas Open à elles seules.

| Clé | Défaut | Gain et compromis |
| :--- | :---: | :--- |
| `nrOpenFast` | `false` | Open sur RTX 30 : précision Fast, temps NR plus court avec de petites différences d'image et sans VRAM supplémentaire. Redémarrage requis. |
| `nrOpenVramForSpeed` | `false` | Open sur RTX 30 : temps NR plus court, à image identique, contre environ 160 Mo de VRAM en plus en 1440p DLSS Balanced. Redémarrage requis. |
| `nrOpenFastProjection` | `true` | Open sur RTX 30 : traitement plus rapide, avec de petites différences d'image. Redémarrage requis. |
| `nrOpenUltraFast` | `false` | Open, une passe NR : **Ultra-fast mode**, environ moitié moins de temps NR dans la comparaison documentée. Couche NR plus ancienne, défauts possibles en mouvement, quelques Mo de VRAM en plus. Redémarrage requis. |
| `nrOpenUltraFastGhostTolerance` | `0.03` | `0` à `0.3` ; plus bas : moins de fantômes, davantage de surfaces en attente temporaire de NR. S'applique en direct. |
| `nrOpenUltraFastFillTolerance` | `0.03` | `0` à `0.3` ; règle le remplissage des surfaces qui apparaissent avec le NR de surfaces semblables. `0` désactive le remplissage. S'applique en direct. |

`nrResolution: "ultra-performance"` règle NR à 33 % de la largeur et de la hauteur de sortie. Ce choix est
indépendant de `nrOpenUltraFast` et de `dlssRenderScale` ; ces réglages ne s'activent pas mutuellement.

### `overlay`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `showOverlay` | `false` | Affiche l'overlay. |
| `overlayPosition` | `"top-left"` | `"top-left"`, `"top-right"`, `"bottom-left"`, `"bottom-right"` |
| `overlayFontSize` | `14` | Taille du texte en pixels, `10` à `32` |
| `overlayShowNr`, `overlayShowFramePacing`, `overlayShowGpu`, `overlayShowVram`, `overlayShowVersions` | `false` | Lignes optionnelles |

### `hudUi`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `autoUiRecomposition` | `true` | Traitement séparé de l'interface dès que le jeu fournit ce qu'il faut |
| `uiAssist` | `true` | DirectX 12 : construit la scène sans interface et la couche d'interface quand le jeu ne les fournit pas |
| `forceUiRecomposition` | `false` | Demande le traitement séparé même quand le jeu ne le demande pas |

### `nativeMenuFeatures`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `reflexFrameLimit` | `0` | `0` suit le jeu ; `1` à `1000` plafonne les images calculées |
| `vsyncOff` | `false` | Demande aux jeux DirectX de présenter sans V-Sync |
| `hairEnabled` | `true` | Cheveux en ray tracing compatibles, dans les jeux qui en ont. Relancez pour appliquer. |

### `diagnostics`

| Clé | Défaut | Valeurs |
| :--- | :---: | :--- |
| `logPerformance` | `false` | Écrit la fréquence et les temps d'image dans un fichier CSV de `rtx-encore-logs` |
| `logFilesKept` | `3` | Journaux de session conservés, `1` à `100` |

### `keyboardShortcuts`

Chaque clé `hotkey…` prend une ou plusieurs combinaisons séparées par des virgules, par exemple
`"hotkeyFixed4": "Ctrl+Alt+4, Ctrl+Alt+Num4"`. Une valeur vide `""` désactive l'action.
`"disableKeybinds": true` désactive tous les raccourcis.

## Autres clés

Le fichier contient plus de clés que cette page n'en liste : Smooth Motion, les interrupteurs de qualité d'image et
de compatibilité, et les options expérimentales du moteur Open. Chacune a son commentaire dans le fichier et une
commande libellée dans le menu, qui est l'endroit recommandé pour les changer.
