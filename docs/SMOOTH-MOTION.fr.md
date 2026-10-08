# Smooth Motion sur RTX 30

*[English](SMOOTH-MOTION.md) · [中文](SMOOTH-MOTION.zh-CN.md)*

Smooth Motion est l'interpolation d'images du pilote NVIDIA : elle ajoute une image entre deux images calculées
dans les jeux DirectX 11, DirectX 12 et Vulkan, **y compris les jeux sans DLSS**. RTX Encore la rend disponible sur
les cartes RTX 30.

> [!NOTE]
> Cette fonction est désactivée par défaut. Elle est distincte de DLSS Frame Generation : les deux
> peuvent servir dans des jeux différents, ou ensemble dans un jeu qui a les deux.

## Prérequis

- Une carte **RTX 30**. Les RTX 40 tiennent Smooth Motion de NVIDIA ; les RTX 20 ne sont pas prises en charge.
- L'une de ces versions du pilote NVIDIA, exactement : **617.42**, **617.14**, **616.92** ou **616.64**.
  Seule la 617.14 a été vérifiée en jeu à ce jour. Avec tout autre pilote, la fonction reste désactivée et dit
  pourquoi ; le reste de RTX Encore continue de fonctionner.

## L'activer

1. Ouvrez le menu (**Inser**), onglet **Frames**, section **Smooth Motion**.
2. Activez **Smooth Motion**.
3. Réglez **Graphics API** sur l'API que le jeu utilise réellement : Direct3D 12, Direct3D 11 ou Vulkan. Ce n'est
   pas toujours celle que son moteur laisse supposer ; la page du jeu sur
   [PCGamingWiki](https://www.pcgamingwiki.com/) l'indique.
4. Relancez le jeu.

La pastille **Smooth Motion** en haut du menu montre l'état. « Driver prepared » signifie que le pilote est prêt ;
cela ne confirme pas à soi seul que des images interpolées sont affichées. Vérifiez le résultat avec un outil qui
mesure les images affichées, comme NVIDIA FrameView.

## Ce qui a été observé

| API | Jeu | Résultat rapporté |
| :--- | :--- | :--- |
| DirectX 12 | Manor Lords | 120 FPS affichés pour un jeu limité à 60 FPS |
| DirectX 11 | Shadows of Doubt | 120 FPS affichés pour un jeu limité à 60 FPS |
| Vulkan | Enshrouded | Images interpolées affichées, le profil NVIDIA du jeu étant sur Off |

Ce sont quelques observations d'utilisateurs avec le pilote 617.14. Elles ne garantissent ni la compatibilité, ni
la latence, ni la qualité d'image dans d'autres jeux.

## Dans l'overlay

| Exemple | Signification |
| :--- | :--- |
| `148/74 fps sm` | Sortie estimée avec Smooth Motion / fréquence d'images du jeu |
| `296/74 fps 2x+sm` | DLSS Frame Generation X2, puis Smooth Motion |

Le chiffre de Smooth Motion est une estimation : le pilote peut suspendre l'interpolation, et l'overlay ne mesure
pas ce que l'écran reçoit.

## Si cela ne fonctionne pas

- **Le menu signale un pilote non pris en charge** : installez l'une des quatre versions ci-dessus.
- **Rien ne change en jeu** : vérifiez le choix **Graphics API**, et relancez le jeu après chaque changement.
- **Vulkan** : essayez avec Smooth Motion sur Off dans le profil du jeu de l'application NVIDIA.
- Joignez le rapport du menu (onglet **System**, **Copy**) quand vous ouvrez un ticket.
