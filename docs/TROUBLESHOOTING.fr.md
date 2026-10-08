# Dépannage

*[English](TROUBLESHOOTING.md) · [中文](TROUBLESHOOTING.zh-CN.md)*

## Premières vérifications

1. **Le mod est-il chargé ?** Un dossier `rtx-encore-logs` apparaît à côté du fichier du mod quand il l'est, et
   **Inser** ouvre le menu. Si ni l'un ni l'autre ne se produit, le jeu n'a pas chargé le fichier.
2. **N'y a-t-il qu'une seule copie ?** Deux copies du mod dans le même jeu, sous deux noms, provoquent des plantages
   et des comportements étranges.
3. **Que dit le menu ?** Les quatre pastilles du haut donnent l'état de chaque fonction, et une carte nomme tout
   problème durable avec sa correction.

## Le mod ne se charge pas

| Vérification | Quoi faire |
| :--- | :--- |
| Mauvais dossier | Le fichier doit être à côté de l'exécutable qui fait réellement tourner le jeu. Pour Unreal Engine : `<Jeu>\Binaries\Win64\`. |
| Ce jeu ne charge pas ce nom | Essayez le nom suivant : `version.dll`, puis `dinput8.dll`, `winmm.dll`, `dxgi.dll`. |
| Le nom est pris par un autre mod | Laissez son fichier à ce mod et choisissez un autre nom pour RTX Encore. |
| Le fichier renommé est refusé | Utilisez la copie déjà nommée du dossier `alternative-proxies`. |
| Le jeu a un anti-triche | Il peut bloquer le fichier. RTX Encore ne doit pas y être utilisé. |

## Génération d'images

| Symptôme | Première vérification |
| :--- | :--- |
| Le jeu n'a pas d'option Frame Generation | RTX Encore étend DLSS Frame Generation dans les jeux qui l'ont ; il ne peut pas l'ajouter. Voir [Smooth Motion](SMOOTH-MOTION.fr.md) pour les autres jeux. |
| L'option existe mais est grisée | Vérifiez que DLSS est activé, que le jeu tourne en DirectX 12 ou Vulkan, et que le mod est chargé. |
| Le multiplicateur reste à X2 | Désactivez puis réactivez la génération d'images dans le jeu, ou relancez-le. Vérifiez le mode dans le menu : en **Game decides**, c'est le jeu qui choisit. |
| X5 ou X6 n'est pas atteint en mode Dynamique | Activez **Allow 5x and 6x**. |
| Rien ne se passe avec cette version du jeu | Le fichier `nvngx_dlssg.dll` du jeu est peut-être dans une version non prise en charge : voir [Compatibilité](COMPATIBILITY.fr.md#versions-de-dlss-frame-generation). |
| Contours dédoublés ou images déformées en mouvement | Désactivez **Automatic UI recomposition**. |
| Défauts sur les grillages, les feuillages ou les ombres | Gardez **Anti-tearing / anti-ghosting** activé et essayez l'autre **Protection tuning**. |
| Saccades ou plantages aux multiplicateurs élevés | La VRAM est probablement pleine : baissez le multiplicateur ou la qualité des textures. |
| Plantage dans les menus ou les écrans de chargement | Laissez **Disable menu detection** désactivé. |

## Smooth Motion et Neural Rendering

Voir la fin de leurs pages : [Smooth Motion](SMOOTH-MOTION.fr.md#si-cela-ne-fonctionne-pas),
[Neural Rendering](NEURAL-RENDERING.fr.md#quand-quelque-chose-ne-fonctionne-pas).

## Menu et réglages

| Symptôme | Première vérification |
| :--- | :--- |
| **Inser** ne fait rien | Le mod n'est pas chargé (voir plus haut), ou le raccourci a été changé : regardez `menuState` dans `rtx-encore.jsonc`. |
| Le menu s'ouvre dans une fenêtre séparée | Normal dans quelques jeux : voir [Menu et overlay](MENU-AND-OVERLAY.fr.md#quand-le-menu-souvre-dans-une-fenêtre-séparée). |
| Les clics ne sont pas pris en compte | Relâchez tous les boutons de la souris, puis cliquez de nouveau. Le mode fenêtré sans bordure aide. |
| Les réglages reviennent aussitôt en arrière | Le fichier de réglages n'a pas pu être enregistré : vérifiez qu'il n'est ni en lecture seule, ni ouvert et verrouillé dans un éditeur. |
| Un réglage n'a aucun effet | Les réglages marqués `*` sont lus au démarrage du jeu : relancez le jeu. |
| Les raccourcis ne font rien | Ils n'agissent que lorsque la fenêtre du jeu a le focus, et `disableKeybinds` doit valoir `false`. |

## Signaler un problème

Ouvrez un [ticket](https://github.com/SilyNoMeta/rtx-encore/issues) avec :

- le jeu, sa version ou sa boutique, et son API graphique ;
- votre carte et la version du pilote NVIDIA ;
- la version de RTX Encore et le nom que vous avez donné au fichier ;
- le rapport copié depuis le menu (onglet **System**, **Copy**) ;
- le journal de la session où le problème s'est produit, pris dans `rtx-encore-logs`. Les journaux peuvent contenir
  des noms de dossiers de votre PC : relisez-les avant de les publier.

Un problème par ticket, avec ce que vous avez fait et ce que vous avez vu.
