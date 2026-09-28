# Kernels optimisés (bit-exacts)

*English: [OPTIMIZED-KERNELS.md](OPTIMIZED-KERNELS.md)*

Réglage : `"optimizedKernels": true` (défaut ; redémarrer pour appliquer). Ces optimisations viennent du backend dlssg_for_sm86 0.3.5 : le travail d'optimisation d'origine, perdu, est reconstitué depuis le PTX extrait. Chaque étape est livrée dans sa propre branche, rapatriée dans `feat/sm86-75`.

## Étape 1 — kernels image (branche `feat/sm86-75-opt-image`)

Les kernels image de DLSS-G 310.9.1 que Transfusion exécute sont les kernels Blackwell de NVIDIA (PTX sm_120, retargetés par Tony). Ce PTX est **identique instruction par instruction** aux ressources 8039, 8060 et 8063 de l'upstream 0.3.5, dont dérivent les versions optimisées :

| Kernel | Optimisation | Source |
|---|---|---|
| `Kernel_BlendCandidatesFused` | Dix paires d'écritures 32 bits contiguës fusionnées en `st.global.v2.u32`, appliquées **par-dessus** la politique valid-warp (mêmes registres) | règle de 9039 |
| `Kernel_OutputPull` | Masques d'inpainting compactés en bits par ligne en mémoire partagée, tests de voisinage par bits | PTX 9060 embarqué |
| `Kernel_OutputPushFine` | Test collectif du travail à faire, sortie anticipée des blocs sans contribution | PTX 9063 embarqué |

La substitution a lieu dans le hook `NvAPI_D3D12_CreateCuModule`, au même endroit que la réécriture Turing.
- **Identification** : un kernel est reconnu par son nom d'entrée et par l'**empreinte du PTX NVIDIA d'origine**, calculée hors ligne `.target` exclue. Sans correspondance, rien n'est modifié.
- **Blend** : il doit contenir les dix paires exactes.
- **Chaîne de compilation** : le PTX obtenu est retargeté vers le SM cible (86, 89 ou 75), puis réécrit en sm_75 sur Turing. 9060 contient 8 instructions sm_80, dont la réécriture Turing se charge. Il est ensuite compilé par le pilote.

Hors du chemin Blackwell (`blackwellTransfusion: false`), les empreintes ne correspondent pas et rien n'est substitué.

### Validation (RTX 3070 Ti Laptop, runtime NVIDIA 310.9.1)

| Test | Résultat |
|---|---|
| Sortie avec contre sans optimisation : X6 1080p en mouvement, politique `transfusion` | **20/20 identiques** |
| Idem, politique `explained-warp` | **20/20 identiques** |
| UIAlpha 720p | **8/8 identiques** |
| Turing émulé X6 | **20/20 identiques** ; 3 kernels optimisés en sm_75 ; tous les modules acceptés |
| Temps GPU 1080p X2 (médiane, 2 passes) | 2,33 → **2,23 ms** (−0,10 ms, −4 %) |
| Temps GPU 1080p X6 | 6,32 → **6,10 ms** (−0,23 ms, −3,6 %) |

Le gain augmente avec le multiplicateur, car OutputPull et OutputPushFine tournent pour chaque image générée. Sur Ada, le PTX est compilé en sm_89 : c'est correct, mais le gain n'y a pas été mesuré.

## Étape 2 — fusion du décodeur DL1 (branche `feat/sm86-75-opt-fusion`, remplacée par l'étape 3)

### Recette relevée avec l'oracle

Pour construire la fusion, j'ai d'abord **observé** la chaîne de lancements.

- **Outils** : deux outils de test inaccessibles depuis la configuration. `DLSSG_TRANSFUSION_CHAIN_DUMP` enregistre, au plus près de NvAPI, chaque module, chaque fonction et chaque entrée de `LaunchCuKernelChain`. `DLSSG_TRANSFUSION_OBSERVE_ONLY` coupe tous les patchs de Transfusion.
- **Montage** : Transfusion en observateur, sous le moteur sdli 0.3.5. Les adresses GPU sont identiques d'un lancement à l'autre, ce qui permet de comparer paramètre par paramètre la chaîne NVIDIA (89 lancements) et celle de sdli (70).

À chaque niveau du décodeur, NVIDIA fait trois lancements séparés :

```
k_upscale(low → tmp) ; k_element_wise(tmp + skip → x) ; k_conv_fp16_nhwc(W, x → out)
```

