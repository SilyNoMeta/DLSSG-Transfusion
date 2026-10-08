# Neural Rendering

*[English](NEURAL-RENDERING.md) · [中文](NEURAL-RENDERING.zh-CN.md)*

DLSS Neural Rendering (NR) enrichit l'image produite par DLSS Super Resolution ou DLAA. RTX Encore le fait tourner
sur les cartes RTX 20, RTX 30 et RTX 40, dans les jeux DirectX 11, DirectX 12 et Vulkan.

> [!NOTE]
> Neural Rendering est expérimental et désactivé par défaut. Il coûte beaucoup de temps GPU : voir [Coût](#coût).

À chaque fonction son rôle : **DLSS Super Resolution** reconstruit l'image du jeu, **Neural Rendering** en change
le rendu, **la génération d'images** ajoute des images intermédiaires. Activer NR n'active pas la génération
d'images.

## Ce que RTX Encore lui apporte

RTX Encore développe ses propres options de performance NR pour **Ampere (RTX 30)**, en parallèle de la prise en
charge des RTX 40 et du support expérimental des RTX 20. Elles concernent le moteur NVIDIA et notre intégration
d'OpenDLSS-NR : traitement plus rapide, résolution de travail NR indépendante, et **mode ultra-rapide** qui a réduit
le temps NR de **48 %** dans la comparaison Wukong ci-dessous.

| | |
| :--- | :--- |
| **Trois générations de cartes** | RTX 40, RTX 30 et, à titre expérimental, RTX 20, dans les jeux DirectX 11, DirectX 12 et Vulkan. |
| **Traitement plus rapide, à image identique** | Des optimisations activées par défaut pour le moteur NVIDIA sur RTX 20 et RTX 30, avec une sortie identique dans les tests locaux de référence. Performances mesurées sur RTX 30 ; la validation sur une RTX 20 physique reste à faire. |
| **Précision Fast** | Moteur NVIDIA : environ 17 % de temps NR en moins mesurés sur RTX 30, avec de petites différences d'image. Open a aussi sa propre précision Fast pour RTX 30, désactivée par défaut. |
| **Résolution NR indépendante** | NR peut travailler sous la résolution de l'écran, tandis que DLSS conserve sa résolution de rendu et les détails reconstruits. Performance et Ultra Performance réduisent le travail NR sans imposer un réglage DLSS inférieur. |
| **D'autres réglages Open pour RTX 30** | L'option **More speed for more VRAM** réduit le temps NR à image identique, contre davantage de VRAM ; **Fast projection** privilégie la vitesse avec de petites différences d'image. Voir [Réglages de performance](#réglages-de-performance-sur-rtx-30). |
| **Moins de mémoire pour Open** | Environ 41–43 % de mémoire allouée en moins par le moteur NR Open dans les tests locaux de son chemin RTX 30, à sortie identique. Voir [Mémoire](#mémoire-du-moteur-open). |
| **Mode ultra-rapide** | L'option **Ultra-fast mode** développée par RTX Encore pour Open : 18,7 → 9,8 ms de temps NR et 73,8 → 94,9 FPS affichés dans la comparaison Wukong. Voir [Mode ultra-rapide](#mode-ultra-rapide). |
| **Conçu pour cohabiter avec la génération d'images** | NR s'exécute une fois par image calculée, jamais sur les images générées. |
| **Plusieurs passes, un style pour chacune** | Une à quatre passes, chacune avec son rendu. |
| **Jeux HDR** | Pris en charge, avec un réglage de luminosité. |
| **Préparation en arrière-plan** | Pendant la préparation de NR ou quand des données nécessaires du jeu manquent, l'image reste telle que DLSS l'a produite. |

Les options qui consomment davantage de VRAM sont désactivées par défaut.

## Prérequis

- Le fichier NR de NVIDIA, **`nvngx_dlssnr.dll` version 310.8.0**. Il n'est pas inclus dans RTX Encore :
  fournissez-le vous-même et placez-le à côté du fichier du mod, normalement dans le dossier de l'exécutable du jeu.
  Toute autre version est refusée.
- Un jeu avec **DLSS Super Resolution ou DLAA activé**. NR s'applique à son résultat.
- Une carte RTX 40 ou RTX 30. RTX 20 est expérimental.

## L'activer

1. Placez `nvngx_dlssnr.dll` à côté du fichier du mod.
2. Activez DLSS ou DLAA dans le jeu.
3. Ouvrez le menu (**Inser**), onglet **Image**, et activez **Neural Rendering**.

NR se prépare en arrière-plan : les premières images peuvent rester telles que DLSS les a produites. La pastille
**Neural Rendering** en haut du menu indique si NR tourne réellement, et pourquoi quand ce n'est pas le cas.

## Réglages principaux

| Réglage | Défaut | Ce qu'il fait |
| :--- | :---: | :--- |
| **Resolution** | Render | La taille à laquelle NR travaille : la taille de rendu DLSS, la taille de sortie complète, ou Quality (67 %), Balanced (58 %), Performance (50 %), Ultra Performance (33 %) de la sortie, ou une échelle personnalisée. **C'est le principal levier de performance** : plus petit, plus rapide. |
| **Strength** | 100 % | L'intensité de l'effet, de 0 à 200 %. |
| **Style** | Default | Le rendu de l'image : Default, Natural ou Cinematic. |
| **Passes** | 1 | NR peut être appliqué de nouveau à son propre résultat, jusqu'à quatre fois. Chaque passe coûte du temps GPU et de la VRAM, et davantage de passes ne garantit pas une meilleure image. |
| **Precision** | Exact | Moteur NVIDIA, RTX 20 et RTX 30 : *Fast* réduit le temps NR, avec environ 17 % mesurés sur RTX 30 et de petites différences d'image. Open a un choix Exact/Fast séparé pour RTX 30. Relancez le jeu pour appliquer. |

### Réglages de performance sur RTX 30

Le groupe **Image → Neural Rendering → Performance** donne accès aux travaux de RTX Encore pour réduire le coût
NR. Ces options ont des compromis différents ; leurs gains ne sont pas des pourcentages à additionner.

| Réglage | Moteur | Défaut | Gain et compromis |
| :--- | :--- | :---: | :--- |
| **Precision : Fast** | NVIDIA ou Open sur RTX 30 | Exact | Traitement plus rapide, avec une image légèrement différente. Les 17 % mesurés ci-dessus concernent NVIDIA, pas Open. Relancez le jeu pour appliquer. |
| **More speed for more VRAM** | Open sur RTX 30 | Désactivé | Temps NR plus court, à image identique, contre environ 160 Mo de VRAM en plus en 1440p avec DLSS Balanced. Laissez désactivé si la VRAM est presque pleine. Redémarrage requis. |
| **Fast projection** | Open sur RTX 30 | Activé | Traitement plus rapide, avec de petites différences d'image. Redémarrage requis. |
| **Avoid NR padding** | NVIDIA ou Open | Désactivé | Autorise une taille NR voisine plus petite, au plus 2 % de moins par axe. Peut changer l'image NR ; ne change pas la taille de rendu DLSS. S'applique en direct. |
| **NR frames in flight** | NVIDIA, DirectX 12 avec Reflex | 0 | Limite les images NR en attente à 1 ou 2 ; la génération d'images active utilise au moins 2. S'applique en direct ; son effet sur les performances en jeu reste à vérifier. |
| **Ultra-fast mode** | Open, une passe NR | Désactivé | Environ moitié moins de temps NR dans la comparaison ci-dessous, avec une couche NR plus ancienne et des défauts possibles en mouvement. Redémarrage requis. |

Pour comparer les performances, conservez une passe, la même scène, la même résolution de sortie et le même
multiplicateur de génération d'images. Réduisez d'abord **NR Resolution** à Performance ou Ultra Performance en
gardant votre réglage DLSS préféré. Comparez ensuite la précision Fast. Pour essayer le mode ultra-rapide,
choisissez Open et le backend marqué **RTX 30**, activez **Ultra-fast mode**, puis relancez le jeu. Changez
une option à la fois et vérifiez les personnages en mouvement et les surfaces qui apparaissent, ainsi que les FPS.

**Ultra Performance** est le préréglage de résolution NR (33 % de la largeur et de la hauteur de sortie).
**Ultra-fast mode** est le mode ultra-rapide d'Open. On peut les combiner, mais choisir l'un n'active pas
l'autre.

### Passes et styles

Avec plus d'une passe, chaque passe peut avoir son style : **Global** (suit le réglage **Style**), **Default**,
**Natural** ou **Cinematic**. Par exemple, Style Cinematic avec la passe un sur Natural et la passe deux sur Global
donne Natural, puis Cinematic. **Tone after pass one** (0 par défaut) évite de réappliquer la correction de ton à
chaque passe.

Commencez avec une passe et gardez-la comme référence pour comparer. Plusieurs passes n'ont pas encore été validées
en jeu.

### Réglages fins

Le ton local, la structure locale, un masque des personnages avec son propre réglage de structure de la peau, la
luminosité donnée à NR dans les jeux HDR, et le profil de modèle NVIDIA (**Automatic** est recommandé) sont dans les
groupes qui s'ouvrent sous les réglages principaux. Chacun a sa description dans le menu.

## Moteurs

| Moteur | État | Notes |
| :--- | :--- | :--- |
| **NVIDIA** (par défaut) | Expérimental | Fait tourner le NR de NVIDIA. |
| **Open** | Très expérimental | Une implémentation ouverte de NR ([OpenDLSS-NR](https://github.com/maanHimself/OpenDLSS-NR)). Attendez-vous à des plantages, du scintillement, des défauts d'image et des incompatibilités. Il a quand même besoin de `nvngx_dlssnr.dll` à côté du fichier du mod. |

Changer de moteur demande de relancer le jeu, et il n'y a pas de bascule automatique d'un moteur à l'autre. Avec
**Open**, le menu demande de choisir le backend adapté à votre carte et comment il s'exécute ; ses options y sont
décrites. Open ne fonctionne pas sur RTX 20. Son chemin RTX 30 et ses options de performance font partie du travail
Ampere de RTX Encore ; il n'est pas garanti de produire la même image que le moteur NVIDIA.

## Coût

NR ajoute du travail GPU à chaque image calculée. Mesuré dans le benchmark intégré de Black Myth: Wukong sur un
RTX 3070 Ti Laptop (2560×1440, DLSS à 58 %, génération d'images X3) :

| Configuration | FPS moyens | 5 % les plus bas |
| :--- | ---: | ---: |
| NR désactivé | 146 | 127 |
| NR activé, Precision Exact | 76 | 71 |
| NR activé, Precision Fast | 82 | 76 |
| NR activé, Precision Fast, Resolution Performance | 90 | 83 |

Il s'agit des **FPS affichés avec la génération d'images X3**, avec le moteur NR NVIDIA. Dans ce test, passer des
réglages par défaut à Precision Fast et Resolution Performance augmente les FPS affichés de 18 % (76 à 90 FPS).
Cela ne mesure pas une hausse de 17 % des FPS du jeu : ce chiffre concerne le temps de traitement NR. Avec la
génération d'images, NR s'applique aux images que le jeu calcule, jamais aux images générées.

Deux options expérimentales vont plus loin, dans le groupe **Performance** : **Avoid NR padding** et, pour les jeux
DirectX 12, **NR frames in flight**. Chacune est décrite dans le menu. Le gain le plus important est le mode
ultra-rapide ci-dessous.

## Mode ultra-rapide

**Ultra-fast mode** est le mode ultra-rapide développé par RTX Encore pour le moteur Open. Il répartit le
travail NR sur deux images calculées et maintient la couche NR alignée sur la scène entre les mises à jour. Il
réduit ainsi le travail NR par image avec un rendu temporel différent, au lieu de simplement réduire la résolution.

Mesuré dans le benchmark intégré de Black Myth: Wukong sur un **RTX 3070 Ti Laptop (Ampere)**, avec la génération
d'images X3 (**un passage par configuration**) :

| | Moteur Open | Moteur Open, ultra-rapide |
| :--- | ---: | ---: |
| Temps NR par image calculée | 18,7 ms | 9,8 ms |
| Fréquence d'images affichée | 73,8 FPS | 94,9 FPS |

Soit **48 % de temps NR en moins** et **29 % de FPS affichés en plus** dans cette comparaison. Le mode répartit le
travail pour éviter d'alterner une image avec tout le coût NR et une image sans travail NR. Le démarrage et les
réinitialisations d'historique peuvent encore coûter davantage ; ce résultat ne garantit ni un gain ni des temps
d'image réguliers dans tous les jeux. Ces mesures existantes ne sont pas le benchmark d'une nouvelle build de
publication ; cette page ne consigne pas d'identifiant exact de build.

- **L'activer** : onglet **Image**, moteur **Open**, puis **Ultra-fast mode** dans le groupe **Performance**.
  Relancez le jeu. Il fonctionne avec une passe NR, dans les jeux DirectX 11, DirectX 12 et Vulkan, et a été vu en
  jeu dans Black Myth: Wukong, Palworld et Shadows of Doubt.
- **Ce qui change dans l'image** : la couche NR suit l'image avec une image de retard. Dans les mouvements rapides,
  elle peut traîner ou baver, et une surface qui vient d'apparaître reçoit son NR un instant plus tard.
- **Deux réglages l'ajustent en direct** : **Ghost tolerance** (plus bas : moins de fantômes derrière ce qui bouge,
  plus de surface en attente de NR) et **Fill tolerance** (la facilité avec laquelle une surface qui vient
  d'apparaître emprunte le NR de surfaces semblables autour d'elle ; 0 désactive ce remplissage).
- Il utilise quelques Mo de VRAM en plus.

Ce mode appartient au moteur Open, qui reste très expérimental : voir [Moteurs](#moteurs).

## Mémoire du moteur Open

Nos travaux Ampere réduisent aussi l'empreinte mémoire d'Open. Des tests automatisés locaux existants du chemin
RTX 30, avec **More speed for more VRAM** désactivé, ont relevé ces allocations GPU propres au moteur NR :

| Taille de travail NR | Avant | Après |
| :--- | ---: | ---: |
| 1472×828 | 601 MiB | 345 MiB |
| 1920×1080 | 846 MiB | 501 MiB |

Soit environ **41–43 % de mémoire NR en moins**, avec une sortie identique dans les comparaisons de référence.
Ces chiffres comptent les allocations du moteur Open, pas la VRAM totale du jeu ; ce sont des résultats automatisés
locaux, pas une mesure de FPS en jeu. Ils ne décrivent pas la mémoire du moteur NVIDIA ni celle de la génération
d'images. Activer **More speed for more VRAM** ajoute de nouveau de la VRAM en échange de moins de temps NR.

## Quand quelque chose ne fonctionne pas

| Symptôme | Première vérification |
| :--- | :--- |
| L'interrupteur ne peut pas être activé | `nvngx_dlssnr.dll` manque à côté du fichier du mod, ou n'est pas en version 310.8.0. |
| La pastille dit attendre DLSS | Activez DLSS ou DLAA dans le jeu et chargez une scène 3D. |
| Les premières images n'ont pas NR | NR se prépare en arrière-plan ; c'est normal. |
| L'image reste telle que DLSS l'a produite | Lisez la raison affichée dans le menu : quand le jeu ne fournit pas ce dont NR a besoin, l'image est laissée intacte. |
| Les réglages reviennent aussitôt en arrière | Le fichier de réglages n'a pas pu être enregistré : vérifiez qu'il n'est ni en lecture seule ni verrouillé par un éditeur. |
| La fréquence d'images chute | NR coûte cher. Réduisez **Resolution**, et comparez dans la même scène. |

Quand vous signalez un problème, indiquez le jeu et son API graphique, le moteur choisi, et joignez le rapport du
menu (onglet **System**, **Copy**).
