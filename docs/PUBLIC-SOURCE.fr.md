# Ce que le code source public ne contient pas

*English: [PUBLIC-SOURCE.md](PUBLIC-SOURCE.md)*

Le dépôt public contient tout le code de ce fork : patchs d'architecture, réécriture Turing, transport Vulkan, contrôles, overlay, panneau ReShade, assistance UI, tests et documentation. Quelques **kernels GPU** en sont absents, parce que nous n'avons pas le droit de les publier. Le moteur se compile et fonctionne sans eux ; seule une partie de `"optimizedKernels"` est concernée.

## Ce qui manque

| Absent | Contenu | Pourquoi il n'est pas publié |
|---|---|---|
| `source/native/private/kernels/*.ptx` et `private/kernels.rc` | 54 kernels PTX des réseaux DL1 et DL2 (40xx pour sm_86, 110xx pour sm_75) | Repris du backend dlssg_for_sm86 0.3.5, qui ne les publie pas sous forme de source, et dérivés des kernels réseau de DLSS-G de NVIDIA |
| `source/native/private/image_kernels_ptx.h` | PTX des remplacements exacts de `Kernel_OutputPull` et `Kernel_OutputPushFine` | Dérivés des kernels DLSS-G 310.9.1 de NVIDIA, par le même backend |

Le code qui les utilise est public : `network_optimizer.h` (règles de lancement et fusions), `image_kernels.h` (identification par empreinte, vectorisation du Blend) et les hooks qui les substituent.

## Ce que fait une build publique sans eux

| Fonction | Build publique |
|---|---|
| X2–X6, modes dynamique et adaptatif, overlay, panneau, raccourcis | Inchangé |
| RTX 30 / RTX 20, Blackwell Transfusion | Inchangé (patchs du runtime NVIDIA lui-même, appliqués en mémoire) |
| Protection valid-warp, les deux politiques (`explained-warp` et `transfusion`) | Inchangé |
| Transport Vulkan (`VK_NVX_binary_import`) | Inchangé |
| Assistance UI | Inchangé |
| `optimizedKernels` : écritures vectorielles de `BlendCandidatesFused` | Actif (réécriture textuelle du kernel NVIDIA, sans contenu embarqué) |
| `optimizedKernels` : `OutputPull`, `OutputPushFine`, réseaux DL1/DL2 et fusions de lancements | **Inactif** : les kernels NVIDIA tournent tels quels. Même image, génération d'images plus lente (sur RTX 3070 Ti Laptop en 1080p, environ 2,3 ms au lieu de 1,6 ms en 2x) |

CMake le signale à la configuration (`private/ not present`), et le log l'indique une fois :

```text
Network optimization unavailable: kernel 4000 is not embedded in this build (docs/PUBLIC-SOURCE.md)
```

Les binaires publiés en release sont compilés avec tous ces éléments.

## Également absent

- **L'historique de développement.** L'historique privé contient les kernels ci-dessus et n'est donc pas publié : le dépôt public garde l'historique de [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) (MIT) et ajoute un commit par release.
- **Les binaires précompilés.** Le dépôt n'en contient aucun (l'upstream gardait un ancien `DLSSG-Transfusion.asi`) ; les binaires sont publiés en release.

## Éléments tiers conservés

- **Microsoft Detours** (`source/native/detours`, MIT).
- **De courts fragments du PTX de NVIDIA** servant de motifs de recherche aux patchs en mémoire (noms de registres, séquences d'instructions à trouver et réécrire), et les empreintes qui reconnaissent les kernels NVIDIA. Ils identifient le code de NVIDIA sans le reproduire. Les patchs hérités de l'upstream de Tony fonctionnent de la même façon.
- **La liste des jeux DLSS-G** (`nvidia_mfg_manifest.generated.h`) : noms et paliers issus du manifeste public de NVIDIA, comme dans l'upstream.

Streamline, le SDK NGX et ReShade ne sont pas inclus non plus ; la compilation prend leur chemin (`STREAMLINE_ROOT`, `RESHADE_ROOT`, voir la section *Build* du README).
