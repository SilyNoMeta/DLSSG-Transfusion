# Compatibilité

*[English](COMPATIBILITY.md) · [中文](COMPATIBILITY.zh-CN.md)*

Ces notes consignent ce qui a été testé et observé. Elles ne valent pas certification : une mise à jour du jeu, un
autre pilote ou un autre mod peut changer le résultat.

## Cartes graphiques

| Carte | Génération d'images | Smooth Motion | Neural Rendering |
| :--- | :--- | :--- | :--- |
| RTX 40 | Prise en charge. DirectX 12 et Vulkan éprouvés sur une RTX 4090. | Fourni par NVIDIA | Pris en charge |
| RTX 30 | Prise en charge. DirectX 12 éprouvé sur un RTX 3070 Ti Laptop. | Pris en charge avec les pilotes listés plus bas | Pris en charge |
| RTX 20 | Expérimentale : pas encore validée sur une vraie carte RTX 20. | Non pris en charge | Expérimental |

RTX Encore est fait pour les cartes RTX 20, 30 et 40. Les cartes GTX et celles d'autres fabricants ne sont pas
prises en charge.

## API graphiques

| Fonction | DirectX 11 | DirectX 12 | Vulkan |
| :--- | :---: | :---: | :---: |
| Génération d'images X2 à X6 | — | ✅ | ✅ (expérimental sur RTX 20 et RTX 30) |
| UI assist | — | ✅ | — |
| Smooth Motion (RTX 30) | ✅ | ✅ | ✅ |
| Neural Rendering | 🧪 | 🧪 | 🧪 |
| Menu et overlay | ✅ | ✅ | ✅ |

Les jeux DirectX 9 et OpenGL ne sont pas pris en charge.

## Versions de DLSS Frame Generation

La génération d'images fonctionne avec ces versions du fichier `nvngx_dlssg.dll` du jeu :

`310.1.0` · `310.2.0` · `310.2.1` · `310.3.0` · `310.4.0` · `310.5.0` · `310.5.2` · `310.5.3` · `310.6.0` ·
`310.7.0` · `310.7.128` · `310.7.129` · `310.8.0` · `310.9.0` · `310.9.1`

- Toute autre version est laissée telle quelle : le jeu garde sa propre génération d'images.
- **310.9.0 ou 310.9.1** est recommandée sur toutes les cartes, pour la meilleure qualité au-delà de X2, et
  nécessaire sur les RTX 20. **Fast frame generation** demande la 310.9.1.
- Les autres fichiers DLSS du jeu (Super Resolution, Ray Reconstruction, ...) peuvent avoir n'importe quelle
  version.

## Pilotes pour Smooth Motion

Smooth Motion sur RTX 30 accepte les pilotes NVIDIA **617.42**, **617.14**, **616.92** et **616.64**, et aucun
autre. Seul le **617.14** a été vérifié en jeu. Voir [Smooth Motion](SMOOTH-MOTION.fr.md).

## Fichier pour Neural Rendering

Neural Rendering demande le fichier `nvngx_dlssnr.dll` **310.8.0** de NVIDIA, que vous fournissez. Voir
[Neural Rendering](NEURAL-RENDERING.fr.md).

## Notes par jeu

| Jeu | API | Notes |
| :--- | :---: | :--- |
| Cyberpunk 2077 | DX12 | Génération d'images testée sur un RTX 3070 Ti Laptop. Fonctionne sous le nom `version.dll` ou comme plugin `.asi` ; voir [Installation](INSTALLATION.fr.md#cyberpunk-2077). |
| Black Myth: Wukong | DX12 | Génération d'images testée sur une RTX 4090. Neural Rendering confirmé par des utilisateurs. |
| No Man's Sky | Vulkan | Génération d'images testée sur une RTX 4090. **Désactivez Automatic UI recomposition** : dans ce jeu, il provoque des contours dédoublés quand la caméra bouge. Neural Rendering avec la génération d'images X2 confirmé en 1080p et 1440p. |
| DOOM: The Dark Ages | Vulkan | Génération d'images testée sur une RTX 4090. Le jeu garde son propre choix pour le traitement de l'interface, quel que soit le réglage. |
| Starfield | DX12 | Fonctionne avec UI assist activé ; un plantage au chargement d'une sauvegarde a été corrigé après le rapport d'un utilisateur. |
| The Witcher 3 | DX12 | Cheveux en ray tracing compatibles sur les versions du jeu 25575366 et 25646871 ; voir [Menu et overlay](MENU-AND-OVERLAY.fr.md#autres-commandes). |
| Manor Lords | DX12 | Smooth Motion observé en fonctionnement sur RTX 30 (pilote 617.14). |
| Shadows of Doubt | DX11 | Smooth Motion observé en fonctionnement sur RTX 30 (pilote 617.14). Neural Rendering confirmé par des utilisateurs. |
| Enshrouded | Vulkan | Smooth Motion observé en fonctionnement sur RTX 30 (pilote 617.14). Neural Rendering confirmé par des utilisateurs. |
| Bodycam, Palworld, Portal with RTX, Star Wars Zero Company | — | Neural Rendering confirmé par des utilisateurs. |

Dites-nous ce que vous observez dans d'autres jeux en ouvrant un
[ticket](https://github.com/SilyNoMeta/rtx-encore/issues) : jeu, API graphique, carte, pilote, et ce qui fonctionne
ou non.

## Autres outils

| Outil | Notes |
| :--- | :--- |
| ReShade, Special K | Laissez-leur leur `dxgi.dll` et donnez un autre nom à RTX Encore. ReShade n'est pas nécessaire pour le menu. |
| DXVK | Ne nommez pas RTX Encore `dxgi.dll` : DXVK fournit ce fichier. Le menu est dessiné dans sa sortie Vulkan. |
| OptiScaler | Une option dédiée existe pour les installations qui en ont besoin : voir [Génération d'images](FRAME-GENERATION.fr.md#options-de-compatibilité). |
| Cyber Engine Tweaks, RED4ext | Laissez-leur leurs fichiers sous leur nom. Un panneau optionnel et expérimental existe pour Cyber Engine Tweaks. |
| NVIDIA Profile Inspector | En mode **Game decides**, le multiplicateur qu'il fixe est suivi. |

## Anti-triche et jeux en ligne

RTX Encore se charge dans le jeu. Les jeux protégés par un anti-triche peuvent refuser de démarrer, ou signaler le
compte. Ne l'y utilisez pas.
