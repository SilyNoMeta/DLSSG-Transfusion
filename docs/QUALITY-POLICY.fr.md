# Politique valid-warp : `transfusion` ou `explained-warp`

*English: [QUALITY-POLICY.md](QUALITY-POLICY.md)*

Branche `feat/sm86-75-qualitywarp-v3`, rapatriée dans `feat/sm86-75`.

Le *valid warp* a été inventé par Tony Joaca pour Transfusion. C'est un patch PTX inséré dans `Kernel_BlendCandidatesFused` : quand les candidats déformés sont fiables, la sortie suit le warp plutôt que le mélange de NVIDIA. `dlssg_for_sm86` en a écrit sa propre réimplémentation, jusqu'à sa troisième itération, livrée dans la v0.3.5-5 sous le nom de « V3 ». Sa numérotation n'a aucun lien avec la V4 de Tony.

## L'idée derrière `explained-warp`

Entre deux images réelles, DLSS-G dispose de deux candidats pour chaque pixel généré : l'image précédente et la suivante, chacune déplacée le long des vecteurs de mouvement (le warp). NVIDIA les mélange ; le valid warp décide quand faire plutôt confiance au warp.

Le test de Tony vérifie que **les deux candidats déplacés sont d'accord**. C'est sûr, mais cela rejette aussi un mouvement correct dès que les deux candidats diffèrent légitimement : une ombre qui passe sur un sol éclairé, un détail fin qui crénèle différemment d'une image à l'autre.

`explained-warp` pose une autre question : **le warp explique-t-il ce qui a changé ?** Il compare les deux candidats deux fois, avant déplacement (`Es`) et après (`Em`). Si les images diffèrent beaucoup sans déplacement mais peu une fois déplacées, les vecteurs de mouvement rendent compte du changement : le warp complet est retenu, même si les candidats ne coïncident pas exactement. Si le déplacement ne réduit pas l'écart, le mélange de NVIDIA est conservé. Un candidat auquel NVIDIA donne déjà un poids notable est aussi suivi quand l'image change beaucoup.

C'est pourquoi elle garde à la fois le détail fin et des ombres en mouvement propres (mesures ci-dessous), là où chaque politique précédente sacrifiait l'un pour l'autre.

```json
"qualityValidWarp": true,         // interrupteur général (Tony), inchangé
"qualityPolicy": "explained-warp" // "explained-warp" (défaut) ou "transfusion" ; redémarrer pour appliquer
```

| | `transfusion` (Tony, `QUALITY_VALID_WARP_V4`) | `explained-warp` (dlssg_for_sm86 V3, défaut) |
|---|---|---|
| Critère | Les deux candidats déformés **s'accordent** : écart RGB ≤ 14 % de la luminance max | Le warp **explique le changement** entre les deux images sources : `Em + 0,08 < Es` et `Em < 0,15` |
| Cas forcé | — | Candidat valide, poids NVIDIA ≥ 0,20 et image qui change (`Es > 0,25`) |
| Effet | Poids du warp relevé à au moins 0,98 | Warp complet |
| Retour terrain | Ombres dédoublées vues en v0.3.5-3 ; la garde A qui les corrigeait (v0.3.5-4) réduisait le détail. Sans garde A : plaques éclairées dans les ombres en mouvement (voir plus bas) | Cyberpunk, X2 (v0.3.5-5) : détail de la 310.9.1-11 et ombre propre, avec quelques mouchetures au bout des membres rapides |

`Es` et `Em` sont les écarts RGB L1 entre les deux candidats, non déformés et déformés.

## Implémentation

- `quality_explained_warp.h` contient le bloc PTX produit par `tools/companion/quality_program.py` de dlssg_for_sm86 (`program_v3('blackwell')`), octet pour octet.
- Il s'insère au même point d'ancrage que la politique de Tony, avant `ld.param.u8 %rs8, [%rd6+220]`, et remplace celle-ci : les deux ne sont jamais combinées.
- La garde d'empreinte du PTX et les remplacements SFU exacts de `quality_fix::Patch` s'appliquent aux deux politiques.
- Le PTX Blackwell du Blend patché par Transfusion est **identique instruction par instruction** au 8039 de l'upstream 0.3.5, sur lequel V3 a été conçue : même disposition des registres.

