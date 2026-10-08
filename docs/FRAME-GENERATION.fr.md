# Génération d'images

*[English](FRAME-GENERATION.md) · [中文](FRAME-GENERATION.zh-CN.md)*

RTX Encore apporte DLSS Multi Frame Generation, de X2 à X6, aux cartes RTX 20, RTX 30 et RTX 40.

## Prérequis

- Un jeu qui propose **DLSS Frame Generation**, en DirectX 12 ou Vulkan. RTX Encore étend la génération d'images
  que le jeu possède déjà ; il ne peut pas en ajouter une à un jeu qui n'en a pas. Pour ces jeux, voir
  [Smooth Motion](SMOOTH-MOTION.fr.md).
- Le fichier de génération d'images du jeu, `nvngx_dlssg.dll`, dans une version prise en charge : voir
  [Compatibilité](COMPATIBILITY.fr.md#versions-de-dlss-frame-generation). La version 310.9.1 est celle qui en tire
  le plus.
- La génération d'images activée dans les réglages du jeu.

## Modes

Choisissez le mode dans le menu (onglet **Frames**) ou avec les raccourcis.

| Mode | Ce qu'il fait |
| :--- | :--- |
| **Game decides** (par défaut) | Le jeu, ou NVIDIA Profile Inspector, choisit le multiplicateur. |
| **Fixed** | Toujours le multiplicateur que vous choisissez : X2, X3, X4, X5 ou X6. |
| **Dynamic** | Le multiplicateur suit la fréquence d'images pour atteindre une cible. La cible est la fréquence de rafraîchissement de l'écran qui affiche le jeu, ou une valeur que vous fixez. |

Le multiplicateur est le nombre d'images affichées pour chaque image que le jeu calcule : X3 affiche deux images
générées après chaque image calculée.

- **X5 et X6 sont expérimentaux.** Ils demandent beaucoup de VRAM, et la latence augmente avec le multiplicateur.
- Le mode Dynamique reste à X4 ou moins tant que **Allow 5x and 6x** n'est pas activé.
- Dans les jeux Vulkan, le mode Dynamique choisit entre X2 et X6 d'après la fréquence mesurée et la cible.
- Sur une carte de 8 Go, commencez par X2 ou X3, ou X4 avec des textures Élevées ou Moyennes.

## Raccourcis

| Raccourci | Action |
| :--- | :--- |
| `Ctrl + Alt + 2…6` | Multiplicateur fixe X2 à X6 |
| `Ctrl + Alt + Page préc. / Page suiv.` | Augmenter ou réduire le multiplicateur (passe en mode Fixe) |
| `Ctrl + Alt + D` | Basculer entre les modes Fixe et Dynamique |
| `Ctrl + Alt + Haut` ou `+` | Augmenter la cible du mode Dynamique de 5 FPS (`Maj` : 1 FPS) |
| `Ctrl + Alt + Bas` ou `-` | Réduire la cible du mode Dynamique de 5 FPS (`Maj` : 1 FPS) |
| `Ctrl + Alt + G` | Revenir à **Game decides** |

Un raccourci enregistre le nouveau choix et l'applique aussitôt. Les raccourcis se changent, ou se désactivent un
par un, dans le menu (onglet **System**).

## Qualité d'image

Le groupe **Quality** de l'onglet **Frames** réunit les options qui agissent sur les images générées. Elles sont
activées par défaut et sont lues au démarrage du jeu.

- **Anti-tearing / anti-ghosting** : protège la géométrie fine (grillages, câbles, feuillages) et les ombres en
  mouvement dans les images générées.
- **Protection tuning** : *Refined* (par défaut) ou *Classic*. Essayez Classic si un jeu montre des défauts que
  Refined ne supprime pas.
- **High-multiplier quality** : garde les images générées propres de X3 à X6. Laissez-le activé.
- **Fast frame generation** : l'option de performance décrite ci-dessous. Laissez-la activée.

### Fast frame generation

Avec **Fast frame generation**, chaque image générée coûte moins de temps GPU et l'image est la même. Mesuré sur un
RTX 3070 Ti Laptop en 1080p :

| Multiplicateur | Option désactivée | Option activée | Écart |
| :---: | ---: | ---: | ---: |
| X2 | 2,33 ms | 1,57 ms | −33 % |
| X6 | 6,38 ms | 5,23 ms | −18 % |

Ces chiffres viennent d'un seul système ; ils ne sont pas une promesse pour toutes les cartes ni tous les jeux.
L'option demande `nvngx_dlssg.dll` 310.9.1.

## Interface du jeu

Les images générées sont les plus propres quand l'interface du jeu est traitée à part de la scène 3D. Trois
interrupteurs, dans l'onglet **Frames** :

| Réglage | Défaut | Ce qu'il fait |
| :--- | :---: | :--- |
| **Automatic UI recomposition** | activé | Utilise le traitement séparé de l'interface dès que le jeu fournit ce qu'il faut. Désactivez-le si les images générées sont déformées ou dédoublées en mouvement. |
| **UI assist (D3D12)** | activé | Dans les jeux DirectX 12 qui ne fournissent pas une scène sans interface et une couche d'interface, RTX Encore les construit. Ce que le jeu fournit lui-même a toujours priorité. |
| **Force UI recomposition** | désactivé | Demande le traitement séparé même quand le jeu ne le demande pas. Pour les essais. |

Les recommandations propres à certains jeux sont dans [Compatibilité](COMPATIBILITY.fr.md#notes-par-jeu).

## Options de compatibilité

Dans l'onglet **System** :

| Réglage | Défaut | Ce qu'il fait |
| :--- | :---: | :--- |
| **Disable menu detection** | désactivé | Désactivé : la génération d'images se met en veille dans les menus et les écrans de chargement, ce qui y évite des plantages. Laissez-le désactivé. |
| **Force NVIDIA OTA models** | désactivé | Utilise les modèles de génération d'images téléchargés par l'application NVIDIA. Relancez le jeu pour appliquer. |
| **OptiScaler flip metering bypass** | désactivé | Seulement pour les installations avec OptiScaler qui en ont besoin. Relancez le jeu pour appliquer. |
| **Graphics card series** | auto | Laissez sur auto, sauf si la carte n'est pas reconnue. |

## Lire l'overlay

| Exemple | Signification |
| :--- | :--- |
| `74 fps` | Fréquence d'images du jeu ; aucune génération d'images observée |
| `74/37 fps 2x` | Fréquence affichée / fréquence calculée, avec le multiplicateur observé |
| `296/74 fps 2x+sm` | Génération d'images et Smooth Motion ensemble |

Si aucun DLSS Frame Generation n'est détecté dans la session, choisir un multiplicateur ne peut pas en créer un :
le menu le signale.

## Limites

- La prise en charge des RTX 20 est expérimentale : elle n'a pas encore été validée sur une vraie carte RTX 20.
- Vulkan sur RTX 20 et RTX 30 est expérimental.
- La génération d'images augmente la fréquence affichée, pas la réactivité du jeu : la latence augmente avec le
  multiplicateur, et le résultat est meilleur quand la fréquence calculée est déjà confortable.
