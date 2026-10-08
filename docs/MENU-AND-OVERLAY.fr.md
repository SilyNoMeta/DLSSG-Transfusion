# Menu et overlay

*[English](MENU-AND-OVERLAY.md) · [中文](MENU-AND-OVERLAY.zh-CN.md)*

RTX Encore dessine son propre menu et son overlay dans les jeux DirectX 11, DirectX 12 et Vulkan. Rien d'autre n'est
à installer.

## Le menu

Appuyez sur **Inser** dans la fenêtre du jeu pour l'ouvrir ou le fermer. Il s'ouvre une fois de lui-même au premier
lancement. Le raccourci se change dans le menu.

Une rangée de quatre pastilles coiffe chaque page : **Upscaling**, **Frame Generation**, **Smooth Motion** et
**Neural Rendering**. Chacune montre l'état de sa fonction par un point de couleur et un chiffre ; survolez-la pour
le détail, cliquez pour aller à ses réglages. Quand un problème dure, une carte sous la rangée le nomme et dit
comment le corriger.

| Onglet | Ce qu'il contient |
| :--- | :--- |
| **Frames** | La génération d'images avec ses options de qualité et d'interface, Smooth Motion, la limite d'images calculées, la V-Sync |
| **Image** | La résolution de rendu DLSS, Neural Rendering |
| **Overlay** | Un aperçu de l'overlay, sa position, la taille de son texte et les lignes qu'il affiche |
| **System** | Les raccourcis, la compatibilité, les diagnostics et les détails de la session, avec un bouton **Copy** pour les rapports de bug |

À savoir :

- Chaque changement est enregistré aussitôt dans `rtx-encore.jsonc`, commentaires conservés.
- La plupart des réglages s'appliquent en direct. Un `*` après un nom marque un réglage lu au démarrage du jeu ; il
  passe à l'orange une fois modifié, et une ligne en bas du menu liste les changements en attente d'un redémarrage.
- Un réglage modifié a un nom plus lumineux et un bouton de réinitialisation.
- Chaque section montre ses réglages principaux ; les autres sont dans des groupes qui s'ouvrent sur place
  (**Fine tuning**, **Performance**, **Diagnostics**, ...).
- Faites glisser la barre de titre pour déplacer le menu, et son coin inférieur droit pour le redimensionner.
  Position et taille sont retenues.
- Si un bouton était enfoncé à l'ouverture du menu, relâchez-le avant de cliquer.

### Quand le menu s'ouvre dans une fenêtre séparée

Dans quelques jeux, le menu ne peut pas être dessiné sans risque dans l'image. Les réglages s'ouvrent alors dans une
fenêtre séparée et l'overlay devient un petit texte posé sur le jeu, qui laisse passer les clics. Le mode fenêtré
sans bordure est recommandé dans ce cas. Les réglages et leur effet sont les mêmes.

## L'overlay

Activez-le dans l'onglet **Overlay** ou avec `Ctrl + Alt + O` ; `Ctrl + Alt + P` le déplace dans le coin suivant.

| Exemple | Signification |
| :--- | :--- |
| `74 fps` | Fréquence d'images du jeu ; aucune génération d'images observée |
| `74/37 fps 2x` | Fréquence affichée / calculée, avec le multiplicateur observé |
| `148/74 fps sm` | Sortie estimée avec Smooth Motion / fréquence d'images du jeu |
| `296/74 fps 2x+sm` | Génération d'images X2, puis Smooth Motion |

Lignes optionnelles, toutes désactivées par défaut :

| Ligne | Affiche |
| :--- | :--- |
| Neural Rendering | Son état et la taille à laquelle il travaille |
| Régularité des images | Temps d'image : moyenne, 99e centile et gigue |
| GPU | Charge, température, puissance et fréquences |
| VRAM | Utilisée / totale sur la carte, tous programmes confondus |
| Versions | Versions de DLSS, DLSS Frame Generation et Streamline chargées par le jeu |
| Interface | Si le traitement séparé de l'interface est actif, et d'où viennent la scène sans interface et la couche d'interface |

La position (quatre coins) et la taille du texte (10 à 32 px) sont dans le même onglet.

## Autres commandes

| Réglage | Onglet | Ce qu'il fait |
| :--- | :--- | :--- |
| **Render resolution** | Image | Force la résolution à laquelle le jeu calcule l'image avant DLSS : DLAA, Quality, Balanced, Performance, Ultra Performance, ou une échelle personnalisée de 50 à 100 %. Le jeu doit déjà utiliser DLSS. Le menu montre la résolution observée et si le jeu suit la demande ; revenez à *Game* si un jeu se comporte mal. Dans les jeux Unreal Engine 4 et 5, le pourcentage d'écran est ajusté en direct quand c'est possible. |
| **Render-frame limit (FPS)** | Frames | Plafonne les images que le jeu calcule, par NVIDIA Reflex quand le jeu le prend en charge. 0 suit le jeu. Les images générées ne sont pas comptées, et la limite est suspendue en mode Dynamique. |
| **Request V-Sync off** | Frames | Demande aux jeux DirectX de présenter sans V-Sync. Un réglage forcé dans le panneau de configuration NVIDIA garde la priorité. Sans effet dans les jeux Vulkan. |
| **Compatible ray-traced hair** | System | The Witcher 3 (DirectX 12, versions du jeu 25575366 et 25646871) : rend les cheveux en ray tracing du jeu disponibles sur les cartes RTX 20, 30 et 40. Les réglages de cheveux du jeu décident toujours s'ils sont affichés. Pas encore validé en jeu sur RTX 20 et RTX 30. Relancez pour appliquer. |

## Raccourcis

Les raccourcis par défaut sont listés dans [Génération d'images](FRAME-GENERATION.fr.md#raccourcis). Dans l'onglet
**System**, chaque action peut recevoir plusieurs combinaisons de touches, ou aucune. Les raccourcis n'agissent que
lorsque la fenêtre du jeu a le focus.

## Journaux

Les journaux de session sont écrits dans le dossier `rtx-encore-logs` à côté du fichier du mod ; les trois plus
récents sont conservés (**Session logs kept**, 1 à 100). **Log performance** écrit la fréquence et les temps d'image
de la session dans un fichier CSV du même dossier.