Ils sont remplacés par un seul :

```
k_conv_fp16_nhwc_fused_up(W, low, skip, out, lw, lh, hw, hh)
```

- **Poids** : ceux de NVIDIA pour les niveaux 1 à 4.
- **Grille** : ⌈hw·hh/16⌉, vérifiée en 720p, 1080p et 1440p.
- **Blocs** : ceux du `.maxntid` du kernel (256, 256, 256, 128).

Le niveau 5 utilise des poids réorganisés par sdli. Il est laissé intact.

### Implémentation (`decoder_fusion.h`)

- **Kernels** : 4024, 4026, 4028 et 4030 en sm_86, compilés à la volée en sm_89 sur Ada ; 11024 à 11030 en sm_75 pour Turing, sans `cp.async` ni `m16n8k16`.
- **Création** : sur le device du runtime, au moment où celui-ci crée son propre `k_upscale`. Les handles de fonctions recyclés prennent toujours leur dernier sens.
- **Mise en attente** : chaque lancement arrive dans son propre appel de chaîne. L'upscale et l'add sont donc mis en attente, avec copie de leurs paramètres, jusqu'à la conv qui les consomme.
- **Vérifications** : le chaînage des pointeurs (sortie de l'un = entrée du suivant) et la cohérence des dimensions.
- **Repli** : tout autre lancement, un changement de command list ou un contrôle en échec rejoue les lancements retenus à l'identique, dans l'ordre.

### Validation (RTX 3070 Ti Laptop, runtime NVIDIA 310.9.1)

| Test | Résultat |
|---|---|
| X6 1080p en mouvement : fusion contre étape 1 seule | **20/20 identiques** ; 4 lancements fusionnés par évaluation |
| Turing émulé, X6 720p : fusion contre étape 1 seule | **20/20 identiques** |
| Temps GPU 1080p X2 : aucune / étape 1 / étapes 1 et 2 | 2,33 / 2,25 / **2,11 ms** |
| Temps GPU 1080p X6 | 6,38 / 6,13 / **6,04 ms** |

Au total, les étapes 1 et 2 font gagner **−0,22 ms en X2 (−9,5 %)** et **−0,34 ms en X6**. La fusion est **active aussi sur Ada**, à valider sur une 4090 : sortie et temps, avec le même banc.

## Étape 3 — réseau DL1 complet (branche `feat/sm86-75-opt-network`)

La comparaison entre sdli niveau 0 et niveau 1, dans le même processus et donc avec des allocations identiques, montre ceci : **tout le réseau DL1 s'exécute avec les poids NVIDIA**. Les pointeurs qui différaient dans la comparaison NVIDIA / sdli étaient les mêmes poids, décalés par les allocations de sdli. Chaque convolution et chaque pooling garde donc **les paramètres NVIDIA à l'identique** ; seuls le kernel, la grille et les blocs changent.

`network_optimizer.h` remplace `decoder_fusion.h`. Il reproduit la chaîne DL1 de sdli lancement par lancement :

| Rang | Kernel | Grille | Bloc |
|---|---|---|---|
| conv 0 | 4000 | ⌈w/16⌉, ⌈h/4⌉, 1 | 128 |
| conv 1 | 4008 | ⌈wh/16⌉ | 128 |
| conv 2 / 3 | 4002 / 4004 | ⌈w/16⌉, ⌈h/4⌉, 4 | 128 |
| conv 4 | 4006 | ⌈w/16⌉, ⌈h/4⌉, 8 | 256 |
| conv 5 / 6 | 4010 / 4012 | ⌈wh/32⌉, 1, 8 | 512 / 128 |
| conv 8 | 4014 | ⌈wh/32⌉, 1, 2 | 512 |
| conv 10 / 12 | 4016 / 4018 | ⌈wh/16⌉ | 256 / 128 |
| conv 14 | 4020 | ⌈wh/32⌉ | 128 |
| conv 16 | 4022 | ⌈wh/16⌉ | 64 |
| décodeur N1 à N4 / N5 (upscale + add + conv) | 4024 à 4030 / 4032 | ⌈hw·hh/16⌉ / ⌈hw·hh/64⌉ | 256, 256, 256, 128 / 128 |
| pooling | 4039 | ⌈ow·oh·C/2048⌉ | 256 |

- **Formules** : relevées avec l'oracle et vérifiées en 720p, 1080p et 1440p ; `w×h` sont les dimensions de sortie lues dans les paramètres.
- **Kernels embarqués** : les PTX sont des ressources de la DLL (`private/kernels.rc`, `private/kernels/*.ptx`, environ 1,5 Mo ; absents du dépôt public, voir [PUBLIC-SOURCE.fr.md](PUBLIC-SOURCE.fr.md)). La famille 40xx sert en sm_86 (compilée à la volée en sm_89 sur Ada), la famille 110xx en sm_75.
- **Identification** : chaque lancement est reconnu par son rang dans le réseau et la taille de ses paramètres ; les fusions vérifient en plus le chaînage des pointeurs et les dimensions. À la première incohérence, le reste de ce passage réseau tourne tel quel et les lancements retenus sont rejoués dans l'ordre.
- **Garde** : actif seulement si un kernel du runtime a correspondu à l'empreinte exacte de la 310.9.1. Sur le chemin Blackwell par défaut, c'est OutputPull.

### Validation (RTX 3070 Ti Laptop)

| Test | Résultat |
|---|---|
| Chaîne DL1 enregistrée | **identique à celle de sdli** en noms, grilles et blocs |
| X6 1080p : DL1 optimisé contre étape 1 seule | **20/20 identiques** |
| Turing émulé X6 720p | **20/20 identiques** (18 kernels sm_75) |
| Temps GPU 1080p X2 : étape 1 / DL1 complet / sdli niveau 1 | 2,22 / **1,66** / 1,60 ms |
| Temps GPU 1080p X6 : étape 1 / DL1 complet / sdli niveau 1 | 6,13 / **5,57** / 5,20 ms |

Au total : **−0,68 ms en X2 (−29 %)** et **−0,81 ms en X6**, pour une référence sans optimisation à 2,33 et 6,38 ms.

## Étape 4 — réseau DL2 (branche `feat/sm86-75-opt-dl2`)

### Constats

- **DL2 tourne à chaque image générée** : en X6, la chaîne contient 5 passages DL2 par image réelle, contre 1 seul passage DL1. C'est pourquoi l'écart restant avec sdli après l'étape 3 grandissait avec le multiplicateur (0,06 ms en X2, 0,37 ms en X6).
- **Les poids sont ceux de NVIDIA.** Les pointeurs de poids de sdli diffèrent de ceux de NVIDIA d'un décalage constant (par exemple +0x5000 sur toute la chaîne de résidus du block1), dû à ses propres allocations. La sortie bit-exacte obtenue avec les pointeurs NVIDIA le confirme. La note « buffers de poids propres » de l'étape 3 était fausse.
- **Identification des variantes.** sdli charge des ELF précompilés, pas du PTX. Chaque module ELF de l'oracle a donc été associé à sa ressource PTX en compilant les candidats avec `ptxas -arch=sm_86`. Les tailles concordent à un décalage constant près (+0,5 à +1,3 Ko), et le nom d'entrée départage les tailles identiques.
- **Paramètres.** Les kernels `conv_dl2_*` sont spécialisés par forme : canaux et taille de noyau sont figés dans le code, seules les dimensions spatiales sont lues. Leurs paramètres ont **exactement la disposition de ceux de NVIDIA** (80, 92, 144 et 152 octets). Seules les deux fusions ont leur propre structure.

### Chaîne DL2 reproduite

| Couche NVIDIA | Remplacement | Grille | Bloc |
|---|---|---|---|
| `k_initial_merge` + `custom_block0_convPre` | 4042 `conv_dl2_merge_pool` (fusion) | ⌈w/8⌉, ⌈h/4⌉ | 256 |
| block0 `conv0_c8` / `conv1` | 4044 / 4046 `conv_dl2_pool` | ⌈w/8⌉, ⌈h/2⌉ | 128 / 256 |
| block0 `conv2` ×8 | 4050 `conv_dl2_resid` | ⌈w/16⌉, ⌈h/2⌉, 2 | 128 |
| block0 `conv_bot0_hf` | inchangé (NVIDIA) | | |
| block0 `conv_bot1_hf` | 4062 `conv_dl2_bot1_block0` | ⌈w/16⌉, ⌈h/4⌉ | 128 |
| `custom_upsample_hf` | 4059 `upsample_hf` | ⌈wh/256⌉, 3 | 256 |
| `k_central_block` + block1 `conv0` | 4060 `conv_dl2_central_pool` (fusion) | ⌈w/8⌉, ⌈h/4⌉ | 256 |
| block1 `conv1` | 4048 `conv_dl2_pool` | ⌈w/8⌉, ⌈h/2⌉ | 128 |
| block1 `conv2` ×8 | inchangé (NVIDIA), voir plus bas | | |
| block1 `conv_bot0_hf` | inchangé (NVIDIA) | | |
| block1 `conv_bot1_hf` | 4058 `conv_dl2_bot1_block1` | ⌈w/16⌉, ⌈h/8⌉ | 256 |

- **Dimensions** : `w×h` sont les dimensions de sortie lues dans les paramètres NVIDIA. Les formules sont vérifiées sur les trois chaînes sdli enregistrées, sans aucun écart.
- **Paramètres des fusions** :
  - `merge_pool` (68 octets) : poids et biais de convPre, les 3 entrées du merge, la sortie de convPre, puis h, w d'entrée, h, w de sortie et le facteur du merge.
  - `central_pool` (92 octets) : poids et biais de conv0, les 6 entrées du bloc central, la sortie de conv0, puis h, w d'entrée, h, w de sortie et le facteur.
  - Dans les deux cas, la sortie du producteur n'est pas écrite. Elle est réécrite par la couche suivante avant toute lecture, comme chez sdli.
- **Garde-fous** :
  - **Identification** : chaque couche est reconnue par le nom de son kernel NVIDIA, puis vérifiée contre sa forme exacte (taille des paramètres, canaux, noyau 3×3).
  - **Fusions** : elles vérifient aussi le chaînage (sortie du producteur = entrée de la conv) et les dimensions.
  - **Repli** : en cas d'écart, le producteur retenu est rejoué tel quel et la couche tourne avec le kernel NVIDIA.
- **Non repris** :
  - **`conv_dl2_reschain`** (les 8 `conv2` du block1 en un seul lancement). C'est un kernel persistant avec une barrière de grille. Il repose sur un compteur atomique que sdli alloue lui-même, et sa grille est plafonnée (136 à 1080p, 192 au lieu de 230 en 1280) pour que tous les blocs soient résidents. Une grille trop grande bloquerait le GPU. Le gain mesuré par sdli pour cette fusion (bit 16) est dans le bruit.
  - Les deux `bot0` : sdli les laisse aussi à NVIDIA.

### Validation (RTX 3070 Ti Laptop)

| Test | Résultat |
|---|---|
| X6 1080p en mouvement, DL1+DL2 contre étape 1 seule | **20/20 identiques** |
| Turing émulé X6 720p (27 kernels sm_75) | **20/20 identiques** |
| 1440p X2, contre la même build sans optimisation | **4/4 identiques** |
| Temps GPU 1080p X2 : DL1 seul / DL1+DL2 / sdli niveau 1 | 1,65 / **1,57–1,58** / 1,53–1,60 ms |
| Temps GPU 1080p X6 : DL1 seul / DL1+DL2 / sdli niveau 1 | 5,59 / **5,22–5,24** / 5,20–5,21 ms |

Les passes sont entrelacées dans le même banc (DL1, DL2, sdli, sdli, DL2, DL1), puis trois passes X2 supplémentaires DL2 / sdli.

## Bilan (1080p, RTX 3070 Ti Laptop)

| | X2 | X6 |
|---|---|---|
| Sans optimisation | 2,33 ms | 6,38 ms |
| Étape 1, kernels image | 2,25 ms | 6,13 ms |
| Étapes 1 à 3, DL1 complet | 1,66 ms | 5,59 ms |
| **Étapes 1 à 4, DL1 + DL2** | **1,57 ms (−33 %)** | **5,23 ms (−18 %)** |
| sdli niveau 1 (référence) | 1,53–1,60 ms | 5,20 ms |

**Le niveau 1 de sdli est rattrapé**, à l'écart de mesure près, avec une sortie bit-identique au runtime NVIDIA. Tout reste sous le seul réglage `optimizedKernels`, actif aussi sur Ada. Il reste à valider sur une 4090 : sortie et temps, avec le même banc.

## Pistes restantes

- **`conv_dl2_reschain`** : à reprendre seulement avec un tampon compteur alloué côté Transfusion et une grille calculée d'après l'occupation réelle. Le gain attendu est au mieux de quelques centièmes de ms.
- **Kernels image** : sdli précompile ses 26 kernels image en ELF sm_86. Transfusion fait compiler le PTX par le pilote, ce qui donne a priori le même code ; aucun écart n'a été mesuré.
