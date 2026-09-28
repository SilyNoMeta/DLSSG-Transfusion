# Transfusion sur RTX 30 (Ampere) et RTX 20 (Turing) — branches `feat/sm86` et `feat/sm86-75`

*English: [RTX30-SM86.md](RTX30-SM86.md)*

26 septembre 2026. Fork de [TonyJoaca/DLSSG-Transfusion](https://github.com/TonyJoaca/DLSSG-Transfusion) au commit `93ae5c1` (v1.4.5).

**Principe : les patchs de Tony sur le runtime NVIDIA restent le moteur.** Ils sont étendus pour viser l'architecture réellement présente. Ce portage n'embarque aucun code de sdli1995 ni binaire dérivé de NVIDIA ; seuls les kernels optimisés, ajoutés ensuite et facultatifs, en dérivent (voir [PUBLIC-SOURCE.fr.md](PUBLIC-SOURCE.fr.md)). Nos travaux sur `dlssg_for_sm86` (`port3109`, `sm86_free`, rétro-ingénierie) ont seulement servi à identifier ce qu'il faut patcher.

## Pourquoi le runtime NVIDIA refuse une RTX 30, et ce que Transfusion patchait déjà

Analyse de `nvngx_dlssg.dll` 310.9.1 (SHA256 `ff6e90eb…`) :

| Contenu | Nombre | Utilisable en SM86 tel quel |
|---|---:|---|
| Cubins ELF du réseau neuronal **sm_86** (livrés par NVIDIA) | 39 | **oui** |
| Cubins ELF du réseau sm_89 | 39 | non |
| Fatbins réseau, PTX sm_89 seul | 39 | non (PTX trop récent) |
| Fatbins kernels image : PTX sm_120 + PTX sm_89 + ELF sm_89 | 31 | non |

Les **101 PTX compilent tous pour sm_86** après simple changement de `.target`. Il n'y a ni FP8, ni `wgmma`, ni `cp.async.bulk`.

| Verrou | Transfusion d'origine (Ada) | Extension SM86 |
|---|---|---|
| Minimum d'architecture 400 dans `GetGPUArchitecture` et les trois `*_GetFeatureRequirements` | Absent, car Ada le passe | **Nouveau** : immédiat unique trouvé par nom d'export, 0x190 → 0x170. Mécanisme repris de `sm86_free`. Un hook est impossible : l'appelant est vérifié. |
| Seuils Blackwell `cmp …, 0x1b0` (max MFG publié) | `PatchDlssgArchGates` → 0x190 | Même scan, vers **0x170**, ou 0x160 pour Turing |
| Garde `ValidateMultiFrameCount` | `kNgxPatch` | Inchangé |
| Kernels image absents en SM86 | Retarget en place sm_120 → sm_89, images Ada parquées à 122 | Même retarget, vers **sm_86**, et étendu aux conteneurs PTX sm_89 seuls |
| Correctif midpoint et qualité V4 (`BlendCandidatesFused`) | Fatbin reconstruit en `.target sm_89` | Reconstruit en `.target sm_86`, entrée étiquetée 86 |

La directive `.target` apparaît en clair dans le flux LZ4 des 101 PTX. La réécriture de même longueur de Tony (`sm_120` → `sm_86 `, `sm_89` → `sm_86`) couvre donc tout sans décompression. Le pilote compile ensuite le PTX pour sm_86 (JIT via `NvAPI_D3D12_CreateCuModule`).

## Choix de l'architecture

`gpu_arch.cpp` lit les identifiants PCI (D3DKMT) au chargement. Les appels D3DKMT sont sûrs sous le verrou du chargeur. Sur Ada, Blackwell ou un adaptateur inconnu, la cible reste Ada : le comportement de Tony est inchangé. `DLSSG-Transfusion.json` reçoit une nouvelle clé :

```json
"gpuArchitecture": "auto"   // "auto", "ada", "ampere" ou "turing" ; redémarrer pour appliquer
```

La variable `DLSSG_TRANSFUSION_GPU_ARCH` permet de forcer la cible pour les tests. Elle n'est jamais sauvegardée. Sous Ada, le journal liste chaque module CUDA créé (`[CU-MODULE]`), avec son format et la réponse du pilote.

## Validation — RTX 3070 Ti Laptop (SM86), pilote 617.14, runtime NVIDIA d'origine

| Test | Résultat |
|---|---|
| Banc `port3109`, 1080p, X2, 5 images, `blackwellTransfusion=false` | **5/5 SHA256 identiques à la référence NVIDIA sur RTX 4090**. À X2, le correctif midpoint est neutre (t = 0,5). |
| Modules chargés par le runtime | 39 ELF sm_86 de NVIDIA et 25 fatbins PTX→sm_86. **Tous acceptés.** Les 2 rejets restants sont les appels de détection à vide, comme sur Ada. |
| X6 1080p, config par défaut : kernels Blackwell et qualité V4 de Tony | 25 images générées, tous modules acceptés |
| X6 1080p, kernels Ada et midpoint seul | 25 images générées |
| Cadence X6 : décalage mesuré de chaque image générée | 8·i/6 px à ±0,3 px près, sans compaction vers le milieu |
| Streamline 2.14.1 (DLL de jeu) avec le vrai cœur NGX du pilote | Sans Transfusion : `slIsFeatureSupported` = **6** (`NoSupportedAdapterFound`). Avec : **0**, `min_arch` 368. |
| Non-régression Ada (`gpuArchitecture=ada` forcé) | `.text` et `.rdata` du runtime patché **identiques octet par octet** au build `93ae5c1`. Écarts limités à `.data` (pointeurs alloués), comme entre deux lancements du build d'origine. |

### Pas encore validé

- **En jeu** : présentation réelle, pacing, hotkeys, overlay, UIR.
- **Vulkan** : les images des kernels passent par `vkCreateCuModuleNVX` ; le transport est sur la branche `feat/sm86-75-vulkan-nvx`, pas encore validé sur GPU. Voir [VULKAN.fr.md](VULKAN.fr.md).
- **Autres versions** du runtime (310.1 à 310.8) et copies OTA du cache NGX. Les patchs sont trouvés par signature, sans RVA, mais seule la 310.9.1 est mesurée.

## Turing (SM75) — branche `feat/sm86-75`

Sur Turing, trois choses manquent en plus. Chacune est traitée par un patch du même type.

**1. Le réseau.** NVIDIA ne livre pas de cubins sm_75. Le runtime choisit sa variante de réseau d'après la version SM que NGX lui transmet (`Context.GPU.SMVer`, ou NvAPI à défaut). Mesures faites en variant cette valeur :

| SM vu par le runtime | Réseau chargé |
|---|---|
| 8.9 | cubins ELF sm_89 |
| 9.0, 10.0, 12.0 | modules **PTX** |
| 8.6, 7.5, … | cubins ELF sm_86 |

Le sélecteur se trouve à deux endroits : `call [vtbl+0x40]` / `cmp eax, 0x59` / `jle`. `PatchDlssgNetworkSelector` retire ce `jle`, avec une signature unique et masquée, sur cible Turing uniquement. Tous les réseaux passent alors par le PTX, que le retarget en place ramène à `sm_75`.

**2. Les instructions sm_80.** Quatre formes bloquent `ptxas` en sm_75. `ptx_lowering.h` les réécrit en PTX sm_75 :

| Instruction sm_80 | Réécriture sm_75 |
|---|---|
| `mma.sync.m16n8k16.f16` | 2 × `m16n8k8` : les fragments A/B sont les deux moitiés de K |
| `cvt.rn.f16x2.f32 d, a, b` | 2 × `cvt.rn.f16.f32` + `mov.b32 d, {b, a}` |
| `max`/`min.f16` et `.f16x2` | via f32, exact |

Les blocs d'assembleur inline (`{ instr; }`) sont conservés. Sur les 101 PTX du runtime, 670 `mma`, 78 `cvt` et 294 `min`/`max` sont réécrits, et **les 101 compilent en sm_75** (`tests/ptx_lowering`).

**3. Le point d'application.** Le PTX compressé ne peut pas grandir en place. `cu_module_hook.h` intercepte donc `NvAPI_D3D12_CreateCuModule`. Si l'image contient une de ces formes, `midpoint_fix::PrepareModuleImage` la décompresse, la réécrit et la reconstruit en fatbin non compressé, puis la passe au pilote. Les images sont mises en cache par contenu et jamais libérées. Cela couvre aussi les fatbins reconstruits par Tony (Blend, Scatter).

Les seuils suivent : minimum d'architecture 0x190 → **0x160**, arch gates 0x1b0 → 0x160.

### Validation sans carte Turing

`DLSSG_TRANSFUSION_EMULATE_SM=75` est un **mode de test** (`sm_emulation.h`), jamais lu depuis la configuration. NvAPI rapporte alors SM 7.5 et l'architecture 0x160, et le banc passe `Context.GPU.SMVer=75`. La RTX 3070 Ti exécute le PTX sm_75. Tout le chemin Turing tourne ainsi réellement, sans cubin sm_86 ni sm_89 :

| Test (runtime NVIDIA 310.9.1 d'origine) | Résultat |
|---|---|
| Modules : 39 réseau PTX (27 réécrits) et 25 kernels image sm_75 | **tous acceptés** par le pilote |
| X2 1080p, kernels Ada | **5/5 images identiques bit à bit** à la référence NVIDIA, malgré la découpe des `mma` |
| X6 1080p, config par défaut (Blackwell + qualité V4) | 25 images, **25/25 identiques** au chemin Ampere, cadence 8·i/6 px à ±0,3 px |
| Non-régression | Ampere X2 : 5/5 identiques à NVIDIA. Ada forcé : `.text` et `.rdata` identiques à `93ae5c1`. Harnais de contrôle inchangé. |

### Limites

- **Aucune exécution sur une vraie carte Turing.** Le code machine sm_75 compilé par le pilote n'a tourné que sur SM86. Les performances (sans `m16n8k16`) et la VRAM des cartes RTX 20 restent inconnues.
- **Le sélecteur de réseau** n'est localisé que pour la 310.9.x. Ailleurs, le patch journalise `not found`, et Turing n'est alors pas supporté.
- La GTX 16 (TU116/TU117, sans tensor cores) est hors périmètre.
