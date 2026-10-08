# Journal des versions

*[English](CHANGELOG.md) · [中文](CHANGELOG.zh-CN.md)*

## 1.0.0

Première version sous le nom **RTX Encore**. La numérotation repart à 1.0.0 : cette version succède à
`v1.4.5.3-rtx20-30-40`, publiée sous le nom DLSSG-Transfusion, et la remplace.

### Nouveautés

- **Menu et overlay intégrés.** Appuyez sur **Inser** dans le jeu : tous les réglages, l'état de chaque fonction, et
  un overlay avec la fréquence d'images, le multiplicateur, la régularité des images, le GPU et la VRAM. Il
  fonctionne dans les jeux DirectX 11, DirectX 12 et Vulkan. ReShade n'est plus nécessaire, et l'ancien add-on
  ReShade n'est plus fourni.
- **Neural Rendering** sur RTX 20, RTX 30 et RTX 40, appliqué à l'image produite par DLSS : une à quatre passes avec
  un style pour chacune, prise en charge du HDR et choix du moteur NVIDIA ou Open, expérimental. Expérimental,
  désactivé par défaut ; demande le fichier `nvngx_dlssnr.dll` 310.8.0 de NVIDIA. Open ne prend pas en charge RTX 20.
- **Nos travaux de performance NR pour Ampere (RTX 30).** Traitement plus rapide avec une sortie identique dans
  les tests locaux de référence du moteur NVIDIA ; précision Fast NVIDIA avec environ 17 % de temps NR en moins
  mesurés sur RTX 30 ; résolution NR indépendante, dont Ultra Performance ; précision Fast Open et options de
  vitesse/VRAM. Notre mode ultra-rapide **Ultra-fast mode** a réduit le temps NR de **18,7 à 9,8 ms (−48 %)**
  et porté les FPS affichés de **73,8 à 94,9 (+29 %)** dans le benchmark Wukong sur un RTX 3070 Ti Laptop avec FG X3,
  un passage par configuration. Une passe NR, défauts possibles en mouvement ; résultats non garantis.
  [Mesures et compromis](docs/NEURAL-RENDERING.fr.md).
- **Smooth Motion sur RTX 30** accepte maintenant les pilotes NVIDIA 617.42, 616.92 et 616.64 en plus du 617.14, et
  se règle depuis le menu. Seul le 617.14 a été vérifié en jeu.
- **Un seul fichier universel.** `rtx-encore.dll` accepte désormais dix-neuf noms de fichier, et peut donc cohabiter
  avec les mods qui utilisent déjà les noms habituels.
- **Nouvelles commandes** : une limite d'images calculées par NVIDIA Reflex, une demande de présentation sans
  V-Sync, et des cheveux en ray tracing compatibles dans The Witcher 3.
- **Panneau optionnel pour Cyber Engine Tweaks** dans Cyberpunk 2077 (expérimental).
- **Les journaux de session** sont conservés dans un dossier `rtx-encore-logs`, les trois plus récents par défaut.

### Changements

- Les fichiers s'appellent `rtx-encore.*`. Les réglages, les raccourcis et un ancien plugin sont repris
  automatiquement : voir
  [Mise à niveau depuis un ancien nom](docs/INSTALLATION.fr.md#mise-à-niveau-depuis-un-ancien-nom).
- Le fichier de réglages est `rtx-encore.jsonc`, organisé en sections, avec un commentaire sur chaque clé.
- Plusieurs réglages et entrées du menu ont un nom plus clair. Un fichier de réglages d'une version antérieure est
  repris avec ses valeurs.
- La génération d'images coûte 2 à 3 % de temps GPU en moins sur les cartes RTX 40.
- Le texte de l'overlay est plus compact, avec un réglage de taille.

### Limites connues

- La prise en charge des RTX 20 est expérimentale et n'a pas été validée sur une vraie carte RTX 20.
- Neural Rendering avec plusieurs passes, le moteur ouvert, le panneau Cyber Engine Tweaks et les cheveux en ray
  tracing sur RTX 20 et RTX 30 ont été peu ou pas testés en jeu.
- X5 et X6 sont expérimentaux et demandent beaucoup de VRAM.

## Versions antérieures

Les versions jusqu'à `v1.4.5.3-rtx20-30-40` ont été publiées sous le nom DLSSG-Transfusion. Elles ne sont plus distribuées.