## Validation (RTX 3070 Ti Laptop, runtime NVIDIA 310.9.1 d'origine)

| Test | Résultat |
|---|---|
| Compilation `ptxas`, deux politiques, en sm_86, sm_89 et sm_75 (après réécriture Turing) — `tests/quality_policy` | 6/6 |
| `explained-warp` contre la release dlssg_for_sm86 v0.3.5-5 (V3 en cubin), 720p en mouvement : X6, UIAlpha, HUDless seul | **20/20, 4/4, 4/4 images identiques bit à bit** (HUDless seul comparé à la release intégrée, qui applique elle aussi les patchs UIR) |
| `transfusion` contre la même release | Différent (0,4 à 0,7 % des pixels) : les deux politiques sont bien distinctes |
| `explained-warp` en Turing émulé (`DLSSG_TRANSFUSION_EMULATE_SM=75`) | 20/20 identiques au chemin Ampere |

## Comparaison en jeu (26 septembre 2026)

Cyberpunk 2077, RTX 3070 Ti Laptop, X2, SDR, enregistrement par la NVIDIA App (une image par présentation). Même sauvegarde et même parcours que les prises de dlssg_for_sm86 : grillage, course le long du grillage, route, puis une prise dédiée à l'ombre du personnage. Les builds ont été analysées **à l'aveugle** : la politique de chaque prise n'a été révélée qu'après les résultats.

Détail des images générées : netteté médiane (laplacien) d'une image générée divisée par celle des deux images réelles voisines, sur les images en mouvement, par phase (plus haut = plus de détail conservé). Deux prises par build.

| Build | Grillage | Route |
|---|---|---|
| NVIDIA seul (dlssg_for_sm86, `Optimized=0`) | 0,810 | 0,808 – 0,813 |
| dlssg_for_sm86 v0.3.5-4 (V4 de Tony + garde A) | 0,814 – 0,817 | 0,822 – 0,829 |
| **`transfusion`** (V4 de Tony, sans garde A) | 0,815 – 0,819 | 0,830 – 0,834 |
| **`explained-warp`** | 0,826 – 0,827 | 0,835 – 0,836 |
| dlssg_for_sm86 v0.3.5-5 (V3) | 0,827 – 0,832 | 0,833 – 0,838 |

Plaques éclairées dans les ombres en mouvement : pixels nettement plus clairs dans l'image générée que partout dans un voisinage de 9×9 px (demi-résolution) des deux images réelles voisines, là où celles-ci sont sombres. On regroupe ces pixels en blocs et on rapporte le nombre de blocs par image générée à la même mesure sur les images réelles (plus bas = ombre plus propre).

| Build | Rapport générée / réelle |
|---|---|
| NVIDIA seul | 0,26 |
| **`explained-warp`** | 0,39 |
| dlssg_for_sm86 v0.3.5-5 (V3) | 0,64 |
| dlssg_for_sm86 v0.3.5-4 (V4 + garde A) | 0,88 |
| **`transfusion`** | **2,1** |

Constats :
- **`transfusion` sans garde A remplit les ombres qui bougent de lambeaux de sol éclairé à bords nets** (jambe, tête), pires que les plaques de la 310.9.1-11. Retirer la garde A ne rend pas le détail pour autant : le grillage reste au niveau de la v0.3.5-4.
- **`explained-warp` retrouve les chiffres de la v0.3.5-5** sur le grillage et la route, ce qui est attendu puisque ses images sont identiques bit à bit hors jeu. Sur l'unique prise d'ombre, il donne 0,39 contre 0,64 et un détail de 0,887 contre 0,928 pour le même algorithme. Ces écarts donnent la variabilité d'une prise à l'autre : sur cette seule prise, un écart inférieur à environ 1,6× n'est pas significatif. L'écart de `transfusion` (×3 à ×5) l'est.
- **HUD** : même proportion de jaune conservée sur les marqueurs de quête en mouvement pour toutes les builds avec UIR (~96 %, contre 83 % pour la 310.9.1-11 sans UIR). Les marqueurs animés (anneau qui pulse, coche) se déchirent de la même façon avec les deux politiques.
- **Ville (voitures, piétons)** : rien de marquant avec l'une ou l'autre politique ; les deux prises ne filmaient pas la même scène.

**Conséquence : `explained-warp` devient la politique par défaut.** `transfusion` reste disponible.
